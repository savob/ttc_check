#include "wifiInfo.h"

/* wifiInfo.h Explanation
 * This is a header file that contains the SSID and password for my network.
 * It is only the following two lines of code:
 * 
 * const char* ssid = "ssid";
 * const char* password = "password";
 * 
 * Did this to avoid accidentally uploading that info online (again).
 * I know it isn't proper C to define values in headers, but it works in the 
 * Arduino system, so whatever. You can either make your own header, or just
 * hardcode them in this file.
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG // Include this when "debug"
// Used for testing average duration of code sections when debugging
int cycleCount = 0;
long startTime = 0;
float averageCycleTime = 0;
long totalTime = 0;
#endif

// Pin allocations
const byte displayDataPin = 16;
const byte displayDataClockPin = 17;
const byte displayOutputClockPin = 18;
const byte displayEnablePin = 19;

const byte digitPins[] = { 4, 21, 22, 23};
const byte buttonPins[] = { 2, 15, 35, 34};

// Variables used for the display
byte currentDigit = 0;
byte displayBuffer[4];

const byte numberCharacters[] = {0x5F, 0x44, 0x9D, 0xD5, 0xC6, 0xD3, 0xDB, 0x45, 0xDF, 0xD7, 0x7F, 0x64, 0xBD, 0xF5, 0xE6, 0xF3, 0xFB, 0x65, 0xFF, 0xF7};
const byte dashCharacter = 0x80;

hw_timer_t * timer = NULL; // Hardware timer used for display updating

void IRAM_ATTR onTimer() {
  dispDigit(currentDigit, displayBuffer[currentDigit]);

  if (currentDigit == 3) currentDigit = 0; //Reset
  else currentDigit++;
}

const char* host = "retro.umoiq.com";
const int hostPort = 443;

// Root CA for Amazon (Host of UMO IQ)
const char* rootCACertificate = \
                                "-----BEGIN CERTIFICATE-----\n" \
                                "MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF\n" \
                                "ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6\n" \
                                "b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL\n" \
                                "MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv\n" \
                                "b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj\n" \
                                "ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM\n" \
                                "9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw\n" \
                                "IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6\n" \
                                "VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L\n" \
                                "93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm\n" \
                                "jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC\n" \
                                "AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA\n" \
                                "A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI\n" \
                                "U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs\n" \
                                "N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv\n" \
                                "o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU\n" \
                                "5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy\n" \
                                "rqXRfboQnoZsG4q5WTP468SQvvG5\n" \
                                "-----END CERTIFICATE-----\n";

const int MIN_TIME = 3; // Skip arrivals sooner than this (minutes)
const int MAX_TIME = 20; // Disregard times this long or longer (minutes)
const int READ_TIMEOUT = 15; // Cancel query if no data received (seconds)

const byte NUMBER_BUTTONS = 4;
const int DISPLAY_TIME = 5000; // Time numbers are visible (ms)

const char keyword[] = "minutes=\"";
const byte NUMBER_PREDICTIONS = 4; // Number of preditions to maintain per stop

struct {
  const char       *agency; // NOTE: This is currently hardcoded to be the TTC in requests to avoid errors
  const int        route;
  const int        stopID;
  const byte       buttonID; // Which button the stop is meant to get included on
  int              minutes[NUMBER_PREDICTIONS];    // Most recent predictions from server
} stops[] = {
  {"ttc", 352, 15038, 0 },   // Corona 352 E
  {"ttc", 52, 15038, 0 },    // Corona 52 E
  {"ttc", 59, 15038, 0 },    // Corona 59 E
  {"ttc", 52, 6485, 1 },     // Dufferin 52 E
  {"ttc", 352, 6485, 1 },    // Dufferin 352 E
  {"ttc", 59, 6485, 1 },      // Dufferin 59 E
  {"ttc", 29, 8930, 2 },      // Lawernce 29 S
  {"ttc", 52, 5733, 3 },      // Dufferin 52 W
  {"ttc", 352, 5733, 3 }      // Dufferin 352 W
};
const int NUM_STOPS = (sizeof(stops) / sizeof(stops[0]));
uint8_t stopIndex = NUM_STOPS; // Stop # currently being searched

WiFiClientSecure client; // Need to be secure since it uses SSL / HTTPS


void setup() {
  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  ArduinoOTA
  .onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  })
  .onEnd([]() {
    Serial.println("\nEnd");
  })
  .onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\n", (progress / (total / 100)));
  })
  .onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  client.setCACert(rootCACertificate); // Set CA for exchange with Umo IQ

  displaySetup();
  setupButtons();

  log_i("SETUP COMPLETE\n");
  delay(100);
}

void loop() {
  ArduinoOTA.handle();
  
  setupButtons();
  
  uint32_t t;
  boolean  timedOut;

  for (byte buttonIndex = 0; buttonIndex < NUMBER_BUTTONS; buttonIndex++) {
    yield(); // Handle WiFi events
    if (digitalRead(buttonPins[buttonIndex]) == LOW) {
      // Pressed button

      log_i("Button pressed on pin %d, buttonIndex = %d", buttonPins[buttonIndex], buttonIndex);

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG // Include this when "debugging"
      startTime = millis(); // Used for time benchmarks
#endif

      displayDashes(); // Clear display values
      int tempValues[NUMBER_PREDICTIONS]; // Used to store temporary values to be displayed
      for (byte i = 0; i < NUMBER_PREDICTIONS; tempValues[i++] = 0); // Clear values

      for (stopIndex = 0; stopIndex < NUM_STOPS; stopIndex++) {
        if (stops[stopIndex].buttonID == buttonIndex) {


          log_d("Stop #%d Route: %d\n", stops[stopIndex].stopID, stops[stopIndex].route);
          log_d("Contacting server...");

          // Clear predictions
          for (byte i = 0; i < NUMBER_PREDICTIONS; stops[stopIndex].minutes[i++] = 0);

          if (client.connect(host, hostPort)) {

            log_d("Contact made. Requesting data...");

            char request[150];

            sprintf(request, "GET /service/publicXMLFeed?command=predictions&a=%s&r=%d&s=%d HTTP/1.1\r\nHost: %s\r\nConnection: Close\r\n\r\n",
                    "ttc", //stops[stopIndex].agency,
                    /* Hardcoded agency because this was causing issues on the third request rebooting the system.
                      No idea why. Before I was using sprintf, I was printing parameter by parameter like the example
                      Which is why I know that 'agency' was the source of the issue.
                      Hardcoding seems to have resolved it.

                      If anyone can figure it out let me know
                    */
                    stops[stopIndex].route,
                    stops[stopIndex].stopID,
                    host);

            client.print(request);
            client.flush();
            yield(); // Handle WiFi events

            log_v("Request from ESP32:\n%s", request);

            t        = millis(); // Start time
            timedOut = false;

            // Read until transmission is complete and buffer is emptied afterwards
            while (client.connected() || client.available()) {
              yield(); // Handle WiFi events

              if (client.available() > 0) {
                // If data present
                extractMinutes();
                t = millis(); // Reset timeout clock
              }
              else if ((millis() - t) >= (READ_TIMEOUT * 1000)) {

                log_i("---Timeout---");

                timedOut = true;
                break;
              }
            }


            log_v("MERGING INTO MAIN LIST. Current main list:");
            for (byte j = 0; j < NUMBER_PREDICTIONS; j++) log_v("Prediction %d: %d", j, tempValues[j]);

            // Go through existing times to see if our results can be added to the list in order
            for (byte j = 0; j < NUMBER_PREDICTIONS; j++) {
              bool inserted = insertIntoArray(stops[stopIndex].minutes[j], tempValues, NUMBER_PREDICTIONS);

              if (!inserted) break; // Break if can't insert value, since the future values are only larger
            }
          }

          else log_e("Failed to connect.");

          client.stop();
        }
      }

      // Now we need to display these values
      // Plan is to use 7-seg displays. Use decimal to indicate +10, e.g. 1. = 11
      // Hence why we are only interested in four predictions (one per digit)
      // that are below 20 (can only indicate 19 max)
      log_d("Compiled list. Button %d:", buttonIndex + 1);
      for (byte i = 0; i < NUMBER_PREDICTIONS; i++) log_d("Prediction %d: %d", i + 1, tempValues[i]);



#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG // Include this when "debugging"
      Serial.println("=========================================");
      totalTime += millis() - startTime; // Adds current cycle time in ms
      cycleCount++;
      averageCycleTime = totalTime / cycleCount; // Gets new average
      log_d("Average cycle time: %d\n", averageCycleTime);
#endif

      displayPredictions(tempValues);
      delay(DISPLAY_TIME);
      blankDisplay(); //Clear display
    }
  }

  delay(20);
}





/* Minutes Extractor ------------------------------------------
    Goes through the XML string looking for the keyword
    Once it finds the keyword it'll record the minutes
    by taking the next two characters and putting them
    through atoi()
*/
void extractMinutes() {
  const char keyword[] = "minutes=\"";
  const byte keywordLength = 9;

  log_v("RESPONSE");


  while (client.available()) {
    byte charactersMatching;

    // See if it can spot the keyword
    for (charactersMatching = 0; charactersMatching < keywordLength; charactersMatching++) {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE // Run this when "verbose"
      char temp = client.read();
      Serial.print(temp);                   // Print response character by character
      if (temp != keyword[charactersMatching]) break; // Break if there is a mismatch
#else
      if (client.read() != keyword[charactersMatching]) break; // Break if there is a mismatch
#endif
    }

    // Check if there is a match (all characters matched)
    if (charactersMatching == keywordLength) {
      char characters[2];
      characters[0] = client.read();
      characters[1] = client.read();
      int entry = atoi(characters);

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE // Run this when "verbose"
      Serial.printf("!!%d MINUTES!!", entry);
#endif

      if ((entry >= MIN_TIME) && (entry < MAX_TIME)) {
        // When within our defined limits, insert the value into sorted list
        insertIntoArray(entry, stops[stopIndex].minutes, NUMBER_PREDICTIONS);

      }
    }
  }

  log_v("END OF RESPONSE");


  return;
}


// Function used to insert values into an array in ascending order.
// Returns false if value wasn't inserted
bool insertIntoArray(int value, int inputArray[], byte arrayLength) {

  // Output list if "Verbose" output is set
  log_v("Trying to put %d into list. Current list:\n", value);
  for (byte i = 0; i < arrayLength; i++) log_v(" Entry %d: %d", i, inputArray[i]);


  if (value == 0) return false; // Get rid of blanks
  for (byte i = 0; i < arrayLength; i++) {
    if (inputArray[i] == 0) {
      // Comes across a blank
      inputArray[i] = value;
      return true;
    }
    else if (inputArray[i] >= value) {
      // If the new prediction is sooner than another one that was already queued
      // Shifts all predictions to the back, then inserts the new value
      for (byte j = arrayLength; j > i; j--) inputArray[j] = inputArray[j - 1];
      inputArray[i] = value;
      return true;
    }
    // If it is larger than all exsisting values it doesn't get stored
    // sux to succ
  }
  return false; // Value wasn't inserted at all
}


void setupButtons() {

  // Buttons 1 and 2 have internal pullups, 3 and 4 don't
  pinMode(buttonPins[0], INPUT_PULLUP);
  pinMode(buttonPins[1], INPUT_PULLUP);
  pinMode(buttonPins[2], INPUT);
  pinMode(buttonPins[3], INPUT);

  return;
}
