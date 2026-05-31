#include <AccelStepper.h>

// -----------------------------
// Motor Pin Definitions
// -----------------------------
// Motor 1 (Forward/Reverse)
#define M1_STEP 2
#define M1_DIR 3

// Motor 2 (Up/Down)
#define M2_STEP 4
#define M2_DIR 5

// Motor 3 (Extruder)
#define M3_STEP 6
#define M3_DIR 7

// Motor 4 (Y axis)
#define M4_STEP 8
#define M4_DIR 9

// -----------------------------
// Stepper Objects
// -----------------------------
AccelStepper motor1(AccelStepper::DRIVER, M1_STEP, M1_DIR);
AccelStepper motor2(AccelStepper::DRIVER, M2_STEP, M2_DIR);
AccelStepper motor3(AccelStepper::DRIVER, M3_STEP, M3_DIR);
AccelStepper motor4(AccelStepper::DRIVER, M4_STEP, M4_DIR);

// -----------------------------
// Global Stop Flag
// -----------------------------
bool STOP_FLAG = false;

// -----------------------------
// Setup
// -----------------------------
void setup() {
  Serial.begin(115200);

  motor1.setMaxSpeed(800);
  motor1.setAcceleration(800);

  motor2.setMaxSpeed(800);
  motor2.setAcceleration(800);

  motor3.setMaxSpeed(800);
  motor3.setAcceleration(800);

  motor4.setMaxSpeed(800);
  motor4.setAcceleration(800);

  Serial.println("Arduino Ready!");
}

// -----------------------------
// STOP all motors instantly
// -----------------------------
void stopAll() {
  STOP_FLAG = true;

  motor1.stop();
  motor2.stop();
  motor3.stop();
  motor4.stop();

  motor1.setCurrentPosition(0);
  motor2.setCurrentPosition(0);
  motor3.setCurrentPosition(0);
  motor4.setCurrentPosition(0);

  Serial.println("!!! EMERGENCY STOP ACTIVATED !!!");

  delay(50);
  STOP_FLAG = false;
}

// -----------------------------
// Loop
// -----------------------------
void loop() {

  // Handle incoming serial commands
  if (Serial.available()) {
    char cmd = Serial.read();

    switch (cmd) {

      case 'F':   // MOTOR_FORWARD
        motor1.move(200);
        Serial.println("Motor1 Forward");
        break;

      case 'R':   // MOTOR_REVERSE
        motor1.move(-200);
        Serial.println("Motor1 Reverse");
        break;

      case 'U':   // MOTOR_UP
        motor2.move(200);
        Serial.println("Motor2 Up");
        break;

      case 'D':   // MOTOR_DOWN
        motor2.move(-200);
        Serial.println("Motor2 Down");
        break;

      case 'E':   // MOTOR_E+
        motor3.move(200);
        Serial.println("Motor3 Extrude +");
        break;

      case 'e':   // MOTOR_E-
        motor3.move(-200);
        Serial.println("Motor3 Extrude -");
        break;

      case 'Y':   // MOTOR_Y+
        motor4.move(200);
        Serial.println("Motor4 Y+");
        break;

      case 'y':   // MOTOR_Y-
        motor4.move(-200);
        Serial.println("Motor4 Y-");
        break;

      case 'S':   // STOP
        stopAll();
        break;
    }
  }

  // Run motors unless STOP_FLAG is active
  if (!STOP_FLAG) {
    motor1.run();
    motor2.run();
    motor3.run();
    motor4.run();
  }
}
