// OLED display width and height, in pixels
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
// defining the specific keypad here via PID
#define KEYPAD_PID3845

// define keypad pins
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
#define I2S_SAMPLE_RATE (16000)
#define I2S_BITS        (16)
#define I2S_CHANNELS    (1)
#define I2S_SAMPLE_BITS   (16)
#define I2S_READ_LEN      (8 * 1024 )
#define RECORD_TIME       (5) // Seconds
#define FLASH_RECORD_SIZE (I2S_CHANNELS * I2S_SAMPLE_RATE * I2S_SAMPLE_BITS / 8 * RECORD_TIME)

// Connections to INMP441 I2S microphone
#define I2S_WS 14
#define I2S_SD 15
#define I2S_SCK 32
#define I2S_PORT I2S_NUM_0

// Define pins for external DAC
#define DAC_BCK_PIN 26
#define DAC_WS_PIN 25
#define DAC_DATA_PIN 22

WebServer server(80);

TaskHandle_t wifiTaskHandle = NULL;
bool wifiConnected = false;
bool connectToWiFiTask = false;

File file;
const char filename[] = "/recording.wav";
const int headerSize = 44;

SemaphoreHandle_t i2sMutex;

TaskHandle_t recordingTaskHandle = NULL;

enum State {
    CONNECT_TO_WIFI,
    INITIAL_STATE,
    PLAY_START_MESSAGE,
    RECORDING_MESSAGE,
    POST_RECORDING,
    PLAYING_MESSAGE,
    SENDING_MESSAGE,
    TRYING_TO_READ_PASSWORD,
    WAITING_FOR_PASSWORD,
    OPEN_DOOR,
    PASSWORD_EXPIRED,
    ENTRY_DENIED,
    RETRY_SENDING_MESSAGE,
    WRONG_PASSWORD
};

unsigned long lastCheck = 0;
unsigned long lastPasswordCheck = 0;
const unsigned long checkInterval = 5000; // Check every 5 seconds
unsigned long passwordCheckInterval = 10 *1000;  // For example, check every 10 seconds
const unsigned long debounceDelay = 200; // 200 milliseconds debounce delay
unsigned long lastKeyPressTime = 0;


// Initialize the state variable
State state = INITIAL_STATE;

size_t chunkIndex = 0;

String inputSequence = ""; // Variable to store key input sequence
String enteredPassword = "____"; // Variable to store key input sequence
String defult_password = "4444";
int password_counter = 1 ;
unsigned long passwordTimestamp = 0;
int password_duration = 0 ;
String current_password = "";
bool play_message = false;
bool isDeny = false;

void displayMessage(const char* message, int duration, int size) {
  display.clearDisplay();
  display.setCursor(0, 20);
  display.setTextSize(size);
  display.println(message);
  display.display();
  if (duration > 0) {
    delay(duration);
  }
}

void displayOpenedMessage(const char* message, int duration) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);

  String msg = String(message);
  int lineHeight = 20;  // Height of each line

  int y = 0;
  int start = 0;
  int end = msg.indexOf(' ');
  while (end != -1) {
    String line = msg.substring(start, end);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(line, 0, y, &x1, &y1, &w, &h);
    int x = (SCREEN_WIDTH - w) / 2;  // Center horizontally
    display.setCursor(x, y);
    display.println(line);
    y += lineHeight;
    start = end + 1;
    end = msg.indexOf(' ', start);
  }
  String line = msg.substring(start);  // Print the last word
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(line, 0, y, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;  // Center horizontally
  display.setCursor(x, y);
  display.println(line);

  display.display();
  if (duration > 0) {
    delay(duration);
  }
}

void showPostRecordingOptions() {
  displayMessage("Press * to rerecord  Press1to play messagePress 2 to send", 0, 1);
}

void wavHeader(byte* header, int wavSize) {
  header[0] = 'R';
  header[1] = 'I';
  header[2] = 'F';
  header[3] = 'F';
  unsigned int fileSize = wavSize + headerSize - 8;
  header[4] = (byte)(fileSize & 0xFF);
  header[5] = (byte)((fileSize >> 8) & 0xFF);
  header[6] = (byte)((fileSize >> 16) & 0xFF);
  header[7] = (byte)((fileSize >> 24) & 0xFF);
  header[8] = 'W';
  header[9] = 'A';
  header[10] = 'V';
  header[11] = 'E';
  header[12] = 'f';
  header[13] = 'm';
  header[14] = 't';
  header[15] = ' ';
  header[16] = 0x10;
  header[17] = 0x00;
  header[18] = 0x00;
  header[19] = 0x00;
  header[20] = 0x01;
  header[21] = 0x00;
  header[22] = 0x01;
  header[23] = 0x00;
  header[24] = 0x80;
  header[25] = 0x3E;
  header[26] = 0x00;
  header[27] = 0x00;
  header[28] = 0x00;
  header[29] = 0x7D;
  header[30] = 0x00;
  header[31] = 0x00;
  header[32] = 0x02;
  header[33] = 0x00;
  header[34] = 0x10;
  header[35] = 0x00;
  header[36] = 'd';
  header[37] = 'a';
  header[38] = 't';
  header[39] = 'a';
  header[40] = (byte)(wavSize & 0xFF);
  header[41] = (byte)((wavSize >> 8) & 0xFF);
  header[42] = (byte)((wavSize >> 16) & 0xFF);
  header[43] = (byte)((wavSize >> 24) & 0xFF);
}

