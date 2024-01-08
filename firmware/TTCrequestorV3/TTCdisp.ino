const byte pinSeg[] = {23, 1, 21, 18, 5, 22, 3, 19}; //A-G,dp
const byte pinDig[] = {4, 16, 17, 2};
const bool digOn = HIGH; // What state to have pins in to turn a digit on
const bool segOn = HIGH;   // What state to have pins in to turn on a segment

volatile byte currentDigit = 0; // Stores current digit for use in timer interrupt

// Character array
byte segmentsShown[22] = {B00111111, B00000110, B01011011, B01001111, B01100110, B01101101, B01111101, B00000111, B01111111, B01101111}; // Numbers in segment form A to G, lsb to msb
// [20] is used for blanks so should be kept at 0
// [21] is used for a dash

hw_timer_t * timer = NULL;

void IRAM_ATTR onTimer() {
  // Timer interupt, displays a digit on screen
  dispDigit(currentDigit, segmentsShown[displayBuffer[currentDigit]]);

  if (currentDigit == 3) currentDigit = 0; //Reset
  else currentDigit++;
}

void displaySetup() {
  // Sets output pins
  for (int i = 0; i < 4; i++) {
    pinMode(pinDig[i], OUTPUT);
  }
  for (int i = 0; i < 8; i++) {
    pinMode(pinSeg[i], OUTPUT);
  }

  // Adds decimals to numbers array by appending it with the digit with decimal
  // E.g. segmentsShown[1] is the chartacter for 1, segmentsShown[11] is 1 with a deciaml point
  for (byte i = 0; i < 10; i ++) segmentsShown[10 + i] = segmentsShown[i] | B10000000;
  segmentsShown[21] = B01000000; // Dash
  //==========================================================
  // Timer

  // Use 1st timer of 4 (counted from zero).
  // Set 80 divider for prescaler (see ESP32 Technical Reference Manual for more info).
  timer = timerBegin(0, 80, true);

  // Attach onTimer function to our timer.
  timerAttachInterrupt(timer, &onTimer, true);

  // Set alarm to call onTimer function every millisecond (value in microseconds).
  // Repeat the alarm (third parameter)
  timerAlarmWrite(timer, 1000, true);

  // Start an alarm
  timerAlarmEnable(timer);
}

void dispDigit(const byte digit, const byte segments) {

  // Set segments
  for (int i = 0; i < 8; i++) {
    digitalWrite(pinSeg[i], bitRead(segments, i) ? segOn : !segOn);
  }

  // Turn off all digits
  for (int i = 0; i < 4; i++) {
    digitalWrite(pinDig[i], !digOn);
  }

  // Turn on needed digit
  digitalWrite(pinDig[digit], digOn);
}

void blankDisplay() {
  for (byte i = 0; i < NUMBER_PREDICTIONS; displayBuffer[i++] = 20);
  displayPredictions();
}

void dashDisplay() {
  for (byte i = 0; i < NUMBER_PREDICTIONS; displayBuffer[i++] = 21);
  displayPredictions();
}
