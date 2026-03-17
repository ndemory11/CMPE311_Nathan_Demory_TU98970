
const int led1Pin = 5;
const int led2Pin = 6;

bool led1State = LOW;
bool led2State = LOW;


unsigned long previousMillis1 = 0;
unsigned long previousMillis2 = 0;

unsigned long interval1 = 0;
unsigned long interval2 = 0;

//  Serial Control 
int selectedLED = 0;
bool waitingForInterval = false;

// Scheduler 
#define NUM_TASKS 3

void Task_Serial();
void Task_LED1();
void Task_LED2();

// Function pointer array 
void (*taskList[NUM_TASKS])() = {
  Task_Serial,
  Task_LED1,
  Task_LED2
};

int currentTask = 0;

 //SETUP

void setup() {
  pinMode(led1Pin, OUTPUT);
  pinMode(led2Pin, OUTPUT);

  Serial.begin(9600);
  while (!Serial);

  Serial.println("What LED? (1 or 2)");
}


//  CYCLIC EXEC 

void loop() {
  // Execute ONE task per loop (round-robin)
  taskList[currentTask]();

  // Move to next task
  currentTask = (currentTask + 1) % NUM_TASKS;
}


//  TASKS 


// Task 1: Serial Input 
void Task_Serial() {
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
        if (selectedLED == 1) interval1 = interval;
        else interval2 = interval;

        Serial.println("What LED? (1 or 2)");
      }

      waitingForInterval = false;
    }

    while (Serial.available()) Serial.read();
  }
}

// Task 2: LED1 
void Task_LED1() {
  if (interval1 > 0) {
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis1 >= interval1 / 2) {
      previousMillis1 = currentMillis;
      led1State = !led1State;
      digitalWrite(led1Pin, led1State);
    }
  }
}

// Task 3: LED2 
void Task_LED2() {
  if (interval2 > 0) {
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis2 >= interval2 / 2) {
      previousMillis2 = currentMillis;
      led2State = !led2State;
      digitalWrite(led2Pin, led2State);
    }
  }
}