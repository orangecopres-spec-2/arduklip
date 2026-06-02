#include <AccelStepper.h>

/* ===========================
   BUILD MODE
   =========================== */
#define SIMULATION_MODE 0   // 1 = simulator, 0 = real hardware

/* ===========================
   CONSTANTS
   =========================== */
const float STEPS_PER_MM_X = 80.0;
const float STEPS_PER_MM_Y = 80.0;
const float STEPS_PER_MM_Z = 400.0;
const float STEPS_PER_MM_E = 95.0;

// Step/dir pins
const int X_STEP_PIN = 2;
const int X_DIR_PIN  = 5;
const int Y_STEP_PIN = 3;
const int Y_DIR_PIN  = 6;
const int Z_STEP_PIN = 4;
const int Z_DIR_PIN  = 7;
const int E_STEP_PIN = 12;
const int E_DIR_PIN  = 13;

// Enable + endstops
const int STEPPER_ENABLE_PIN = 8;
const int X_ENDSTOP_PIN = 9;
const int Y_ENDSTOP_PIN = 10;
const int Z_ENDSTOP_PIN = 11;

// Fan + heaters (real mode)
const int FAN_PIN = A0;
const int HOTEND_PIN = 9;   // adjust to your board
const int BED_PIN    = 10;  // adjust to your board
const int THERM_HOTEND = A1;
const int THERM_BED    = A2;

/* ===========================
   GLOBAL STATE
   =========================== */
AccelStepper stepX(AccelStepper::DRIVER, X_STEP_PIN, X_DIR_PIN);
AccelStepper stepY(AccelStepper::DRIVER, Y_STEP_PIN, Y_DIR_PIN);
AccelStepper stepZ(AccelStepper::DRIVER, Z_STEP_PIN, Z_DIR_PIN);
AccelStepper stepE(AccelStepper::DRIVER, E_STEP_PIN, E_DIR_PIN);

float xPos=0, yPos=0, zPos=0, ePos=0;
bool absolutePositioning = true;
bool absoluteExtrusion = true;

float feedrate = 1500;
float speedFactor = 1.0;
float extrudeFactor = 1.0;

float hotendTemp = 25.0, bedTemp = 25.0;
float targetHotend = 0.0, targetBed = 0.0;

const float HEAT_RATE = 2.5;
const float COOL_RATE = 0.4;

/* ===========================
   GCODE STRUCT
   =========================== */
struct GCodeCommand {
  String cmd;
  float X=NAN, Y=NAN, Z=NAN, E=NAN, F=NAN, S=NAN, P=NAN;
  float I=NAN, J=NAN, K=NAN;
};

/* ===========================
   GCODE PARSER
   =========================== */
GCodeCommand parseGCode(String line) {
  GCodeCommand g;
  line.trim();

  // Remove line numbers
  if (line.startsWith("N")) {
    int sp = line.indexOf(' ');
    if (sp > 0) line = line.substring(sp + 1);
  }

  // Remove checksum
  int star = line.indexOf('*');
  if (star > 0) line = line.substring(0, star);

  // Remove comments
  int comment = line.indexOf(';');
  if (comment >= 0) line = line.substring(0, comment);

  if (line.length() < 2) return g;

  // Extract command
  int i = 0;
  while (i < line.length() && (isalpha(line[i]) || isdigit(line[i]))) i++;
  g.cmd = line.substring(0, i);
  g.cmd.toUpperCase();

  // Extract parameters
  for (int j = i; j < line.length(); j++) {
    char c = toupper(line[j]);
    if (strchr("XYZEFSPRIJK", c)) {
      float val = line.substring(j + 1).toFloat();
      switch (c) {
        case 'X': g.X = val; break;
        case 'Y': g.Y = val; break;
        case 'Z': g.Z = val; break;
        case 'E': g.E = val; break;
        case 'F': g.F = val; break;
        case 'S': g.S = val; break;
        case 'P': g.P = val; break;
        case 'I': g.I = val; break;
        case 'J': g.J = val; break;
        case 'K': g.K = val; break;
      }
    }
  }

  return g;
}

/* ===========================
   HEATERS
   =========================== */
float readThermistor(int pin) {
#if SIMULATION_MODE
  // Use simulated temps
  if (pin == THERM_HOTEND) return hotendTemp;
  if (pin == THERM_BED)    return bedTemp;
  return 25.0;
#else
  // TODO: real thermistor conversion (ADC -> °C)
  int raw = analogRead(pin);
  // crude placeholder: map 0–1023 to 0–300°C
  return (raw / 1023.0) * 300.0;
#endif
}

void updateHeaters() {
#if SIMULATION_MODE
  if (hotendTemp < targetHotend) hotendTemp = min(hotendTemp + HEAT_RATE, targetHotend);
  else if (hotendTemp > targetHotend) hotendTemp = max(hotendTemp - COOL_RATE, targetHotend);

  if (bedTemp < targetBed) bedTemp = min(bedTemp + HEAT_RATE * 0.6, targetBed);
  else if (bedTemp > targetBed) bedTemp = max(bedTemp - COOL_RATE, targetBed);
#else
  hotendTemp = readThermistor(THERM_HOTEND);
  bedTemp    = readThermistor(THERM_BED);

  // Simple bang-bang control (replace with PID later)
  if (targetHotend > 0) {
    if (hotendTemp < targetHotend - 2) digitalWrite(HOTEND_PIN, HIGH);
    else if (hotendTemp > targetHotend + 2) digitalWrite(HOTEND_PIN, LOW);
  } else {
    digitalWrite(HOTEND_PIN, LOW);
  }

  if (targetBed > 0) {
    if (bedTemp < targetBed - 2) digitalWrite(BED_PIN, HIGH);
    else if (bedTemp > targetBed + 2) digitalWrite(BED_PIN, LOW);
  } else {
    digitalWrite(BED_PIN, LOW);
  }
#endif
}

void reportTemperatures() {
  Serial.print("ok T:");
  Serial.print(hotendTemp, 1);
  Serial.print(" /");
  Serial.print(targetHotend, 1);
  Serial.print(" B:");
  Serial.print(bedTemp, 1);
  Serial.print(" /");
  Serial.println(targetBed, 1);
}

/* ===========================
   FAN
   =========================== */
void setFan(int speed) {
  analogWrite(FAN_PIN, constrain(speed, 0, 255));
}

/* ===========================
   HOMING
   =========================== */
void homeAxis(AccelStepper &motor, int endstopPin, float &pos, const char *axis, float stepsPerMm) {
#if SIMULATION_MODE
  motor.setCurrentPosition(0);
  pos = 0;
  Serial.print("Simulated homing "); Serial.println(axis);
#else
  // Assume endstops are NC, triggered LOW
  motor.setMaxSpeed(2000);
  motor.setSpeed(-2000);

  while (digitalRead(endstopPin) == HIGH) {
    motor.runSpeed();
    updateHeaters();
  }

  motor.move(5 * stepsPerMm);
  while (motor.distanceToGo() != 0) {
    motor.run();
    updateHeaters();
  }

  motor.setMaxSpeed(500);
  motor.setSpeed(-500);
  while (digitalRead(endstopPin) == HIGH) {
    motor.runSpeed();
    updateHeaters();
  }

  motor.setCurrentPosition(0);
  pos = 0;
#endif
}

/* ===========================
   MOTION
   =========================== */
void executeLinearMove(GCodeCommand &g) {
  if (!isnan(g.F) && g.F > 0) feedrate = g.F;

  float newX = absolutePositioning ? (isnan(g.X) ? xPos : g.X) : xPos + (isnan(g.X) ? 0 : g.X);
  float newY = absolutePositioning ? (isnan(g.Y) ? yPos : g.Y) : yPos + (isnan(g.Y) ? 0 : g.Y);
  float newZ = absolutePositioning ? (isnan(g.Z) ? zPos : g.Z) : zPos + (isnan(g.Z) ? 0 : g.Z);
  float newE = absoluteExtrusion ? (isnan(g.E) ? ePos : g.E) : ePos + (isnan(g.E) ? 0 : g.E);

  xPos = newX;
  yPos = newY;
  zPos = newZ;
  ePos = newE * extrudeFactor;

  stepX.moveTo(xPos * STEPS_PER_MM_X);
  stepY.moveTo(yPos * STEPS_PER_MM_Y);
  stepZ.moveTo(zPos * STEPS_PER_MM_Z);
  stepE.moveTo(ePos * STEPS_PER_MM_E);

  float mmPerSec = (feedrate * speedFactor) / 60.0;
  float maxStepsPerSec = mmPerSec * STEPS_PER_MM_X; // rough

  stepX.setMaxSpeed(maxStepsPerSec);
  stepY.setMaxSpeed(maxStepsPerSec);
  stepZ.setMaxSpeed(maxStepsPerSec);
  stepE.setMaxSpeed(maxStepsPerSec * 1.8);
}

void executeArc(GCodeCommand &g, bool clockwise) {
  // For now: approximate as straight line to target
  executeLinearMove(g);
}

/* ===========================
   REPORT POSITION
   =========================== */
void reportPosition() {
  Serial.print("X:"); Serial.print(xPos, 3);
  Serial.print(" Y:"); Serial.print(yPos, 3);
  Serial.print(" Z:"); Serial.print(zPos, 3);
  Serial.print(" E:"); Serial.println(ePos, 3);
}

/* ===========================
   SETUP
   =========================== */
void setup() {
  Serial.begin(115200);

  Serial.println("start");
  Serial.println("FIRMWARE_NAME:arduklip v2.0dev");
  Serial.println("MACHINE_TYPE:o1vo EXTRUDER_COUNT:1");
  Serial.println("ok");

  pinMode(FAN_PIN, OUTPUT);
  pinMode(STEPPER_ENABLE_PIN, OUTPUT);
  digitalWrite(STEPPER_ENABLE_PIN, LOW);

  pinMode(X_ENDSTOP_PIN, INPUT_PULLUP);
  pinMode(Y_ENDSTOP_PIN, INPUT_PULLUP);
  pinMode(Z_ENDSTOP_PIN, INPUT_PULLUP);

  pinMode(HOTEND_PIN, OUTPUT);
  pinMode(BED_PIN, OUTPUT);

  stepX.setAcceleration(3000);
  stepY.setAcceleration(3000);
  stepZ.setAcceleration(1500);
  stepE.setAcceleration(5000);
}

/* ===========================
   MAIN LOOP
   =========================== */
void loop() {
  static String buffer = "";

  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (buffer.length()) {
        GCodeCommand g = parseGCode(buffer);
        buffer = "";

        bool sendOk = true;

        // ===== G-CODES =====
        if (g.cmd == "G0" || g.cmd == "G1") {
          executeLinearMove(g);
        }
        else if (g.cmd == "G2" || g.cmd == "G02") {
          executeArc(g, true);
        }
        else if (g.cmd == "G3" || g.cmd == "G03") {
          executeArc(g, false);
        }
        else if (g.cmd == "G4" || g.cmd == "G04") {
          unsigned long ms = isnan(g.P) ? 0 : (unsigned long)g.P;
          unsigned long start = millis();
          while (millis() - start < ms) {
            updateHeaters();
            stepX.run(); stepY.run(); stepZ.run(); stepE.run();
          }
        }
        else if (g.cmd == "G28") {
          if (isnan(g.X) && isnan(g.Y) && isnan(g.Z)) {
            homeAxis(stepX, X_ENDSTOP_PIN, xPos, "X", STEPS_PER_MM_X);
            homeAxis(stepY, Y_ENDSTOP_PIN, yPos, "Y", STEPS_PER_MM_Y);
            homeAxis(stepZ, Z_ENDSTOP_PIN, zPos, "Z", STEPS_PER_MM_Z);
          } else {
            if (!isnan(g.X)) homeAxis(stepX, X_ENDSTOP_PIN, xPos, "X", STEPS_PER_MM_X);
            if (!isnan(g.Y)) homeAxis(stepY, Y_ENDSTOP_PIN, yPos, "Y", STEPS_PER_MM_Y);
            if (!isnan(g.Z)) homeAxis(stepZ, Z_ENDSTOP_PIN, zPos, "Z", STEPS_PER_MM_Z);
          }
        }
        else if (g.cmd == "G90") absolutePositioning = true;
        else if (g.cmd == "G91") absolutePositioning = false;
        else if (g.cmd == "G92") {
          if (!isnan(g.X)) { xPos = g.X; stepX.setCurrentPosition(xPos * STEPS_PER_MM_X); }
          if (!isnan(g.Y)) { yPos = g.Y; stepY.setCurrentPosition(yPos * STEPS_PER_MM_Y); }
          if (!isnan(g.Z)) { zPos = g.Z; stepZ.setCurrentPosition(zPos * STEPS_PER_MM_Z); }
          if (!isnan(g.E)) { ePos = g.E; stepE.setCurrentPosition(ePos * STEPS_PER_MM_E); }
        }

        // ===== M-CODES =====
        else if (g.cmd == "M104") {
          if (!isnan(g.S)) targetHotend = g.S;
        }
        else if (g.cmd == "M109") {
          if (!isnan(g.S)) targetHotend = g.S;
          while (hotendTemp < targetHotend - 0.5) {
            updateHeaters();
            delay(80);
          }
        }
        else if (g.cmd == "M140") {
          if (!isnan(g.S)) targetBed = g.S;
        }
        else if (g.cmd == "M190") {
          if (!isnan(g.S)) targetBed = g.S;
          while (bedTemp < targetBed - 0.5) {
            updateHeaters();
            delay(150);
          }
        }
        else if (g.cmd == "M105") {
          updateHeaters();
          reportTemperatures();
          sendOk = false;
        }
        else if (g.cmd == "M106") {
          setFan(isnan(g.S) ? 255 : (int)g.S);
        }
        else if (g.cmd == "M107") {
          setFan(0);
        }
        else if (g.cmd == "M82") {
          absoluteExtrusion = true;
        }
        else if (g.cmd == "M83") {
          absoluteExtrusion = false;
        }
        else if (g.cmd == "M114") {
          reportPosition();
          sendOk = false;
        }
        else if (g.cmd == "M220") {
          if (!isnan(g.S)) speedFactor = g.S / 100.0;
        }
        else if (g.cmd == "M221") {
          if (!isnan(g.S)) extrudeFactor = g.S / 100.0;
        }
        else if (g.cmd == "M84") {
          digitalWrite(STEPPER_ENABLE_PIN, HIGH);
          stepX.disableOutputs();
          stepY.disableOutputs();
          stepZ.disableOutputs();
          stepE.disableOutputs();
        }
        else if (g.cmd == "M115") {
          Serial.println("FIRMWARE_NAME:SimuPrint 2.0 PROTOCOL_VERSION:1.0 MACHINE_TYPE:SimulatedPrinter EXTRUDER_COUNT:1");
        }

        if (sendOk) Serial.println("ok");
      }
    } else {
      buffer += c;
    }
  }

  updateHeaters();
  stepX.run();
  stepY.run();
  stepZ.run();
  stepE.run();
}
