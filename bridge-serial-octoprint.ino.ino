#include <AccelStepper.h>

/* ===========================
   CONFIGURATION
   =========================== */

// Steps per mm
const float stepsPerMM = 80.0;

// Stepper drivers
AccelStepper stepX(AccelStepper::DRIVER, 2, 5);
AccelStepper stepY(AccelStepper::DRIVER, 3, 6);
AccelStepper stepZ(AccelStepper::DRIVER, 4, 7);
AccelStepper stepE(AccelStepper::DRIVER, 12, 13); // Extruder

// Endstop pins
const int X_ENDSTOP = 9;
const int Y_ENDSTOP = 10;
const int Z_ENDSTOP = 11;

// Position tracking
float xPos = 0, yPos = 0, zPos = 0, ePos = 0;

// Homing settings
const int HOMING_SPEED = 800;
const int HOMING_SLOW_SPEED = 200;
const int HOMING_BACKOFF_MM = -5;

// Feedrate & acceleration
float feedrate = 1500; // mm/min
float accel = 1000;

// Heater simulation
float hotendTemp = 25.0;
float bedTemp = 25.0;
float targetHotend = 0.0;
float targetBed = 0.0;

const float HEAT_RATE = 1.5;
const float COOL_RATE = 0.2;

/* ===========================
   UTILITY FUNCTIONS
   =========================== */

String readCommand() {
  if (!Serial.available()) return "";
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  if (cmd.startsWith("N")) {
    int spaceIndex = cmd.indexOf(' ');
    if (spaceIndex > 0) cmd = cmd.substring(spaceIndex + 1);
  }

  int starIndex = cmd.indexOf('*');
  if (starIndex > 0) cmd = cmd.substring(0, starIndex);

  return cmd;
}

void updateHeaters() {
  if (hotendTemp < targetHotend) hotendTemp += HEAT_RATE;
  else if (hotendTemp > targetHotend) hotendTemp -= COOL_RATE;

  if (bedTemp < targetBed) bedTemp += HEAT_RATE * 0.5;
  else if (bedTemp > targetBed) bedTemp -= COOL_RATE;
}

void waitForHotend() {
  while (hotendTemp < targetHotend - 0.5) {
    updateHeaters();
    Serial.print("T:"); Serial.println(hotendTemp);
    delay(200);
  }
}

void waitForBed() {
  while (bedTemp < targetBed - 0.5) {
    updateHeaters();
    Serial.print("B:"); Serial.println(bedTemp);
    delay(200);
  }
}

void homeAxis(AccelStepper &motor, int endstopPin, float &pos) {
  motor.setMaxSpeed(HOMING_SPEED);
  motor.setSpeed(-HOMING_SPEED);

  while (digitalRead(endstopPin) == HIGH) motor.runSpeed();

  motor.move(HOMING_BACKOFF_MM * stepsPerMM);
  while (motor.distanceToGo() != 0) motor.run();

  motor.setMaxSpeed(HOMING_SLOW_SPEED);
  motor.setSpeed(-HOMING_SLOW_SPEED);

  while (digitalRead(endstopPin) == HIGH) motor.runSpeed();

  motor.setCurrentPosition(0);
  pos = 0;
}

/* ===========================
   SETUP
   =========================== */

void setup() {
  Serial.begin(115200);

  pinMode(X_ENDSTOP, INPUT_PULLUP);
  pinMode(Y_ENDSTOP, INPUT_PULLUP);
  pinMode(Z_ENDSTOP, INPUT_PULLUP);

  stepX.setAcceleration(accel);
  stepY.setAcceleration(accel);
  stepZ.setAcceleration(accel);
  stepE.setAcceleration(accel);
}

/* ===========================
   MAIN LOOP
   =========================== */

void loop() {
  String cmd = readCommand();
  if (cmd.length() > 0) {

    /* -------------------------
       G28 HOMING
       ------------------------- */
    if (cmd.startsWith("G28")) {
      homeAxis(stepX, X_ENDSTOP, xPos);
      homeAxis(stepY, Y_ENDSTOP, yPos);
      homeAxis(stepZ, Z_ENDSTOP, zPos);
      Serial.println("ok");
      return;
    }

    /* -------------------------
       G0/G1 MOVEMENT
       ------------------------- */
    if (cmd.startsWith("G0") || cmd.startsWith("G1")) {

      if (cmd.indexOf("F") > 0)
        feedrate = cmd.substring(cmd.indexOf("F") + 1).toFloat();

      if (cmd.indexOf("X") > 0) {
        xPos = cmd.substring(cmd.indexOf("X") + 1).toFloat();
        stepX.moveTo(xPos * stepsPerMM);
      }
      if (cmd.indexOf("Y") > 0) {
        yPos = cmd.substring(cmd.indexOf("Y") + 1).toFloat();
        stepY.moveTo(yPos * stepsPerMM);
      }
      if (cmd.indexOf("Z") > 0) {
        zPos = cmd.substring(cmd.indexOf("Z") + 1).toFloat();
        stepZ.moveTo(zPos * stepsPerMM);
      }
      if (cmd.indexOf("E") > 0) {
        ePos = cmd.substring(cmd.indexOf("E") + 1).toFloat();
        stepE.moveTo(ePos * stepsPerMM);
      }

      Serial.println("ok");
      return;
    }

    /* -------------------------
       M114 POSITION REPORT
       ------------------------- */
    if (cmd == "M114") {
      Serial.print("X:"); Serial.print(xPos);
      Serial.print(" Y:"); Serial.print(yPos);
      Serial.print(" Z:"); Serial.print(zPos);
      Serial.print(" E:"); Serial.println(ePos);
      Serial.println("ok");
      return;
    }

    /* -------------------------
       HEATER COMMANDS
       ------------------------- */

    if (cmd.startsWith("M104")) {
      int s = cmd.indexOf("S");
      if (s > 0) targetHotend = cmd.substring(s + 1).toFloat();
      Serial.println("ok");
      return;
    }

    if (cmd.startsWith("M109")) {
      int s = cmd.indexOf("S");
      if (s > 0) targetHotend = cmd.substring(s + 1).toFloat();
      waitForHotend();
      Serial.println("ok");
      return;
    }

    if (cmd.startsWith("M140")) {
      int s = cmd.indexOf("S");
      if (s > 0) targetBed = cmd.substring(s + 1).toFloat();
      Serial.println("ok");
      return;
    }

    if (cmd.startsWith("M190")) {
      int s = cmd.indexOf("S");
      if (s > 0) targetBed = cmd.substring(s + 1).toFloat();
      waitForBed();
      Serial.println("ok");
      return;
    }

    if (cmd == "M105") {
      Serial.print("ok T:");
      Serial.print(hotendTemp);
      Serial.print(" /");
      Serial.print(targetHotend);
      Serial.print(" B:");
      Serial.print(bedTemp);
      Serial.print(" /");
      Serial.println(targetBed);
      return;
    }

    Serial.println("ok");
  }

  // Background tasks
  updateHeaters();
  stepX.run();
  stepY.run();
  stepZ.run();
  stepE.run();
}
