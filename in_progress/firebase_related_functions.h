#include "definitions.h"

//Firebase credentials and details
#define FIREBASE_HOST "https://smartintercom-b517f-default-rtdb.europe-west1.firebasedatabase.app/"
#define FIREBASE_AUTH "AIzaSyAFFmEbQA4_s2ilpv90l0v516sc4BiurZc"
#define DATABASE_SECRET "AIzaSyAFFmEbQA4_s2ilpv90l0v516sc4BiurZc"
#define DATABASE_URL "https://smartintercom-b517f-default-rtdb.europe-west1.firebasedatabase.app/"

WiFiClientSecure ssl;
DefaultNetwork network;
AsyncClientClass client(ssl, getNetwork(network));

FirebaseApp app;
RealtimeDatabase Database;
AsyncResult result;
LegacyToken dbSecret(DATABASE_SECRET);


bool sendTestString() {
  String testPath = "/ICST/TestString";
  String testValue = "This is a test";

  Serial.println("Sending test string...");
  bool success = Database.set<String>(client, testPath.c_str(), testValue);
  if (success) {
    Serial.println("Test string uploaded successfully.");
  } else {
    Serial.println("Failed to upload test string.");
    Serial.printf("Error: %s\n", result.error().message().c_str());
  }
  return success;
}

bool sendWavFile(const char* filename) {
  File file = LittleFS.open(filename, FILE_READ);
  if (!file) {
    Serial.println("Failed to open WAV file for reading");
    return false;
  }
  displayMessage("sending message ...", 0, 1);

  Serial.println("WAV file opened successfully.");
  size_t fileSize = file.size();
  Serial.printf("WAV file size: %d bytes\n", fileSize);

  const size_t bufferSize = 1024 * 4;
  uint8_t buffer[bufferSize];
  size_t bytesRead;

  // Reset file pointer to the beginning
  //file.seek(0, SeekSet);
  file.seek(bufferSize * chunkIndex , SeekSet);

  Serial.println("Reading and encoding WAV file...");

  while ((bytesRead = file.read(buffer, bufferSize)) > 0) {
    Serial.printf("Read %d bytes from WAV file at position %d\n", bytesRead, file.position());

    if (bytesRead > 0) {
      String encodedChunk = base64::encode(buffer, bytesRead);
      String path = String("/ICST/WAVString/chunk") + chunkIndex;

      bool success = false;
      int retryCount = 0;

      success = Database.set<String>(client, path.c_str(), encodedChunk);
      delay(100);
      /*
      // Retry loop for sending the chunk
      while (!success && retryCount < 10) {
        Serial.printf(" 1 - Failed to upload chunk %d\n", chunkIndex, retryCount + 1);
        try {
            Serial.printf(" 11 - Failed to upload chunk %d\n", chunkIndex, retryCount + 1);
            Serial.printf("Free heap after Database.set: %d bytes\n", ESP.getFreeHeap());
            success = Database.set<String>(client, path.c_str(), encodedChunk);
            Serial.printf("Free heap after Database.set: %d bytes\n", ESP.getFreeHeap());
            Serial.printf("Success status after upload attempt: %s\n", success ? "true" : "false");

            Serial.printf(" 12 - Failed to upload chunk %d\n", chunkIndex, retryCount + 1);

        } catch (const std::exception& e) {  // Catch exceptions by reference
            Serial.printf(" 13 - Failed to upload chunk %d\n", chunkIndex, retryCount + 1);
            Serial.println(e.what());  // Print the exception message
            Serial.printf(" 14 - Failed to upload chunk %d\n", chunkIndex, retryCount + 1);

        }

        Serial.printf(" 2 - Failed to upload chunk %d\n", chunkIndex, retryCount + 1);

        if (!success) {
          Serial.printf("/////////////////////Failed to upload chunk %d, attempt %d\n", chunkIndex, retryCount + 1);
          Serial.printf(" 3 - Failed to upload chunk %d\n", chunkIndex, retryCount + 1);
          retryCount++;
          Serial.printf(" 4 - Failed to upload chunk %d\n", chunkIndex, retryCount + 1);
          delay(500);
          Serial.printf(" 5 - Failed to upload chunk %d\n", chunkIndex, retryCount + 1);

        }
      }
*/
      if (success) {
        Serial.printf("Chunk %d uploaded successfully\n", chunkIndex);
        chunkIndex++;
      } else {
        Serial.printf("Failed to upload chunk %d after 10 attempts\n", chunkIndex);
        file.close();
        state = RETRY_SENDING_MESSAGE;
        return false;
      }
    } else {
      Serial.println("No data read from file.");
    }

    delay(100);  // Small delay to avoid overwhelming the server
  }

  if (chunkIndex == 0) {
    Serial.println("No chunks were processed.");
  }

  Serial.println("Finished reading and encoding WAV file.");
  file.close();

  // Fallback test read
  file = LittleFS.open(filename, FILE_READ);
  if (!file) {
    Serial.println("Failed to reopen WAV file for reading");
    return false;
  }
  Serial.println("Reopened WAV file for fallback read test.");
  bytesRead = file.read(buffer, bufferSize);
  Serial.printf("Fallback read %d bytes from WAV file\n", bytesRead);
  file.close();
  LittleFS.remove(filename);

  chunkIndex = 0;
  return true;
}

void sendWaveToFirebase(){
  // Test string upload
  if (sendTestString()) {
    Serial.println("Test string sent to Firebase successfully.");
  } else {
    Serial.println("Failed to send test string.");
  }

  // Check if file exists
  if (LittleFS.exists("/recording.wav")) {
    Serial.println("WAV file exists. Proceeding to send...");
    // Read, encode, and send the WAV file to Firebase Realtime Database
    if (sendWavFile("/recording.wav")) {
      displayMessage("Message is sent", 2 * 1000,1);
      Serial.println("WAV file sent to Firebase successfully.");
    } else {
      Serial.println("Failed to send WAV file.");
    }
  } else {
    Serial.println("WAV file does not exist in LittleFS.");
  }
}


bool readPasswordFromFireBase() {
    String passwordPath = "/messages/password";
    String passwordDurationPath = "/messages/duration";
    String passwordDenyPath = "/messages/isDeny";

    isDeny =  Database.get<bool>(client, passwordDenyPath.c_str());
    Serial.println("isDeny is ");
    Serial.println(isDeny);
    if (isDeny == true) {  // Check if reading failed or returned "null"
      state = ENTRY_DENIED;
      return true;
    }

    Serial.println("Reading password string...");
    current_password = Database.get<String>(client, passwordPath.c_str());
    if (current_password == "null" || current_password.isEmpty()) {  // Check if reading failed or returned "null"
        Serial.println("Failed to read password string.");
        return false;
    }

    String password_duration_str = Database.get<String>(client, passwordDurationPath.c_str());
    if (password_duration_str == "null" || password_duration_str.isEmpty()) {  // Check if reading failed or returned "null"
        Serial.println("Failed to read password duration.");
        return false;
    }

    
    Serial.println("Password string read successfully.");
    Serial.println("Password: " + current_password);
    Serial.println("Password duration: " + password_duration_str);
    password_duration = password_duration_str.toInt();

    displayMessage("waiting for owner's  response", 0, 1);
    return true;
}



bool waiting_to_read_password()
{ 
  displayMessage("waiting for owner's  response", 0, 1);
  unsigned long startMillis = millis(); 
  unsigned long curentMillis =  millis() ;

  while (!readPasswordFromFireBase() )
  {
    if ( curentMillis - startMillis > 60 * 1000)
    {
      displayMessage("Did not get any      response from owner, please try again     later", 20 * 1000, 1);
      return false;
    }
    curentMillis = millis();
    delay(1000);
  }

   return true;

}

bool isPasswordExpired() {
  unsigned long currentMillis = millis(); 
  unsigned long duration = password_duration * 1000;
  if (currentMillis - passwordTimestamp >= duration) {
    Serial.printf("currentMillis is %d \n", currentMillis);
    Serial.printf("passwordTimestamp is %d \n", passwordTimestamp);
    Serial.printf("duration is %d \n", duration);

    Serial.println("Password has expired.");
    return true;
  } else {
    return false;
  }
}

