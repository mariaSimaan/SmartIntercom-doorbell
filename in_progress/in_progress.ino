#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Keypad.h>
#include <Arduino.h>
#include <driver/i2s.h>
#include <math.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include <Base64.h>
#include <HTTPClient.h>
#include "freertos/FreeRTOS.h"  // Ensure FreeRTOS.h is included
#include "freertos/task.h"
#include "firebase_related_functions.h"

void LittleFSInit() {
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS initialization failed!");
    while (1) yield();
  } else {
    Serial.println("LittleFS initialized successfully.");
  }

  Serial.println("Listing LittleFS files:");
  File root = LittleFS.open("/");
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    Serial.print("FILE: ");
    Serial.print(file.name());
    Serial.print("  SIZE: ");
    Serial.println(file.size());
    file = root.openNextFile();
  }
}


void i2sInit() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = I2S_SAMPLE_RATE,
    .bits_per_sample = i2s_bits_per_sample_t(I2S_SAMPLE_BITS),
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = 0,
    .dma_buf_count = 64,
    .dma_buf_len = 512,
    .use_apll = 1
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);

  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = -1,
    .data_in_num = I2S_SD
  };

  i2s_set_pin(I2S_PORT, &pin_config);
}

void i2s_adc_data_scale(uint8_t * d_buff, uint8_t* s_buff, uint32_t len) {
  uint32_t j = 0;
  uint32_t dac_value = 0;
  for (int i = 0; i < len; i += 2) {
    dac_value = ((((uint16_t) (s_buff[i + 1] & 0xf) << 8) | ((s_buff[i + 0]))));
    d_buff[j++] = 0;
    d_buff[j++] = dac_value * 256 / 2048;
  }
}

#define DEBUG true

void i2s_adc(void *arg) {
  int i2s_read_len = I2S_READ_LEN;
  int flash_wr_size = 0;
  size_t bytes_read;

  char* i2s_read_buff = (char*) calloc(i2s_read_len, sizeof(char));
  uint8_t* flash_write_buff = (uint8_t*) calloc(i2s_read_len, sizeof(char));

  if (!i2s_read_buff || !flash_write_buff) {
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.println("Failed to allocate memory for buffers");
    if (i2s_read_buff) free(i2s_read_buff);
    if (flash_write_buff) free(flash_write_buff);
    vTaskDelete(NULL);
    return;
  }

  if (DEBUG) {
    Serial.printf("Total LittleFS space: %d, Used LittleFS space: %d\n", LittleFS.totalBytes(), LittleFS.usedBytes());
  }

  i2s_read(I2S_PORT, (void*) i2s_read_buff, i2s_read_len, &bytes_read, portMAX_DELAY);
  i2s_read(I2S_PORT, (void*) i2s_read_buff, i2s_read_len, &bytes_read, portMAX_DELAY);

  Serial.println(" *** Recording Start *** ");
  while (flash_wr_size < FLASH_RECORD_SIZE) {
    i2s_read(I2S_PORT, (void*) i2s_read_buff, i2s_read_len, &bytes_read, portMAX_DELAY);
    if (bytes_read > 0) {
      Serial.printf("Bytes read from I2S: %d\n", bytes_read);
      i2s_adc_data_scale(flash_write_buff, (uint8_t*)i2s_read_buff, i2s_read_len);

      if (DEBUG) {
        // Print a few bytes of the flash_write_buff to check data validity
        Serial.print("flash_write_buff data: ");
        for (int i = 0; i < min(10, i2s_read_len); i++) {
          Serial.printf("%02X ", flash_write_buff[i]);
        }
        Serial.println();
      }

      size_t bytes_written = file.write((const byte*) flash_write_buff, i2s_read_len);
      if (bytes_written != i2s_read_len) {
        Serial.printf("///////////////////////////Error writing to file: expected %d, wrote %d\n", i2s_read_len, bytes_written);
      } else {
        Serial.printf("Successfully written %d bytes to file\n", bytes_written);
      }
      file.flush(); // Ensure data is written to the file

      flash_wr_size += bytes_written;
      Serial.printf("Sound recording %u%%\n", flash_wr_size * 100 / FLASH_RECORD_SIZE);
      Serial.printf("Never Used Stack Size: %u\n", uxTaskGetStackHighWaterMark(NULL));
    } else {
      Serial.println("I2S read error");
    }
    vTaskDelay(1);  // Reset watchdog timer
  }

  file.close();

  free(i2s_read_buff);
  free(flash_write_buff);

  Serial.println(" *** Recording Finished *** ");
  listLittleFS();

  // Uninstall I2S driver for recording
  i2s_driver_uninstall(I2S_PORT);

  recordingTaskHandle = NULL;  // Set the task handle to NULL
  vTaskDelete(NULL);
}

void listLittleFS(void) {
  Serial.println(F("\r\nListing LittleFS files:"));
  static const char line[] PROGMEM =  "=================================================";

  Serial.println(FPSTR(line));
  Serial.println(F("  File name                              Size"));
  Serial.println(FPSTR(line));

  fs::File root = LittleFS.open("/");
  if (!root) {
    Serial.println(F("Failed to open directory"));
    return;
  }
  if (!root.isDirectory()) {
    Serial.println(F("Not a directory"));
    return;
  }

  fs::File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("DIR : ");
      String fileName = file.name();
      Serial.print(fileName);
    } else {
      String fileName = file.name();
      Serial.print("  " + fileName);
      int spaces = 33 - fileName.length(); // Tabulate nicely
      if (spaces < 1) spaces = 1;
      while (spaces--) Serial.print(" ");
      String fileSize = (String) file.size();
      spaces = 10 - fileSize.length(); // Tabulate nicely
      if (spaces < 1) spaces = 1;
      while (spaces--) Serial.print(" ");
      Serial.println(fileSize + " bytes");
    }
    file = root.openNextFile();
  }
  Serial.println(FPSTR(line));
  Serial.println();
  delay(1000);
}

void handleRoot() {
  server.send(200, "text/plain", "ESP32 LittleFS Web Server");
}

void handleDownload() {
  if (LittleFS.exists(filename)) {
    File downloadFile = LittleFS.open(filename, FILE_READ);
    server.streamFile(downloadFile, "audio/wav");
    downloadFile.close();
  } else {
    server.send(404, "text/plain", "File not found");
  }
}

void connectToWiFi() {
  WiFi.begin();

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    Serial.println("Connecting to WiFi...");
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 10);
    display.println("Trying to connect");
    display.println("to WiFi...");
    display.display();

    delay(500);
    retries++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Starting WiFiManager AP mode...");
    wifiManager.autoConnect("AutoConnectAP");

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
    }
  }

  Serial.println("Connected to WiFi");
  displayMessage("Press * to start", 0, 1);

  server.on("/", handleRoot);
  server.on("/download", handleDownload);
  server.begin();
  Serial.println("HTTP server started");
}

void disableDAC() {
  esp_err_t err = i2s_driver_uninstall(I2S_PORT);
  if (err != ESP_OK) {
    Serial.printf("Failed to uninstall I2S driver: %d\n", err);
  } else {
    Serial.println("DAC disabled successfully.");
  }
}

void setupDAC() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX), // Master transmitter
    .sample_rate = I2S_SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S_MSB,
    .intr_alloc_flags = 0, // Interrupt level 1
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false  // Set to true if using APLL (ESP32-S2 specific)
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = DAC_BCK_PIN,   // BCK pin
    .ws_io_num = DAC_WS_PIN,    // LRCK pin
    .data_out_num = DAC_DATA_PIN, // DATA pin
    .data_in_num = I2S_PIN_NO_CHANGE // Not used
  };

  esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("Failed to install I2S driver: %d\n", err);
    return;
  }
  err = i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("Failed to set I2S pins: %d\n", err);
    return;
  }
  Serial.println("DAC setup complete.");
}

void startRecording() {
   inputSequence = "";
  if (recordingTaskHandle != NULL) {
    Serial.println("Recording task is already running, cannot start another recording.");
    return;
  }

  if (LittleFS.exists(filename)) {
    LittleFS.remove(filename);
  }

  file = LittleFS.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println("File is not available!");
    return;
  }

  byte header[headerSize];
  wavHeader(header, FLASH_RECORD_SIZE);
  file.write(header, headerSize);

  // Ensure previous I2S instance is stopped and uninstalled
  i2s_driver_uninstall(I2S_PORT);

  // Reinitialize I2S for recording
  i2sInit();

  displayMessage("Recording", 0, 2);

  BaseType_t xReturned = xTaskCreatePinnedToCore(i2s_adc, "i2s_adc", 4096, NULL, 1, &recordingTaskHandle, 1);
  if (xReturned != pdPASS) {
    Serial.println("Failed to create recording task");
    recordingTaskHandle = NULL;
    file.close();
  }
}

void playRecording(const char* filename) {
    Serial.println("Starting playRecording function.");

    int i2s_read_len = I2S_READ_LEN / 2;
    disableDAC();  // Uninstall I2S driver before playback
    Serial.println("DAC disabled.");

    setupDAC();  // Reinstall I2S driver
    Serial.println("DAC setup complete.");

    if (!LittleFS.exists(filename)) {
        Serial.println("Recorded file not found!");
        return;
    }
    Serial.println("Recorded file exists.");

    File playbackFile = LittleFS.open(filename, FILE_READ);
    if (!playbackFile) {
        Serial.println("Failed to open recorded file!");
        return;
    }
    Serial.println("Recorded file opened successfully.");

    playbackFile.seek(headerSize, SeekSet);
    Serial.println("Skipped file header.");

    size_t bytesRead;
    uint8_t *dataBuffer = (uint8_t *)malloc(i2s_read_len);
    if (!dataBuffer) {
        Serial.println("Failed to allocate memory for playback buffer!");
        playbackFile.close();
        return;
    }
    Serial.println("Memory allocated for playback buffer.");

    Serial.println(" *** Playback Start *** ");
    while ((bytesRead = playbackFile.read(dataBuffer, i2s_read_len)) > 0) {
        Serial.printf("Read %d bytes from file.\n", bytesRead);

        size_t bytesWritten;
        esp_err_t err = i2s_write(I2S_PORT, dataBuffer, bytesRead, &bytesWritten, portMAX_DELAY);
        if (err != ESP_OK) {
            Serial.printf("Failed to write to I2S: %d\n", err);
            break;
        }
        Serial.printf("Wrote %d bytes to I2S.\n", bytesWritten);

        if (bytesWritten != bytesRead) {
            Serial.printf("Bytes written (%d) do not match bytes read (%d)\n", bytesWritten, bytesRead);
        }

        // Add a delay to prevent the CPU from being overburdened
        delay(10);
    }

    free(dataBuffer);  // Ensure this is not freeing prematurely
    Serial.println("Freed playback buffer memory.");
    
    playbackFile.close();
    Serial.println("Closed playback file.");

    Serial.println(" *** Playback Finished *** ");

    disableDAC();  // Uninstall I2S driver after playback
    Serial.println("DAC disabled after playback.");
}

void setup() {
  Wire.begin(18, 23);
  Serial.begin(115200);
  customKeypad.begin();
  listLittleFS();
  Serial.println(" in setup ");

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  LittleFSInit();
  listLittleFS();
  i2sInit();
  i2sMutex = xSemaphoreCreateMutex();

  connectToWiFi();

  Firebase.printf("Firebase Client v%s\n", FIREBASE_CLIENT_VERSION);

  ssl.setInsecure();

  // Initialize the authentication handler.
  initializeApp(client, app, getAuth(dbSecret));

  // Binding the authentication handler with your Database class object.
  app.getApp<RealtimeDatabase>(Database);

  // Set your database URL
  Database.url(DATABASE_URL);
  // In sync functions, we have to set the operating result for the client that works with the function.
  client.setAsyncResult(result);

  state = INITIAL_STATE;
  inputSequence = "";	
}

bool check_password() {
  // Compare the input sequence with the default password
  if (inputSequence == defult_password) 
  {
      state = OPEN_DOOR;
      return true;
  }

  if (state == WAITING_FOR_PASSWORD)
  {
    if (inputSequence == current_password) {
      state = OPEN_DOOR;
      return true;
      }
    // Display the current and entered passwords
    displayMessage(("Please enter the     following Code: " + String(current_password) + 
                    " Entered Code: " + String(enteredPassword) + 
                    "   press # to delete").c_str(), 0, 1);

    // Check if the entered password matches the current password


    // Check if the input sequence contains any underscores (incomplete entry)
    for (int i = 0; i < enteredPassword.length(); i++) {
      if (enteredPassword.charAt(i) == '_') {
        return false;
      }
    }
    
    // if we reach this code, then there is a full entered password and it is wrong
    if (password_counter > 0) {
      displayMessage("The password that youhave entered is wrongPlease try again.    You have one more try", 6 * 1000, 1);
      inputSequence = "";
      enteredPassword = "____";
      // Prompt user to re-enter the password
      displayMessage(("Please enter the     following Code: " + String(current_password) + 
                      " Entered Code: " + String(enteredPassword) + 
                      "   press # to delete").c_str(), 0, 1);
      password_counter--;
    } else {
      // If no tries are left
      state = WRONG_PASSWORD ; 
      return false; 
    }

  }
  
  return false;
}

void handlePressedKey(char key)
{
    if (key == '\0')
      return;
    Serial.print(key);
    Serial.println(" pressed");
    Serial.println(" state is");
    Serial.println(state);

    display.clearDisplay();
    display.setCursor(0, 10);
    display.setTextSize(1);
    display.println("Entered password is:");
    display.setTextSize(2);
    display.setCursor(0, 30);

    if (key == '#')
    {
        // Remove the last character or replace it with '_'
        if (inputSequence.length() > 0)
        {
        inputSequence = inputSequence.substring(0, inputSequence.length() - 1);
        }
    }
    else 
    {
        inputSequence += key;
    }
    
    // Limit the inputSequence to the last 4 characters
    if (inputSequence.length() > 4) {
        inputSequence = inputSequence.substring(inputSequence.length() - 4);
    }

    // Create a 4-character display string with underscores for padding
    enteredPassword = inputSequence;
    while (enteredPassword.length() < 4) {
        enteredPassword += '_';
    }

    // Display the entered password with padding
    Serial.println("enteredPassword is ");
    Serial.println(enteredPassword);

    Serial.println(" inputSequence is ");
    Serial.println(inputSequence);

    check_password();
    return;
}

String lastPassword;
void checkForPasswordUpdate() {
    String newPassword = Database.get<String>(client, "/default_password");

    if (newPassword != defult_password) {
        lastPassword = newPassword;
        Serial.println("Password updated: " + lastPassword);
        // Here you can add your logic to handle the new password
        defult_password = lastPassword; // Update the default password variable
    }
}

unsigned long lastCheck = 0;
unsigned long lastPasswordCheck = 0;
const unsigned long checkInterval = 5000; // Check every 5 seconds
unsigned long passwordCheckInterval = 10 *1000;  // For example, check every 10 seconds
const unsigned long debounceDelay = 200; // 200 milliseconds debounce delay
unsigned long lastKeyPressTime = 0;

void loop() {
    unsigned long startTime = 0;  // Declare outside the switch
    char key = '\0';
    // check wifi connection
    if (WiFi.status() != WL_CONNECTED && (millis() - lastCheck >= checkInterval)) {
        Serial.println("Lost WiFi connection. Reconnecting...");
        lastCheck = millis();
        WiFi.disconnect();
        connectToWiFi();
    }
    
    // check if default password has changed
    if (millis() - lastPasswordCheck >= passwordCheckInterval) {
        lastPasswordCheck = millis();
        checkForPasswordUpdate(); // Polling for password updates
    }

    customKeypad.tick();
    if (customKeypad.available()) {
        keypadEvent e = customKeypad.read();
        customKeypad.clear();
        key = (char)e.bit.KEY;

        // Debounce: Only process the key if enough time has passed since the last press
        if (millis() - lastKeyPressTime > debounceDelay) {
            Serial.println(" in read key ");
            handlePressedKey(key);
            lastKeyPressTime = millis(); // Update the last key press time
        }
    }

    switch (state) {
      case INITIAL_STATE:
          displayMessage("Press * to start", 0, 1);
          if (key == '*') {
              state = PLAY_START_MESSAGE;
              password_counter = 1;
              passwordTimestamp = 0;
              password_duration = 0;
              current_password = "";
              key = '\0';
          }
          break;

      case PLAY_START_MESSAGE:
          displayMessage("Playing Start Message", 0, 1);
          playRecording("/start_message.wav");
          display.clearDisplay();
          display.setTextSize(2);
          startTime = millis();  // Initialize startTime here
          while (millis() - startTime < 5000) {  // Non-blocking delay loop
              int secondsLeft = 5 - (millis() - startTime) / 1000;
              display.clearDisplay();
              display.setCursor(0, 20);
              display.print("Recording in ");
              display.print(secondsLeft);
              display.display();
              delay(100);  // Small delay to avoid overwhelming the display
          }
          state = RECORDING_MESSAGE;
          break;

      case RECORDING_MESSAGE:
          Serial.println("Starting recording...");
          xSemaphoreTake(i2sMutex, portMAX_DELAY);
          startRecording();
          xSemaphoreGive(i2sMutex);
          delay(6 * 1000);
          state = POST_RECORDING;
          break;

      case POST_RECORDING: 
          showPostRecordingOptions();
          if (key == '1') {
              state = PLAYING_MESSAGE; 
              play_message = true;
              key = '\0';
          } else if (key == '2') {
              state = SENDING_MESSAGE;
              key = '\0';
          } else if (key == '*') {
              state = RECORDING_MESSAGE;
              key = '\0';
          }
          break;

      case PLAYING_MESSAGE:
          Serial.println(" in PLAYING_MESSAGE ");
          if (play_message == true)
            displayMessage("Playing Message", 0, 1);
          play_message = false;
          playRecording(filename);
          state = POST_RECORDING;
          break;

      case SENDING_MESSAGE:
          chunkIndex = 0;
          state = TRYING_TO_READ_PASSWORD;
          displayMessage("Sending message ...", 0, 1);
          sendWaveToFirebase();
          break;

      case TRYING_TO_READ_PASSWORD:
          if (waiting_to_read_password()) {
              enteredPassword = "____";
              passwordTimestamp = millis();
              inputSequence = "";    
              state = WAITING_FOR_PASSWORD;
          } else {
              inputSequence = "";
              state = INITIAL_STATE;
          }
          break;

      case WAITING_FOR_PASSWORD:
          if (isDeny)
          {
            state = ENTRY_DENIED;
            break;
          }
           
          if (isPasswordExpired()) {
              state = PASSWORD_EXPIRED;
          } else {
              check_password();  // Assumes this function updates the state accordingly
          }
          break;

      case OPEN_DOOR:
          displayOpenedMessage("DOOR OPENED WELCOME!!!", 0);
          playRecording("/welcome_message.wav");
          inputSequence = "";
          state = INITIAL_STATE;
          break;

      case PASSWORD_EXPIRED:
          displayMessage("The Code Has Expired.Please try again.", 10 * 1000, 1);
          state = INITIAL_STATE;
          resetPasswordState();
          break;

      case WRONG_PASSWORD:
          resetPasswordState();
          displayMessage("The password you haveentered is incorrect Unfortunately, we    can not let you in.", 10 * 1000, 1);
          state = INITIAL_STATE;
          break;
        
      case ENTRY_DENIED:
        resetPasswordState();
        displayMessage("Sorry, the owner     denied your entry", 15 * 1000, 1);
        state = INITIAL_STATE;
        break;

      case RETRY_SENDING_MESSAGE:
        displayMessage("Sending message ...", 0, 1);
        sendWaveToFirebase();
        state = TRYING_TO_READ_PASSWORD;
        break;
  
      default:
          state = INITIAL_STATE;
          break;
        
    }

  server.handleClient();
  delay(5);
}

void resetPasswordState() {
    inputSequence = "";
    enteredPassword = "____";
    password_counter = 1;
    password_duration = 0;
    current_password = "";
    isDeny = false;
}
