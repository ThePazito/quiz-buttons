#include <LiquidCrystal.h>

const int xPin = A0;
const int yPin = A1;
const int buttonPin = 2;
const int contrastPin = 10;

const int gamePowerSupplyPin = 9;

int contrastValue = 100;

int xValue;
int yValue;

int buttonState;

int whatModeIsSelected = 0;

LiquidCrystal lcd_1(12, 11, 7, 6, 5, 4);

void setup() {
  // For debugging
  Serial.begin(9600);

  pinMode(xPin, INPUT);
  pinMode(yPin, INPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  pinMode(contrastPin, OUTPUT);
  analogWrite(contrastPin, contrastValue);

  pinMode(gamePowerSupplyPin, OUTPUT);
  digitalWrite(gamePowerSupplyPin, LOW);   // default OFF

  lcd_1.begin(16, 2);
  lcd_1.print(" Start  |  Stop ");
  lcd_1.setCursor(0, 1);
  lcd_1.print(" -----          ");
}

void loop() {
  // put your main code here, to run repeatedly:
  yValue = analogRead(yPin);

  buttonState = digitalRead(buttonPin); // LOW = pressed (pullup)

  // Mode selection
  if (yValue > 923 && whatModeIsSelected != 0) { 
    // Left
    whatModeIsSelected = 0;
    lcd_1.setCursor(0, 1);
    lcd_1.print(" -----          ");
  } else if (yValue < 100 && whatModeIsSelected != 1) { 
    // Right
    whatModeIsSelected = 1;
    lcd_1.setCursor(0, 1);
    lcd_1.print("           ---- ");
  }

  static unsigned long lastPressMs = 0;
  if (buttonState == LOW && (millis() - lastPressMs) > 200) {
    lastPressMs = millis();
    if (whatModeIsSelected == 0) {
      // Start
      digitalWrite(gamePowerSupplyPin, HIGH);

      // Debugging
      Serial.println("Start -> HIGH to other Arduino");
    } else {
      // Reset
      digitalWrite(gamePowerSupplyPin, LOW);

      // Debugging
      Serial.println("Reset -> LOW to other Arduino");
    }
  }

  delay(30);
}