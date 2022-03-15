const byte pinSeg[] = {23, 1, 21, 18, 5, 22, 3, 19}; //A-G,dp
const byte pinDig[] = {4, 16, 17, 2};
const bool digOn = HIGH; // What state to have pins in to turn a digit on
const bool segOn = HIGH;   // What state to have pins in to turn on a segment

int buff[4]; // Display buffer
volatile byte currentDigit = 0; // Stores current digit for use in timer interrupt

int temp = 0;

hw_timer_t * timer = NULL;

void IRAM_ATTR onTimer() {
  dispDigit(currentDigit, convertNum(buff[currentDigit]));

  if (currentDigit == 3) currentDigit = 0; //Reset
  else currentDigit++;
}

void setup() {
  for (int i = 0; i < 4; i++) {
    pinMode(pinDig[i], OUTPUT);
  }
  for (int i = 0; i < 8; i++) {
    pinMode(pinSeg[i], OUTPUT);
  }

  // Preparing the lookup tables
  // Messing around with numbers
  addDecimals(); // Create the number constrants with decimals


  // Use 1st timer of 4 (counted from zero).
  // Set 80 divider for prescaler (see ESP32 Technical Reference Manual for more
  // info).
  timer = timerBegin(3, 80, true);

  // Attach onTimer function to our timer.
  timerAttachInterrupt(timer, &onTimer, true);

  // Set alarm to call onTimer function every millisecond (value in microseconds).
  // Repeat the alarm (third parameter)
  timerAlarmWrite(timer, 1000, true);

  // Start an alarm
  timerAlarmEnable(timer);
}

void loop() {
  temp = millis() / 100;

  // Break down number by digit, starting at thousands digit.
  int base = 1000;
  for (byte i = 0; i < 4; i++) {
    buff[i] = temp / base;
    temp = temp % base;
    base /= 10;
  }
  delay(100);
}
