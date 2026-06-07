#include <AccelStepper.h>

/* ===========================
   BUILD MODE
   =========================== */
#define SIMULATION_MODE 0   // 1 = simulator, 0 = real hardware

/* ===========================
   CONSTANTS & PINS
   =========================== */
const float STEPS_PER_MM_X = 80.0;
const float STEPS_PER_MM_Y = 80.0;
const float STEPS_PER_MM_Z = 400.0;
const float STEPS_PER_MM_E = 95.0;

// Stepper pins
const int X_STEP_PIN = 2;
const int X_DIR_PIN  = 5;
const int Y_STEP_PIN = 3;
const int Y_DIR_PIN  = 6;
const int Z_STEP_PIN = 4;
const int Z_DIR_PIN  = 7;
const int E_STEP_PIN = 12;
const int E_DIR_PIN  = 13;

// Enable + Endstops
const int STEPPER_ENABLE_PIN = 8;
const int X_ENDSTOP_PIN = 9;
const int Y_ENDSTOP_PIN = 10;
const int Z_ENDSTOP_PIN = 11;

// Heaters + Fan
const int FAN_PIN = A0;
const int HOTEND_PIN = 9;    // Adjust for your board
const int BED_PIN    = 10;   // Adjust for your board
const int THERM_HOTEND = A1;
const int THERM_BED    = A2;

/* ===========================
   GLOBAL STATE
   =========================== */
AccelStepper stepX(AccelStepper::DRIVER, X_STEP_PIN, X_DIR_PIN);
AccelStepper stepY(AccelStepper::DRIVER, Y_STEP_PIN, Y_DIR_PIN);
AccelStepper stepZ(AccelStepper::DRIVER, Z_STEP_PIN, Z_DIR_PIN);
AccelStepper stepE(AccelStepper::DRIVER, E_STEP_PIN, E_DIR_PIN);

float xPos = 0.0, yPos = 0.0, zPos = 0.0, ePos = 0.0;
bool absolutePositioning = true;
bool absoluteExtrusion = true;

float feedrate = 1500.0;
float speedFactor = 1.0;
float extrudeFactor = 1.0;

float hotendTemp = 25.0, bedTemp = 25.0;
float targetHotend = 0.0, targetBed = 0.0;

const float HEAT_RATE = 2.5;
const float COOL_RATE = 0.4;

/* ===========================
   GCODE PARSER
   =========================== */
struct GCodeCommand {
  String cmd;
  float X = NAN, Y = NAN, Z = NAN, E = NAN, F = NAN, S = NAN, P = NAN;
};

GCodeCommand parseGCode(String line) {
  GCodeCommand g;
  line.trim();

  // Remove line number
  if (line.startsWith("N")) {
    int sp = line.indexOf(' ');
    if (sp > 0) line = line.substring(sp + 1);
  }

  // Remove checksum
  int star = line.indexOf('*');
  if (star > 0) line = line.substring(0, star);

  // Remove comment
  int comment = line.indexOf(';');
  if (comment >= 0) line = line.substring(0, comment);

  if (line.length() < 2) return g;

  int i = 0;
  while (i < line.length()() && (isalpha(line[i]) || isdigit(line[i]))) i++;
  g.cmd = line.substring(0, i);
  g.cmd.toUpperCase();

  for (int j = i; j < line.length(); j++) {
    char c = toupper(line[j]);
    if (strchr("XYZEFSP", c)) {
      float val = line.substring(j + 1).toFloat();
      switch (c) {
        case 'X': g.X = val; break;
        case 'Y': g.Y = val; break;
        case 'Z': g.Z = val; break;
        case 'E': g.E = val; break;
        case 'F': g.F = val; break;
        case 'S': g.S = val; break;
        case 'P': g.P = val; break;
      }
    }
  }
  return g;
}

/* ===========================
   REPORTING FUNCTIONS
   =========================== */
void reportTemperatures() {
  Serial.print(F("ok T:")); Serial.print(hotendTemp, 1);
  Serial.print(F(" /")); Serial.print(targetHotend, 1);
  Serial.print(F(" B:")); Serial.print(bedTemp, 1);
  Serial.print(F(" /")); Serial.print(targetBed, 1);
  Serial.println(F(" @:0 B@:0"));   // Heater power (placeholder)
}

void reportPosition() {
  Serial.print(F("X:")); Serial.print(xPos, 3);
  Serial.print(F(" Y:")); Serial.print(yPos, 3);
  Serial.print(F(" Z:")); Serial.print(zPos, 3);
  Serial.print(F(" E:")); Serial.println(ePos, 3);
  Serial.println(F("ok"));
}

void sendOk() {
  Serial.println(F("ok"));
}

/* ===========================
   HEATERS & FAN
   =========================== */
float readThermistor(int pin) {
#if SIMULATION_MODE
  if (pin == THERM_HOTEND) return hotendTemp;
  if (pin == THERM_BED)    return bedTemp;
  return 25.0;
#else
  int raw = analogRead(pin);
  return (raw / 1023.0) * 300.0;  // TODO: Replace with proper thermistor table
#endif
}

void updateHeaters() {
#if SIMULATION_MODE
  // Simulated heating
  if (hotendTemp < targetHotend) hotendTemp = min(hotendTemp + HEAT_RATE, targetHotend);
  else if (hotendTemp > targetHotend) hotendTemp = max(hotendTemp - COOL_RATE, targetHotend);

  if (bedTemp < targetBed) bedTemp = min(bedTemp + HEAT_RATE * 0.6, targetBed);
  else if (bedTemp > targetBed) bedTemp = max(bedTemp - COOL_RATE, targetBed);
#else
  hotendTemp = readThermistor(THERM_HOTEND);
  bedTemp    = readThermistor(THERM_BED);

  // Bang-bang control
  digitalWrite(HOTEND_PIN, (targetHotend > 0 && hotendTemp < targetHotend - 2) ? HIGH : LOW);
  digitalWrite(BED_PIN,    (targetBed    > 0 && bedTemp    < targetBed    - 2) ? HIGH : LOW);
#endif
}

void setFan(int speed) {
  analogWrite(FAN_PIN, constrain(speed, 0, 255));
}

/* ===========================
   HOMING
   =========================== */
void homeAxis(AccelStepper &motor, int endstopPin, float &pos, float stepsPerMm) {
#if SIMULATION_MODE
  motor.setCurrentPosition(0);
  pos = 0;
  Serial.print(F("ok\n"));  // Simulated
#else
  motor.setMaxSpeed(2000);
  motor.setSpeed(-2000);

  while (digitalRead(endstopPin) == HIGH) {
    motor.runSpeed();
    updateHeaters();
  }

  // Retract a bit
  motor.move(10 * stepsPerMm);
  while (motor.distanceToGo() != 0) {
    motor.run();
    updateHeaters();
  }

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
  float maxStepsPerSec = mmPerSec * STEPS_PER_MM_X;

  stepX.setMaxSpeed(maxStepsPerSec);
  stepY.setMaxSpeed(maxStepsPerSec);
  stepZ.setMaxSpeed(maxStepsPerSec * 0.6);
  stepE.setMaxSpeed(maxStepsPerSec * 1.8);
}

/* ===========================
   SETUP
   =========================== */
void setup() {
  Serial.begin(115200);
  Serial.println(F("start"));
  Serial.println(F("FIRMWARE_NAME:ArduKlip (MarlinCompatible) FIRMWARE_VERSION:2.0 PROTOCOL_VERSION:1.0 MACHINE_TYPE:Custom EXTRUDER_COUNT:1"));
  Serial.println(F("ok"));

  pinMode(STEPPER_ENABLE_PIN, OUTPUT);
  digitalWrite(STEPPER_ENABLE_PIN, LOW);  // Enable steppers

  pinMode(X_ENDSTOP_PIN, INPUT_PULLUP);
  pinMode(Y_ENDSTOP_PIN, INPUT_PULLUP);
  pinMode(Z_ENDSTOP_PIN, INPUT_PULLUP);

  pinMode(HOTEND_PIN, OUTPUT);
  pinMode(BED_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);

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

  // Process incoming G-code
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (buffer.length() > 0) {
        GCodeCommand g = parseGCode(buffer);
        buffer = "";

        bool sendOk = true;

        // ==================== G-CODES ====================
        if (g.cmd == "G0" || g.cmd == "G1") {
          executeLinearMove(g);
        }
        else if (g.cmd == "G28") {
          if (isnan(g.X) && isnan(g.Y) && isnan(g.Z)) {
            homeAxis(stepX, X_ENDSTOP_PIN, xPos, STEPS_PER_MM_X);
            homeAxis(stepY, Y_ENDSTOP_PIN, yPos, STEPS_PER_MM_Y);
            homeAxis(stepZ, Z_ENDSTOP_PIN, zPos, STEPS_PER_MM_Z);
          } else {
            if (!isnan(g.X)) homeAxis(stepX, X_ENDSTOP_PIN, xPos, STEPS_PER_MM_X);
            if (!isnan(g.Y)) homeAxis(stepY, Y_ENDSTOP_PIN, yPos, STEPS_PER_MM_Y);
            if (!isnan(g.Z)) homeAxis(stepZ, Z_ENDSTOP_PIN, zPos, STEPS_PER_MM_Z);
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
        else if (g.cmd == "G4" || g.cmd == "G04") {
          unsigned long ms = isnan(g.P) ? 0 : (unsigned long)(g.P * 1000);
          unsigned long start = millis();
          while (millis() - start < ms) {
            updateHeaters();
            stepX.run(); stepY.run(); stepZ.run(); stepE.run();
          }
        }

        // ==================== M-CODES ====================
        else if (g.cmd == "M104") {
          if (!isnan(g.S)) targetHotend = g.S;
        }
        else if (g.cmd == "M109") {
          if (!isnan(g.S)) targetHotend = g.S;
          while (hotendTemp < targetHotend - 0.5) {
            updateHeaters();
            stepX.run(); stepY.run(); stepZ.run(); stepE.run();
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
            stepX.run(); stepY.run(); stepZ.run(); stepE.run();
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
        else if (g.cmd == "M82") absoluteExtrusion = true;
        else if (g.cmd == "M83") absoluteExtrusion = false;
        else if (g.cmd == "M84") {
          digitalWrite(STEPPER_ENABLE_PIN, HIGH);
          stepX.disableOutputs();
          stepY.disableOutputs();
          stepZ.disableOutputs();
          stepE.disableOutputs();
        }
        else if (g.cmd == "M114") {
          reportPosition();
          sendOk = false;
        }
        else if (g.cmd == "M115") {
          Serial.println(F("FIRMWARE_NAME:ArduKlip (MarlinCompatible) FIRMWARE_VERSION:2.0 PROTOCOL_VERSION:1.0 MACHINE_TYPE:Custom EXTRUDER_COUNT:1"));
          sendOk = false;
        }
        else if (g.cmd == "M220") {
          if (!isnan(g.S)) speedFactor = g.S / 100.0;
        }
        else if (g.cmd == "M221") {
          if (!isnan(g.S)) extrudeFactor = g.S / 100.0;
        }
        else if (g.cmd == "M119") {  // Endstop status
          Serial.print(F("X:")); Serial.print(digitalRead(X_ENDSTOP_PIN) == LOW ? "TRIGGERED" : "open");
          Serial.print(F(" Y:")); Serial.print(digitalRead(Y_ENDSTOP_PIN) == LOW ? "TRIGGERED" : "open");
          Serial.print(F(" Z:")); Serial.print(digitalRead(Z_ENDSTOP_PIN) == LOW ? "TRIGGERED" : "open");
          Serial.println();
          sendOk = false;
        }

        if (sendOk) sendOk();
      }
    } else {
      buffer += c;
    }
  }

  // Background tasks
  updateHeaters();
  stepX.run();
  stepY.run();
  stepZ.run();
  stepE.run();
}
