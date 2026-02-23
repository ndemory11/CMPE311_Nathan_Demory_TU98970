// Pin definitions
const int led1Pin = 6;
const int led2Pin = 5;

// Timing variables
unsigned long previousMillis1 = 0;
unsigned long previousMillis2 = 0;

unsigned long interval1 = 0;   
unsigned long interval2 = 0;

bool led1State = LOW;
bool led2State = LOW;

// Serial input control
int selectedLED = 0;
bool waitingForInterval = false;

void setup() {
  pinMode(led1Pin, OUTPUT);
  pinMode(led2Pin, OUTPUT);

  Serial.begin(9600);
  while (!Serial);

  Serial.println("What LED? (1 or 2)");
}

void loop() {

  //  Handle Serial Input 
  if (Serial.available() > 0) {

    if (!waitingForInterval) {
      selectedLED = Serial.parseInt();

      if (selectedLED == 1 || selectedLED == 2) {
        Serial.println("What interval (in msec)?");
        waitingForInterval = true;
      } else {
        Serial.println("Invalid LED. Enter 1 or 2.");
      }
    }
    else {
      unsigned long interval = Serial.parseInt();

      if (interval > 0) {
        if (selectedLED == 1) {
          interval1 = interval;
        } else {
          interval2 = interval;
        }

        Serial.println("What LED? (1 or 2)");
      }

      waitingForInterval = false;
    }

    // Clear serial buffer
    while (Serial.available()) Serial.read();
  }

  // -LED 1 
  if (interval1 > 0) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis1 >= interval1 / 2) {
      previousMillis1 = currentMillis;
      led1State = !led1State;
      digitalWrite(led1Pin, led1State);
    }
  }

  //  LED 2 Blinking 
  if (interval2 > 0) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis2 >= interval2 / 2) {
      previousMillis2 = currentMillis;
      led2State = !led2State;
      digitalWrite(led2Pin, led2State);
    }
  }
} 