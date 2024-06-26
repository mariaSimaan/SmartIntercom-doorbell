#include <Adafruit_Keypad.h> // Correct inclusion of the library

// define your specific keypad here via PID
#define KEYPAD_PID3845

// define your pins here
#define R4    25
#define R3    27
#define R2    14
#define R1    32
#define C1    33
#define C2    12
#define C3    26

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

void setup() {
  Serial.begin(115200);  // Adjusted for ESP32
  customKeypad.begin();
}

void loop() {
  customKeypad.tick();
  while(customKeypad.available()) {
    keypadEvent e = customKeypad.read();
    char key = (char)e.bit.KEY;
    Serial.print(key);
    if (e.bit.EVENT == KEY_JUST_PRESSED) {
      Serial.println(" pressed");
    } else if (e.bit.EVENT == KEY_JUST_RELEASED) {
      Serial.println(" released");
    }
  }

  delay(10);
}
