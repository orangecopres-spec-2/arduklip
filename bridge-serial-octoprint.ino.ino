// Simple mock printer state
float xPos = 0, yPos = 0, zPos = 0, ePos = 0;
float hotendTemp = 25.0;
float bedTemp = 25.0;
float targetHotend = 0.0;
float targetBed = 0.0;

String readCommand() {
  if (!Serial.available()) return "";
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  // Remove line numbers (Nxx)
  if (cmd.startsWith("N")) {
    int spaceIndex = cmd.indexOf(' ');
    if (spaceIndex > 0) cmd = cmd.substring(spaceIndex + 1);
  }

  // Remove checksum (*xx)
  int starIndex = cmd.indexOf('*');
  if (starIndex > 0) cmd = cmd.substring(0, starIndex);

  return cmd;
}

void setup() {
  Serial.begin(115200);
}

void loop() {
  String cmd = readCommand();
  if (cmd.length() == 0) return;

  // -------------------------
  // Temperature request
  // -------------------------
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

  // -------------------------
  // Firmware info
  // -------------------------
  if (cmd == "M115") {
    Serial.println("FIRMWARE_NAME:arduklip 1.0");
    return;
  }

  // -------------------------
  // Set hotend temp
  // -------------------------
  if (cmd.startsWith("M104")) {
    int sIndex = cmd.indexOf("S");
    if (sIndex > 0) targetHotend = cmd.substring(sIndex + 1).toFloat();
    Serial.println("ok");
    return;
  }

  // -------------------------
  // Set bed temp
  // -------------------------
  if (cmd.startsWith("M140")) {
    int sIndex = cmd.indexOf("S");
    if (sIndex > 0) targetBed = cmd.substring(sIndex + 1).toFloat();
    Serial.println("ok");
    return;
  }

  // -------------------------
  // Movement (G0/G1)
  // -------------------------
  if (cmd.startsWith("G0") || cmd.startsWith("G1")) {
    if (cmd.indexOf("X") > 0) xPos = cmd.substring(cmd.indexOf("X") + 1).toFloat();
    if (cmd.indexOf("Y") > 0) yPos = cmd.substring(cmd.indexOf("Y") + 1).toFloat();
    if (cmd.indexOf("Z") > 0) zPos = cmd.substring(cmd.indexOf("Z") + 1).toFloat();
    if (cmd.indexOf("E") > 0) ePos = cmd.substring(cmd.indexOf("E") + 1).toFloat();
    Serial.println("ok");
    return;
  }

  // -------------------------
  // Report position
  // -------------------------
  if (cmd == "M114") {
    Serial.print("ok X:");
    Serial.print(xPos);
    Serial.print(" Y:");
    Serial.print(yPos);
    Serial.print(" Z:");
    Serial.print(zPos);
    Serial.print(" E:");
    Serial.println(ePos);
    return;
  }

  // -------------------------
  // Line number reset
  // -------------------------
  if (cmd.startsWith("M110")) {
    Serial.println("ok");
    return;
  }

  // -------------------------
  // Default response
  // -------------------------
  Serial.println("ok");
}
