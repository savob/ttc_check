/*
    Based on TinyXML NextBus example

   See the manual on how to configure this at:
   https://www.nextbus.com/xmlFeedDocs/NextBusXMLFeed.pdf
*/

#include <WiFi.h>
#include <TinyXML.h>

// CONFIGURABLE GLOBAL STUFF -----------------------------------------------
const bool debugMode = true; // Toggle sreial and debug/detailed messages

char ssid[] = "ssid",     // WiFi network name
              pass[] = "password", // WiFi network password
                       host[] = "webservices.nextbus.com";

const int MIN_TIME = 1; // Skip arrivals sooner than this (minutes)
const int MAX_TIME = 20; // Disregard times this long or longer (minutes)
const int READ_TIMEOUT = 15; // Cancel query if no data received (seconds)

const byte NUMBER_BUTTONS = 4;
const byte BUTTON_PIN[NUMBER_BUTTONS] = {25, 26, 27, 14}; // buttons used for stops

const int DISPLAY_TIME = 5000; // Time numbers are visible (ms)

WiFiClient client;
TinyXML    xml;
uint8_t    buffer[150]; // For XML decoding
const byte NUMBER_PREDICTIONS = 4; // Number of preditions to maintain per stop
int displayBuffer[NUMBER_PREDICTIONS]; // Stores digits to display
struct {
  const char       *agency; // NOTE: This is currently hardcoded to be the TTC in requests to avoid errors
  const int        route;
  const int        stopID;
  const byte       buttonID; // Which button the stop is meant to get included on
  int              minutes[NUMBER_PREDICTIONS];    // Most recent predictions from server
} stops[] = {
  {"ttc", 352, 15038, 0 },   // Corona 352
  {"ttc", 52, 15038, 0 },    // Corona 52
  {"ttc", 59, 15038, 0 },    // Corona 59
  {"ttc", 52, 6485, 1 },     // Dufferin 52
  {"ttc", 352, 6485, 1 },    // Dufferin 352
  {"ttc", 59, 6485, 1 }      // Dufferin 59
};
const int NUM_STOPS = (sizeof(stops) / sizeof(stops[0]));
uint8_t    stopIndex = NUM_STOPS; // Stop # currently being searched

// Used for testing average duration of code sections
int cycleCount = 0;
long startTime = 0;
float averageCycleTime = 0;
long totalTime = 0;

void setup(void) {
  if (debugMode) {
    Serial.begin(115200);
    Serial.println("NextBus Tracker");
  }

  xml.init((uint8_t *)buffer, sizeof(buffer), &XML_callback);

  connectToWifi();
  displaySetup();

  for (byte i = 0; i < NUMBER_BUTTONS; i++) pinMode(BUTTON_PIN[i], INPUT_PULLUP); // Set button to be pulled up
  blankDisplay();
}

// MAIN LOOP ---------------------------------------------------------------

void loop(void) {
  uint32_t t;
  int      c;
  boolean  timedOut;

  yield(); // Handle WiFi events

  // Check WiFi connection, attempt connection if disconnected
  if (WiFi.status() != WL_CONNECTED) {
    connectToWifi();
  }

  for (byte buttonIndex = 0; buttonIndex < NUMBER_BUTTONS; buttonIndex++) {
    if (digitalRead(BUTTON_PIN[buttonIndex]) == LOW) {
      // Pressed button
      if (debugMode) {
        Serial.print("Button pressed on pin ");
        Serial.print(BUTTON_PIN[buttonIndex]);
        Serial.print(", buttonIndex = ");
        Serial.println(buttonIndex);
      }

      startTime = millis();

      for (byte i = 0; i < NUMBER_PREDICTIONS; displayBuffer[i++] = 0); // Clear display values

      if (debugMode) Serial.println("Button Pressed");
      for (stopIndex = 0; stopIndex < NUM_STOPS; stopIndex++) {
        if (stops[stopIndex].buttonID == buttonIndex) {
          if (debugMode)  {
            Serial.print("Stop #");
            Serial.print(stops[stopIndex].stopID);
            Serial.print(" Route: ");
            Serial.println(stops[stopIndex].route);
            Serial.print("Contacting server...");
          }
          if (client.connect(host, 80)) {
            if (debugMode) Serial.println("OK\r\nRequesting data...");
            char request[150];

            sprintf(request, "GET /service/publicXMLFeed?command=predictions&a=%s&r=%d&s=%d HTTP/1.1\r\nHost: %s\r\nConnection: Close\r\n\r\n",
                    "ttc", //stops[stopIndex].agency,
                    // Hardcoded because this was causing issues on the third request rebooting the system.
                    // No idea why. Before I was using sprintf, I was printing parameter by parameter like the example
                    // Which is why I know that 'agency' was the source of the issue.
                    // Hardcoding seems to have resolved it.
                    // If anyone can figure it out let me know
                    stops[stopIndex].route,
                    stops[stopIndex].stopID,
                    host);

            if (debugMode) Serial.print(request);
            client.print(request);
            client.flush();
            xml.reset();
            t        = millis(); // Start time
            timedOut = false;

            // Clear predictions
            for (byte i = 0; i < NUMBER_PREDICTIONS; stops[stopIndex].minutes[i++] = 0);

            while (client.connected()) {
              yield(); // Handle WiFi events

              if ((c = client.read()) >= 0) {
                xml.processChar(c);
                t = millis(); // Reset timeout clock
              } else if ((millis() - t) >= (READ_TIMEOUT * 1000)) {
                if (debugMode) Serial.println("---Timeout---");
                timedOut = true;
                break;
              }
            }

            // Go through existing times to see if our results can be added to the list in order
            for (byte j = 0; j < NUMBER_PREDICTIONS; j++) {
              bool inserted = insertIntoArray(stops[stopIndex].minutes[j], displayBuffer, NUMBER_PREDICTIONS);

              if (!inserted) break;
              // Break if can't insert value, since the future values are only larger
            }
          }
          else if (debugMode) Serial.println("Failed to connect.");

          client.stop();
          if (debugMode) Serial.println();
        }
      }

      // Now we need to display these values
      // Plan is to use 7-seg displays. Use decimal to indicate +10, e.g. 1. = 11
      // Hence why we are only interested in four predictions (one per digit)
      // that are below 20 (can only indicate 19 max)
      if (debugMode) {
        Serial.print("Compiled list. Button ");
        Serial.print(buttonIndex + 1);
        Serial.println(":");
        for (byte i = 0; i < NUMBER_PREDICTIONS; Serial.println(displayBuffer[i++]));

        Serial.println("=========================================");
        totalTime += millis() - startTime; // Adds current cycle time in ms
        cycleCount++;
        averageCycleTime = totalTime / cycleCount; // Gets new average
        Serial.print("Average cycle time: ");
        Serial.println(averageCycleTime);
      }

      // Display values
      displayPredictions();
      delay(DISPLAY_TIME);      
      blankDisplay(); //Clear display
    }
  }
  delay(200);
}

/* UTILITY FUNCTIONS -------------------------------------------------------

  TinyXML invokes this callback as elements are parsed in XML,
  allowing us to pick out just the tidbits of interest rather than
  dynamically allocating and then traversing a whole brobdingnagian
  tree structure; very Arduino-friendly!
  As written here, this looks for XML tags named "minutes" and extracts
  the accompanying value (a predicted bus arrival time, in minutes).
  If it's longer than our minimum threshold, it is sorted into the list
  for a stop.
*/
void XML_callback(uint8_t statusflags, char* tagName, uint16_t tagNameLen, char* data, uint16_t dataLen) {
  if ((statusflags & STATUS_ATTR_TEXT) && !strcasecmp(tagName, "minutes")) {
    uint32_t t = atoi(data); // Prediction in minutes (0 if gibberish)
    if (debugMode) {
      Serial.print(t);
      Serial.println(" minutes");
    }

    if ((t >= MIN_TIME) && (t < MAX_TIME)) {
      // When within our defined limits, insert the value into sorted list
      insertIntoArray(t, stops[stopIndex].minutes, NUMBER_PREDICTIONS);
    }
  }
}

// Function used to insert values into an array in ascending order.
// Returns false if value wasn't inserted
bool insertIntoArray(int value, int inputArray[], byte arrayLength) {
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

void connectToWifi() {
  int c;

  if (debugMode) Serial.print("WiFi connecting..");
  WiFi.begin(ssid, pass);
  // Wait up to 1 minute for connection...
  for (c = 0; (c < 60) && (WiFi.status() != WL_CONNECTED); c++) {
    if (debugMode) Serial.write('.');
    delay(1000);
  }
  if (c >= 60) { // If it didn't connect within 1 min
    if (debugMode) Serial.println("Failed. Will retry...");
    return;
  }
  if (debugMode) Serial.println("OK!");
  delay(2000); // Pause before hitting it with queries & stuff
}

void displayPredictions() {
  if (displayBuffer[0] == 0 && displayBuffer[1] == 0 && displayBuffer[2] == 0 && displayBuffer[3] == 0 ) return; // If no predictions to display (all zero) leave it
  else {
    // Leave all values but blank trailing zeroes
    for (byte i = 1; i < NUMBER_PREDICTIONS; i++) {
      if (displayBuffer[i] == 0) displayBuffer[i] = 20; // Blank character
    }
  }
  Serial.println("Display characters are:");
  for (byte i = 0; i < NUMBER_PREDICTIONS; Serial.println(displayBuffer[i++]));
}
