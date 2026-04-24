#include <Arduino_FreeRTOS.h>
#include <semphr.h>

// Pin definitions
const int led1Pin = 8;
const int led2Pin = 7;
const int fanPin  = 3;
const int buttonPin = 2;

// LED timing (shared with serial task — protected by mutex)
volatile unsigned long interval1 = 0;
volatile unsigned long interval2 = 0;

// Fan state
const int fanSpeeds[]  = {0, 26, 38, 64};
const int numFanSteps  = 4;
volatile int fanStep   = 0;

// Serial input state
volatile int  selectedLED        = 0;
volatile bool waitingForInterval = false;
String serialBuffer = "";

// Mutex to protect shared serial state
SemaphoreHandle_t xSerialMutex;

// Task handles
TaskHandle_t hSerial, hLED1, hLED2, hFan;

// ── Task prototypes 
void Task_Serial(void *pvParameters);
void Task_LED1  (void *pvParameters);
void Task_LED2  (void *pvParameters);
void Task_Fan   (void *pvParameters);

//  Setup 
void setup() {
  pinMode(led1Pin,  OUTPUT);
  pinMode(led2Pin,  OUTPUT);
  pinMode(fanPin,   OUTPUT);
  pinMode(buttonPin, INPUT);

  analogWrite(fanPin, 0);

  Serial.begin(9600);
  while (!Serial);

  Serial.println("Initial state: Button un-pressed");
  Serial.println("Initial state: Fan off");
  Serial.println("");
  Serial.println("What LED? (1 or 2)");

  xSerialMutex = xSemaphoreCreateMutex();

  // Create tasks
  // Priority 2 = Serial (highest — must not miss input)
  // Priority 1 = LEDs and Fan (equal, preemptable)
  xTaskCreate(Task_Serial, "Serial", 256, NULL, 2, &hSerial);
  xTaskCreate(Task_LED1,   "LED1",   128, NULL, 1, &hLED1);
  xTaskCreate(Task_LED2,   "LED2",   128, NULL, 1, &hLED2);
  xTaskCreate(Task_Fan,    "Fan",    128, NULL, 1, &hFan);

  // Scheduler starts automatically after setup() in Arduino FreeRTOS
}

//  Loop unused under FreeRTOS 
void loop() {
  // FreeRTOS scheduler takes over; loop() is not called
}

// ── Task 1: Serial Input (non-blocking, char-by-char) 
void Task_Serial(void *pvParameters) {
  (void) pvParameters;

  for (;;) {
    if (Serial.available() > 0) {
      char c = Serial.read();

      if (c == '\n' || c == '\r') {
        serialBuffer.trim();

        if (serialBuffer.length() > 0) {
          int value = serialBuffer.toInt();
          serialBuffer = "";

          if (xSemaphoreTake(xSerialMutex, portMAX_DELAY) == pdTRUE) {
            if (!waitingForInterval) {
              selectedLED = value;
              if (selectedLED == 1 || selectedLED == 2) {
                Serial.println("What interval (in msec)?");
                waitingForInterval = true;
              } else {
                Serial.println("Invalid LED. Enter 1 or 2.");
                Serial.println("What LED? (1 or 2)");
              }
            } else {
              unsigned long interval = (unsigned long)value;
              if (interval > 0) {
                if (selectedLED == 1) interval1 = interval;
                else                  interval2 = interval;
                Serial.println("What LED? (1 or 2)");
              }
              waitingForInterval = false;
            }
            xSemaphoreGive(xSerialMutex);
          }
        }
      } else {
        serialBuffer += c;
      }
    }

    vTaskDelay(1); // yield — 1 tick (~15ms on Uno with FreeRTOS defaults)
  }
}

// ── Task 2: LED1 blink 
void Task_LED1(void *pvParameters) {
  (void) pvParameters;

  for (;;) {
    unsigned long iv = interval1;
    if (iv > 0) {
      digitalWrite(led1Pin, HIGH);
      vTaskDelay(pdMS_TO_TICKS(iv / 2));
      digitalWrite(led1Pin, LOW);
      vTaskDelay(pdMS_TO_TICKS(iv / 2));
    } else {
      digitalWrite(led1Pin, LOW);
      vTaskDelay(10);
    }
  }
}

// ── Task 3: LED2 blink 
void Task_LED2(void *pvParameters) {
  (void) pvParameters;

  for (;;) {
    unsigned long iv = interval2;
    if (iv > 0) {
      digitalWrite(led2Pin, HIGH);
      vTaskDelay(pdMS_TO_TICKS(iv / 2));
      digitalWrite(led2Pin, LOW);
      vTaskDelay(pdMS_TO_TICKS(iv / 2));
    } else {
      digitalWrite(led2Pin, LOW);
      vTaskDelay(10);
    }
  }
}

// ── Task 4: Fan button — cycles OFF > LOW > MEDIUM > HIGH > OFF 
void Task_Fan(void *pvParameters) {
  (void) pvParameters;
  bool lastButtonState = LOW;
  const TickType_t debounce = pdMS_TO_TICKS(200);

  for (;;) {
    bool reading = digitalRead(buttonPin);

    if (reading == HIGH && lastButtonState == LOW) {
      fanStep = (fanStep + 1) % numFanSteps;
      analogWrite(fanPin, fanSpeeds[fanStep]);

      switch (fanStep) {
        case 0: Serial.println("Fan off");       break;
        case 1: Serial.println("Fan on low");    break;
        case 2: Serial.println("Fan on medium"); break;
        case 3: Serial.println("Fan on high");   break;
      }

      vTaskDelay(debounce); // hold off for debounce period
    }

    lastButtonState = reading;
    vTaskDelay(1);
  }
}