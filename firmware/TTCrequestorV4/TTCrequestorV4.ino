/*
    Based on TinyXML NextBus example

   See the manual on how to configure this at:
   https://www.nextbus.com/xmlFeedDocs/NextBusXMLFeed.pdf
*/

#include <WiFi.h>

// CONFIGURABLE GLOBAL STUFF -----------------------------------------------
#define DEBUG_MODE // Toggle serial and debug/detailed messages, follow this with "#undef DEBUG_MODE"
#undef DEBUG_MODE

char ssid[] = "ssid",     // WiFi network name
              pass[] = "password", // WiFi network password
                       host[] = "webservices.nextbus.com";

const int MIN_TIME = 3; // Skip arrivals sooner than this (minutes)
const int MAX_TIME = 20; // Disregard times this long or longer (minutes)
const int READ_TIMEOUT = 15; // Cancel query if no data received (seconds)

const byte NUMBER_BUTTONS = 4;
const byte BUTTON_PIN[NUMBER_BUTTONS] = {25, 26, 27, 14}; // buttons used for stops

const int DISPLAY_TIME = 5000; // Time numbers are visible (ms)

const char keyword[] = "minutes=\"";

WiFiClient client;
const byte NUMBER_PREDICTIONS = 4; // Number of preditions to maintain per stop
int displayBuffer[NUMBER_PREDICTIONS]; // Stores digits to display
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
uint8_t    stopIndex = NUM_STOPS; // Stop # currently being searched

#ifdef DEBUG_MODE
// Used for testing average duration of code sections when debugging
int cycleCount = 0;
long startTime = 0;
float averageCycleTime = 0;
long totalTime = 0;
#endif

void setup(void) {
#ifdef DEBUG_MODE
  Serial.begin(115200);
  Serial.println("NextBus Tracker");
#endif

  connectToWifi();
  displaySetup();

  for (byte i = 0; i < NUMBER_BUTTONS; i++) pinMode(BUTTON_PIN[i], INPUT_PULLUP); // Set button to be pulled up
  blankDisplay();
}

// MAIN LOOP ---------------------------------------------------------------

void loop(void) {
  uint32_t t;
  boolean  timedOut;

  yield(); // Handle WiFi events

  // Check WiFi connection, attempt connection if disconnected
  if (WiFi.status() != WL_CONNECTED) {
    connectToWifi();
  }

  for (byte buttonIndex = 0; buttonIndex < NUMBER_BUTTONS; buttonIndex++) {
    if (digitalRead(BUTTON_PIN[buttonIndex]) == LOW) {
      // Pressed button
#ifdef DEBUG_MODE
      Serial.print("Button pressed on pin ");
      Serial.print(BUTTON_PIN[buttonIndex]);
      Serial.print(", buttonIndex = ");
      Serial.println(buttonIndex);

      startTime = millis(); // Used for time benchmarks
#endif

      dashDisplay(); // Clear display values
      int tempValues[NUMBER_PREDICTIONS]; // Used to store temporary values to be displayed
      for (byte i = 0; i < NUMBER_PREDICTIONS; tempValues[i++] = 0); // Clear values

      for (stopIndex = 0; stopIndex < NUM_STOPS; stopIndex++) {
        if (stops[stopIndex].buttonID == buttonIndex) {
#ifdef DEBUG_MODE
          Serial.print("Stop #");
          Serial.print(stops[stopIndex].stopID);
          Serial.print(" Route: ");
          Serial.println(stops[stopIndex].route);
          Serial.print("Contacting server...");
#endif
          // Clear predictions
          for (byte i = 0; i < NUMBER_PREDICTIONS; stops[stopIndex].minutes[i++] = 0);

          if (client.connect(host, 80)) {
#ifdef DEBUG_MODE
            Serial.println("OK\r\nRequesting data...");
#endif
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


#ifdef DEBUG_MODE
            Serial.println(request);
#endif

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
#ifdef DEBUG_MODE
                Serial.println("---Timeout---");
#endif
                timedOut = true;
                break;
              }
            }

            // Go through existing times to see if our results can be added to the list in order
            for (byte j = 0; j < NUMBER_PREDICTIONS; j++) {
              bool inserted = insertIntoArray(stops[stopIndex].minutes[j], tempValues, NUMBER_PREDICTIONS);

              if (!inserted) break;
              // Break if can't insert value, since the future values are only larger
            }
          }
#ifdef DEBUG_MODE
          else Serial.println("Failed to connect.");

          Serial.println();
#endif

          client.stop();
        }


      }

      // Now we need to display these values
      // Plan is to use 7-seg displays. Use decimal to indicate +10, e.g. 1. = 11
      // Hence why we are only interested in four predictions (one per digit)
      // that are below 20 (can only indicate 19 max)
#ifdef DEBUG_MODE
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
#endif

      // Display values
      for (byte i = 0; i < NUMBER_PREDICTIONS; i++) displayBuffer[i] = tempValues[i]; // Copy temp values to display
      displayPredictions();
      delay(DISPLAY_TIME);
      blankDisplay(); //Clear display
    }
  }
  delay(200);
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
  Serial.println(keyword);
  
  while (client.available()) {
    byte charactersMatching;

    // See if it can spot the keyword
    for (charactersMatching = 0; charactersMatching < keywordLength; charactersMatching++) {
#ifdef DEBUG_MODE
      char temp = client.read();
      Serial.print(temp);                   // Print response character by character
      if (temp != keyword[charactersMatching]) break; // Break if there is a mismatch
#endif
#ifndef DEBUG_MODE
      if (client.read() != keyword[charactersMatching]) break; // Break if there is a mismatch
#endif
    }

    // Check if there is a match (all characters matched)
    if (charactersMatching == keywordLength) {
      char characters[2];
      characters[0] = client.read();
      characters[1] = client.read();
      int entry = atoi(characters);
#ifdef DEBUG_MODE
        Serial.print(entry);
        Serial.println(" MINUTES");
#endif

      if ((entry >= MIN_TIME) && (entry < MAX_TIME)) {
        // When within our defined limits, insert the value into sorted list
        insertIntoArray(entry, stops[stopIndex].minutes, NUMBER_PREDICTIONS);

      }
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
  byte count;

#ifdef DEBUG_MODE
  Serial.print("WiFi connecting..");
#endif

  WiFi.begin(ssid, pass);
  // Wait up to 1 minute for connection...
  for (count = 0; (count < 60) && (WiFi.status() != WL_CONNECTED); count++) {
#ifdef DEBUG_MODE
    Serial.write('.');
#endif
    delay(1000);
  }
  if (count >= 60) { // If it didn't connect within 1 min
#ifdef DEBUG_MODE
    Serial.println("Failed. Will retry...");
#endif
    return;
  }
#ifdef DEBUG_MODE
  Serial.println("OK!");
#endif
  delay(2000); // Pause before hitting it with queries & stuff
}

void displayPredictions() {

  // Leave all values but blank trailing zeroes
  for (byte i = 1; i < NUMBER_PREDICTIONS; i++) {
    if (displayBuffer[i] == 0) displayBuffer[i] = 20; // Blank character
  }

#ifdef DEBUG_MODE
  Serial.println("Display characters are:");
  for (byte i = 0; i < NUMBER_PREDICTIONS; Serial.println(displayBuffer[i++]));
#endif
}
