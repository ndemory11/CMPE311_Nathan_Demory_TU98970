#include <Arduino_FreeRTOS.h>
#include <semphr.h>
#include <EEPROM.h>

// ── Pin definitions ───────────────────────────────────────────────────────────
const int led1Pin   = 8;
const int led2Pin   = 7;
const int fanPin    = 3;
const int buttonPin = 2;

// ── LED timing ────────────────────────────────────────────────────────────────
volatile unsigned long interval1 = 0;
volatile unsigned long interval2 = 0;

// ── Fan state ─────────────────────────────────────────────────────────────────
const int fanSpeeds[] = {0, 26, 38, 64};
const int numFanSteps = 4;
volatile int fanStep  = 0;

// ── Serial input state ────────────────────────────────────────────────────────
volatile int  selectedLED        = 0;
volatile bool waitingForInterval = false;
String serialBuffer = "";

// ── EEPROM Data Frame Management ──────────────────────────────────────────────
#define FRAME_SIZE   256
#define FRAME_COUNT  3

const int frameAddresses[FRAME_COUNT] = {0, 256, 512};

SemaphoreHandle_t xFrameSemaphore;
SemaphoreHandle_t xSerialMutex;
SemaphoreHandle_t xFrameMutex;

volatile int frameOwner[FRAME_COUNT] = {-1, -1, -1};

TaskHandle_t hSerial, hLED1, hLED2, hFan;

// ── Task prototypes ───────────────────────────────────────────────────────────
void Task_Serial(void *pvParameters);
void Task_LED1  (void *pvParameters);
void Task_LED2  (void *pvParameters);
void Task_Fan   (void *pvParameters);

// ── EEPROM Frame API ──────────────────────────────────────────────────────────
int acquireFrame(int taskId) {
  if (xSemaphoreTake(xFrameSemaphore, portMAX_DELAY) == pdTRUE) {
    if (xSemaphoreTake(xFrameMutex, portMAX_DELAY) == pdTRUE) {
      for (int i = 0; i < FRAME_COUNT; i++) {
        if (frameOwner[i] == -1) {
          frameOwner[i] = taskId;
          xSemaphoreGive(xFrameMutex);
          return i;
        }
      }
      xSemaphoreGive(xFrameMutex);
    }
  }
  return -1;
}

void releaseFrame(int frameIndex) {
  if (frameIndex < 0 || frameIndex >= FRAME_COUNT) return;
  if (xSemaphoreTake(xFrameMutex, portMAX_DELAY) == pdTRUE) {
    frameOwner[frameIndex] = -1;
    xSemaphoreGive(xFrameMutex);
  }
  xSemaphoreGive(xFrameSemaphore);
}

void writeFrame(int frameIndex, const uint8_t *data, int len) {
  if (frameIndex < 0 || frameIndex >= FRAME_COUNT) return;
  int base = frameAddresses[frameIndex];
  int writeLen = min(len, FRAME_SIZE);
  for (int i = 0; i < writeLen; i++) {
    EEPROM.update(base + i, data[i]);
  }
}

void readFrame(int frameIndex, uint8_t *buf, int len) {
  if (frameIndex < 0 || frameIndex >= FRAME_COUNT) return;
  int base = frameAddresses[frameIndex];
  int readLen = min(len, FRAME_SIZE);
  for (int i = 0; i < readLen; i++) {
    buf[i] = EEPROM.read(base + i);
  }
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
  pinMode(led1Pin,   OUTPUT);
  pinMode(led2Pin,   OUTPUT);
  pinMode(fanPin,    OUTPUT);
  pinMode(buttonPin, INPUT);

  analogWrite(fanPin, 0);

  Serial.begin(9600);
  while (!Serial);

  Serial.println("Initial state: Button un-pressed");
  Serial.println("Initial state: Fan off");
  Serial.println("");
  Serial.println("What LED? (1 or 2)");

  xSerialMutex    = xSemaphoreCreateMutex();
  xFrameMutex     = xSemaphoreCreateMutex();
  xFrameSemaphore = xSemaphoreCreateCounting(FRAME_COUNT, FRAME_COUNT);

  xTaskCreate(Task_Serial, "Serial", 256, NULL, 2, &hSerial);
  xTaskCreate(Task_LED1,   "LED1",   192, NULL, 1, &hLED1);
  xTaskCreate(Task_LED2,   "LED2",   192, NULL, 1, &hLED2);
  xTaskCreate(Task_Fan,    "Fan",    192, NULL, 1, &hFan);
}

// ── Loop — unused under FreeRTOS ──────────────────────────────────────────────
void loop() {}

// ── Task 1: Serial Input ──────────────────────────────────────────────────────
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

    vTaskDelay(1);
  }
}

// ── Task 2: LED1 blink ────────────────────────────────────────────────────────
void Task_LED1(void *pvParameters) {
  (void) pvParameters;

  for (;;) {
    unsigned long iv = interval1;

    if (iv > 0) {
      int frame = acquireFrame(1);
      if (frame >= 0) {
        uint8_t buf[4];
        buf[0] = (iv >> 24) & 0xFF;
        buf[1] = (iv >> 16) & 0xFF;
        buf[2] = (iv >>  8) & 0xFF;
        buf[3] = (iv      ) & 0xFF;
        writeFrame(frame, buf, 4);
        releaseFrame(frame);
      }

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

// ── Task 3: LED2 blink ────────────────────────────────────────────────────────
void Task_LED2(void *pvParameters) {
  (void) pvParameters;

  for (;;) {
    unsigned long iv = interval2;

    if (iv > 0) {
      int frame = acquireFrame(2);
      if (frame >= 0) {
        uint8_t buf[4];
        buf[0] = (iv >> 24) & 0xFF;
        buf[1] = (iv >> 16) & 0xFF;
        buf[2] = (iv >>  8) & 0xFF;
        buf[3] = (iv      ) & 0xFF;
        writeFrame(frame, buf, 4);
        releaseFrame(frame);
      }

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

// ── Task 4: Fan button ────────────────────────────────────────────────────────
void Task_Fan(void *pvParameters) {
  (void) pvParameters;
  bool lastButtonState = LOW;
  const TickType_t debounce = pdMS_TO_TICKS(200);

  for (;;) {
    bool reading = digitalRead(buttonPin);

    if (reading == HIGH && lastButtonState == LOW) {
      fanStep = (fanStep + 1) % numFanSteps;
      analogWrite(fanPin, fanSpeeds[fanStep]);

      int frame = acquireFrame(3);
      if (frame >= 0) {
        uint8_t buf[1] = { (uint8_t)fanStep };
        writeFrame(frame, buf, 1);
        releaseFrame(frame);
      }

      switch (fanStep) {
        case 0: Serial.println("Fan off");       break;
        case 1: Serial.println("Fan on low");    break;
        case 2: Serial.println("Fan on medium"); break;
        case 3: Serial.println("Fan on high");   break;
      }

      vTaskDelay(debounce);
    }

    lastButtonState = reading;
    vTaskDelay(1);
  }
}