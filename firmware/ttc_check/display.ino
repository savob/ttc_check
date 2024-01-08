//=======================================================================================
// Display functions

void displaySetup() {
  // Shift Register Pins
  pinMode(displayDataPin, OUTPUT);
  pinMode(displayDataClockPin, OUTPUT);
  pinMode(displayOutputClockPin, OUTPUT);
  pinMode(displayEnablePin, OUTPUT);

  digitalWrite(displayDataPin, LOW);
  digitalWrite(displayDataClockPin, LOW);
  digitalWrite(displayOutputClockPin, LOW);
  digitalWrite(displayEnablePin, HIGH); // By default start with display enabled

  // Digit Driving MOSFETs
  for (byte i = 0; i < 4; i++) {
    pinMode(digitPins[i], OUTPUT);
    digitalWrite(digitPins[i], LOW);
  }

  // Use 1st timer of 4 (counted from zero).
  // Set 80 divider for prescaler (see ESP32 Technical Reference Manual for more
  // info).
  timer = timerBegin(3, 80, true);

  // Attach onTimer function to our timer.
  timerAttachInterrupt(timer, &onTimer, true);

  // Set alarm to call onTimer function every few milliseconds (value in microseconds).
  // Repeat the alarm (third parameter)
  timerAlarmWrite(timer, 2500, true);

  // Start an alarm
  timerAlarmEnable(timer);

  for (byte i = 0; i < 4; i++) {
    displayBuffer[i] = 0xFF; // Illuminate all segments
  }

  delay(1000); // Allow us to see all segments are on

  for (byte number = 0; number < 10; number++) {
    for (byte i = 0; i < 4; i++) {
      displayBuffer[i] = numberCharacters[number]; // Illuminate all segments
    }

    delay(200); // Allow us to see all segments are on
  }

  blankDisplay();

  return;
}



void dispDigit(const byte digit, const byte segments) {

  // Set segments
  for (byte i = 0; i < 8; i++) {
    digitalWrite(displayDataPin, bitRead(segments, i));
    delayMicroseconds(10);
    digitalWrite(displayDataClockPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(displayDataClockPin, LOW);
  }

  // Turn off all digits
  for (byte i = 0; i < 4; i++) {
    digitalWrite(digitPins[i], LOW);
  }

  // Set the shift register output
  digitalWrite(displayOutputClockPin, HIGH);
  delayMicroseconds(5);
  digitalWrite(displayOutputClockPin, LOW);

  // Turn on needed digit
  digitalWrite(digitPins[digit], HIGH);

  return;
}

void blankDisplay() {
  for (byte i = 0; i < 4; i++) {
    displayBuffer[i] = 0; // Clear display
  }
  return;
}


void displayDashes() {
  for (byte i = 0; i < 4; i++) {
    displayBuffer[i] = dashCharacter; // Clear display
  }
  return;
}

void displayPredictions (int predictions[]) {
  if (predictions[0] == 0) {
    // If it starts with a zero, nothing was found, show "NoNE"
    displayBuffer[0] = 0x4F;
    displayBuffer[1] = 0xD8;
    displayBuffer[2] = 0x4F;
    displayBuffer[3] = 0x9B;
  }
  else {
    // There are some to show

    for (byte i = 0; i < NUMBER_PREDICTIONS; i++) {
      displayBuffer[i] = numberCharacters[predictions[i]]; // Copy temp values to display
      if (predictions[i] == 0) displayBuffer[i] = 0; // Blank any trailing zeros
    }
  }

  log_v("Display buffer for results:");
  for (byte i = 0; i < NUMBER_PREDICTIONS; i++) log_v("Display %d: 0x%X", i, displayBuffer[i]);

  return;
}
