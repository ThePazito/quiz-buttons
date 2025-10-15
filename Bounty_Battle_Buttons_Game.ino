#include <TM1637Display.h>

// TM1637 Display pins
#define TM_CLK 7
#define TM_DIO 6
TM1637Display display(TM_CLK, TM_DIO);

// Pins
const int powerSupplyInputPin  = 12;  // enable from other Arduino (HIGH = enabled)
const int powerSupplyOutputPin = 8;   // optional mirror/debug LED

const int buzzerPin        = 10;
const int encoderBtnPin    = 3;  // encoder push switch (to GND)
const int encoderPinB      = 4;  // encoder DT
const int encoderPinA      = 5;  // encoder CLK

// 4 game buttons -> pins to GND (INPUT_PULLUP)
const int buttonPins[4] = {A1, A2, A3, A4};

// States
bool buttonLatched[4]   = {false, false, false, false}; // one press per round
unsigned long lastMs[4] = {0,0,0,0};
int  prevState[4]       = {HIGH, HIGH, HIGH, HIGH};      // for edge detection
const unsigned long DEBOUNCE = 25; // shorter debounce to catch quick taps

int soundVolume    = 400;  // Hz
int lastA          = HIGH; // encoder A last state

// For messaging with Py
String rxLine = "";

// Simple non-blocking beep
bool beeping = false;
unsigned long beepUntil = 0;
const unsigned long BEEP_MS = 80;

// A4 (440 Hz) up to G#7 (~3322 Hz)
const int NOTE_COUNT = 37;
const int noteFreqs[NOTE_COUNT] = {
  0,
  440, 466, 494, 523, 554, 587, 622, 659, 698, 740, 784, 831,
  880, 932, 988, 1046,1109,1175,1245,1319,1397,1480,1568,1661,
  1760,1865,1976,2093,2217,2349,2489,2637,2794,2960,3136,3322
};
int noteIdx = 0;  // start at A4

// Order tracking for display
uint8_t orderDigits[4] = {0,0,0,0};

void showOrder() {
  uint8_t segs[4];
  for (int i = 0; i < 4; i++) {
    segs[i] = orderDigits[i] ? display.encodeDigit(orderDigits[i]) : 0x00;
  }
  display.setSegments(segs);
}

void clearDisplayOnly() {
  for (int i = 0; i < 4; i++) {
    orderDigits[i] = 0;
  }
  display.clear();
}

void clearOrder() {
  for (int i = 0; i < 4; i++) {
    orderDigits[i] = 0;
    buttonLatched[i] = false;
    prevState[i] = HIGH;
    lastMs[i] = 0;
  }
  display.clear();
}

void shortBeep() {
  tone(buzzerPin, soundVolume);
  beeping = true;
  beepUntil = millis() + BEEP_MS;
}

void serviceBeep() {
  if (beeping && millis() >= beepUntil) {
    noTone(buzzerPin);
    beeping = false;
  }
}

// Parse a line like: ORDER:1,3,4,2
void processSerialLine(const String& line) {
  if (line.startsWith("ORDER:")) {
    String data = line.substring(6); // after "ORDER:"

    clearDisplayOnly();

    // Fill orderDigits[] from CSV
    int idx = 0;
    int start = 0;
    while (idx < 4 && start < (int)data.length()) {
      int comma = data.indexOf(',', start);
      String tok = (comma == -1) ? data.substring(start) : data.substring(start, comma);
      tok.trim();
      int val = tok.toInt(); // 0 if invalid
      if (val >= 1 && val <= 4) {
        orderDigits[idx++] = (uint8_t)val;
      }
      if (comma == -1) break;
      start = comma + 1;
    }
    showOrder();
  }
}

// Non-blocking line reader: accumulates until \n or \r
void pollSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (rxLine.length() > 0) {
        processSerialLine(rxLine);
        rxLine = "";
      }
    } else {
      rxLine += c;
      if (rxLine.length() > 80) rxLine = ""; // safety
    }
  }
}

void setup() {
  Serial.begin(9600);

  pinMode(powerSupplyInputPin, INPUT);
  pinMode(powerSupplyOutputPin, OUTPUT);
  digitalWrite(powerSupplyOutputPin, LOW);

  pinMode(buzzerPin, OUTPUT);

  // Encoder
  pinMode(encoderPinA,   INPUT_PULLUP);
  pinMode(encoderPinB,   INPUT_PULLUP);
  pinMode(encoderBtnPin, INPUT_PULLUP);
  lastA = digitalRead(encoderPinA);

  // Game buttons
  for (int i=0; i<4; i++) pinMode(buttonPins[i], INPUT_PULLUP);

  display.setBrightness(7, true);
  clearOrder();
}

void loop() {
  const unsigned long now = millis();

  // Enable line from other Arduino
  const int incoming = digitalRead(powerSupplyInputPin);
  digitalWrite(powerSupplyOutputPin, incoming ? LOW : HIGH);

  // Encoder rotation: adjust tone frequency (non-blocking)
  int a = digitalRead(encoderPinA);
  if (a != lastA) {
    if (digitalRead(encoderPinB) != a) noteIdx--; else noteIdx++;
    if (noteIdx < 0) noteIdx = 0;
    if (noteIdx >= NOTE_COUNT) noteIdx = NOTE_COUNT - 1;
    int freq = noteFreqs[noteIdx];
    soundVolume = freq;
    if (digitalRead(encoderBtnPin) == LOW) tone(buzzerPin, freq);
  }
  lastA = a;

  // Encoder push: tone while held
  if (digitalRead(encoderBtnPin) == LOW) {
    tone(buzzerPin, soundVolume);
  } else if (!beeping) {
    noTone(buzzerPin);
  }

  // ----- NON-BLOCKING BUTTON SCAN -----
  if (incoming == HIGH) {
    for (int i=0; i<4; i++) {
      int s = digitalRead(buttonPins[i]); // LOW when pressed

      // rising/falling edges + debounce window
      if (prevState[i] == HIGH && s == LOW) {           // edge: just pressed
        if ((now - lastMs[i]) > DEBOUNCE && !buttonLatched[i]) {
          lastMs[i] = now;
          buttonLatched[i] = true;

          Serial.print(F("BUZZ:"));
          Serial.println(i + 1);

          shortBeep();
        }
      }
      prevState[i] = s;
    }
  }

  // Reset round on rising edge of enable
  static int prevIncoming = LOW;
  if (prevIncoming == LOW && incoming == HIGH) {
    clearOrder();
    shortBeep();

    // Notify Pi to clear its table
    Serial.println("ROUND:RESET");
  }
  if (prevIncoming == HIGH && incoming == LOW) {
    shortBeep();
  }
  prevIncoming = incoming;

  // Maintain beeps without blocking
  pollSerial();
  serviceBeep();
}