#include <WiFiManager.h> // יש לוודא שהספרייה מותקנת

WiFiManager wifiManager;

void setup() {
  // התחל את ה-Serial להדפסות באורך הבדיקות
  Serial.begin(115200);

  // נסה להתחבר לרשת WiFi קיימת
  if (!wifiManager.autoConnect("AutoConnectAP")) {
    Serial.println("Failed to connect and hit timeout");
    // ניתן להוסיף כאן קוד לטיפול במקרה של כישלון התחברות
    ESP.restart();
  }

  // אם הגענו לכאן, ה-ESP32 מחובר לרשת WiFi
  Serial.println("Connected to WiFi");
}

void loop() {
  // בדוק את מצב החיבור
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Lost connection to WiFi. Trying to reconnect...");

    // נסה להתחבר מחדש במשך 10 שניות
    unsigned long startAttemptTime = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
      delay(500);
      Serial.print(".");
    }

    // אם לא הצלחת להתחבר מחדש
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Failed to reconnect. Restarting configuration portal...");
      wifiManager.resetSettings(); // אופציונלי - מאפס את ההגדרות
      wifiManager.autoConnect("AutoConnectAP");
    }
  }

  // כאן ניתן להוסיף קוד לפעולות השוטפות
  delay(1000); // לדוגמה, המתנה של שנייה בין כל בדיקה
}
