/*********
 in the test, why test keypad oled and dac, we see that when we press any of the buttens the key on the OLED changes
 plus when we press 1 a sound starts to play using the dac , and when we press 0 the sound stops
*********/


#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Keypad.h>
#include <Arduino.h>
#include <driver/i2s.h>
#include <math.h>

// OLED display width and height, in pixels
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// define your specific keypad here via PID
#define KEYPAD_PID3845

// define your pins here
#define R4   19 
#define R3   13 
#define R2   12 
#define R1   4    
#define C1   21 
#define C2   27 
#define C3   33  

// Keymap for 3x4 Keypad
char keys[4][3] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};

// Row and column pins for 3x4 Keypad
byte rowPins[4] = { R1, R2, R3, R4 };
byte colPins[3] = { C1, C2, C3 };

// Initialize an instance of class Adafruit_Keypad
Adafruit_Keypad customKeypad = Adafruit_Keypad(makeKeymap(keys), rowPins, colPins, 4, 3);

// I2S configuration constants
#define I2S_NUM         (0) // I2S port number
#define I2S_SAMPLE_RATE (44100)
#define I2S_BITS        (16)
#define I2S_CHANNELS    (2)

bool dacInUse = false;

// Function to generate a sine wave audio signal
void generateSineWave(int16_t* buffer, int bufferSize) {
    const float frequency = 440.0;  // Frequency of the sine wave (440 Hz for A4)
    const float amplitude = 2000;  // Amplitude of the sine wave - change it to change the volume 

    for (int i = 0; i < bufferSize; i += 2) {
        float sample = amplitude * sin(2 * M_PI * frequency * (i / 2) / I2S_SAMPLE_RATE);
        buffer[i] = buffer[i + 1] = (int16_t) sample;
    }
}

void setupAudio() {
    // Initialize I2S configuration
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX), // Master transmitter
        .sample_rate = I2S_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // Interrupt level 1
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false  // Set to true if using APLL (ESP32-S2 specific)
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = 26,   // BCK pin
        .ws_io_num = 25,    // LRCK pin
        .data_out_num = 22, // DATA pin
        .data_in_num = I2S_PIN_NO_CHANGE // Not used
    };

    // Install and start I2S driver
    i2s_driver_install((i2s_port_t)I2S_NUM, &i2s_config, 0, NULL);
    i2s_set_pin((i2s_port_t)I2S_NUM, &pin_config);
}

void playNote() {
    // Prepare a buffer to store audio data
    const int bufferSize = 1024;
    int16_t buffer[bufferSize];

    // Generate a sine wave
    generateSineWave(buffer, bufferSize);

    // Write the buffer to I2S
    size_t bytes_written = 0;
    i2s_write((i2s_port_t)I2S_NUM, buffer, bufferSize * sizeof(int16_t), &bytes_written, portMAX_DELAY);
}

void stopNote() {
    // Stop I2S driver
    i2s_driver_uninstall((i2s_port_t)I2S_NUM);
}

void setup() {
  Wire.begin(18,23);
  Serial.begin(115200);  // Adjusted for ESP32
  customKeypad.begin();

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  delay(2000);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 10);
  // Display static text
  display.println("Press a key:");
  display.display();
}

void loop() {
  customKeypad.tick();
  while(customKeypad.available()) {
    keypadEvent e = customKeypad.read();
    char key = (char)e.bit.KEY;

    if (e.bit.EVENT == KEY_JUST_PRESSED) {
      Serial.print(key);
      Serial.println(" pressed");
      // Update the OLED display with the key press
      display.clearDisplay();
      display.setCursor(0, 10);
      display.setTextSize(1);
      display.println("Press a key:");
      display.setTextSize(2);  // Increase text size for key display
      display.setCursor(0, 30);
      display.print("Key: ");
      display.println(key);
      display.display();

      if (key == '1') {
        if (!dacInUse) {
          setupAudio();
          playNote();
          dacInUse = true;
        }
      } else if (key == '0') {
        if (dacInUse) {
          stopNote();
          dacInUse = false;
        }
      }
    }
  }
  delay(10);
}
