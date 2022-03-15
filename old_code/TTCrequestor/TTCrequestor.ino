/*
    Based on TinyXML NextBus example

   See the manual on how to configure this at:
   https://www.nextbus.com/xmlFeedDocs/NextBusXMLFeed.pdf
*/

#include <WiFi.h>
#include <TinyXML.h>

// CONFIGURABLE GLOBAL STUFF -----------------------------------------------
const bool debugMode = true; // Toggle debug/detailed messages

char ssid[] = "ssid",     // WiFi network name
              pass[] = "password", // WiFi network password
                       host[] = "webservices.nextbus.com";

const int MIN_TIME = 1; // Skip arrivals sooner than this (minutes)
const int MAX_TIME = 30; // Disregard times this long or longer (minutes)
const int READ_TIMEOUT = 15; // Cancel query if no data received (seconds)

const byte BUTTON_PIN = 25;

WiFiClient client;
TinyXML    xml;
uint8_t    buffer[150]; // For XML decoding
const byte numberPredictions = 4; // Number of preditions to maintain per stop
int dispValues[2][numberPredictions]; // Stores digits to display
struct {
  const char       *agency; // NOTE: This is currently hardcoded to be the TTC in requests to avoid errors
  const int       route;
  const int       stopID;
  const byte       displayID; // Which display the stop is meant to go on
  uint32_t         lastQueryTime; // Time of last query (millis())
  int              minutes[numberPredictions];    // Most recent predictions from server
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

// UTILITY FUNCTIONS -------------------------------------------------------

// TinyXML invokes this callback as elements are parsed in XML,
// allowing us to pick out just the tidbits of interest rather than
// dynamically allocating and then traversing a whole brobdingnagian
// tree structure; very Arduino-friendly!
// As written here, this looks for XML tags named "minutes" and extracts
// the accompanying value (a predicted bus arrival time, in minutes).
// If it's longer than our minimum threshold, it is sorted into the list
// for a stop.
void XML_callback(uint8_t statusflags, char* tagName,
                  uint16_t tagNameLen, char* data, uint16_t dataLen) {
  if ((statusflags & STATUS_ATTR_TEXT) && !strcasecmp(tagName, "minutes")) {
    uint32_t t = atoi(data); // Prediction in minutes (0 if gibberish)
    if (debugMode) {
      Serial.print(t);
      Serial.println(" minutes");
    }

    if ((t >= MIN_TIME) && (t < MAX_TIME)) {
      // When within our defined limits, insert the value into sorted list
      insertIntoArray(t, stops[stopIndex].minutes, numberPredictions);
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

// ONE-TIME INITIALIZATION -------------------------------------------------

void setup(void) {
  Serial.begin(115200);
  Serial.println("NextBus Tracker");
  xml.init((uint8_t *)buffer, sizeof(buffer), &XML_callback);

  connectToWifi();

  pinMode(BUTTON_PIN, INPUT_PULLUP); // Set button to be pulled up
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

  if (digitalRead(BUTTON_PIN) == LOW) {
    // Pressed button
    startTime = millis();

    if (debugMode) Serial.println("Button Pressed");
    for (stopIndex = 0; stopIndex < NUM_STOPS; stopIndex++) {

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
        for (byte i = 0; i < numberPredictions; stops[stopIndex].minutes[i++] = 0);
        //delay(500);
        
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
        if (!timedOut && stops[stopIndex].minutes[0]) { // Successful transfer?
          // Copy newly-polled predictions to stops structure:
          stops[stopIndex].lastQueryTime = millis();
        }
        //delay(200); // Time between queries
      }
      client.stop();
      if (debugMode) Serial.println();
    }

    // Now go through all the stops and sort the times
    // Since we have multiple viable buses per stop

    memset(dispValues, 0, sizeof(dispValues)); // Clear previous values

    // Go through all results to collect them into stops
    for (byte i = 0; i < NUM_STOPS; i++) {

      for (byte j = 0; j < numberPredictions; j++) {
        bool inserted = insertIntoArray(stops[i].minutes[j],
                                        dispValues[stops[i].displayID],
                                        numberPredictions);
        if (!inserted) break;
        // Break if can't insert value, since the future values are only larger
      }
    }

    // Now we need to display these values
    // Plan is to use 7-seg displays. Use decimal to indicate +10, e.g. 1. = 11
    // Hence why we are only interested in four predictions (one per digit)
    // that are below 20 (can only indicate 19 max)

    if (debugMode || true) {
      Serial.println("Compiled list. Display 0:");
      for (byte i = 0; i < numberPredictions; Serial.println(dispValues[0][i++]));
      Serial.println("Display 1:");
      for (byte i = 0; i < numberPredictions; Serial.println(dispValues[1][i++]));
    }
    Serial.println("=========================================");


    totalTime += millis() - startTime; // Adds current cycle time in ms
    cycleCount++;
    averageCycleTime = totalTime / cycleCount; // Gets new average
    Serial.print("Average cycle time: ");
    Serial.println(averageCycleTime);

    delay(2500);
  }

  delay(200);
}
