/*
   Note: there is no error checking/correction for inputs

   This code works with the seven segment display. It assumes
   the pins controlling it are sequential all offset by startPin
   The first seven are the segments as shown below:
             -A-
            F   B
             -G-
            E   C
             -D- (dp)
   The eigth pin is that decimal point, then the four for each
   digit (sequencially), and then the semicolon (digit 5)
*/

byte numbers[20] = {B00111111, B00000110, B01011011, B01001111, B01100110, B01101101, B01111101, B00000111, B01111111, B01101111}; // Numbers in segment form A to G, lsb to msb

void addDecimals() {
  // Adds decimals to numbers array by appending it with the digit with decimal
  // E.g. numbers[1] is the chartacter for 1, numbers[11] is 1 with a deciaml point
  
  for (byte i = 0; i < 10; i ++) numbers[10 + i] = numbers[i] | B10000000;
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

byte convertNum(byte number) {
  // Number segment powering
  byte numbers[16] = {B00111111, B00000110, B01011011, B01001111, B01100110, B01101101, B01111101, B00000111, B01111111, B01101111}; // Numbers in segment form A to G, lsb to msb
  return numbers[number];
}
