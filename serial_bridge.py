import serial
import time
import os
import re

# -----------------------------
# Connect to Arduino
# -----------------------------
try:
    arduino = serial.Serial('/dev/ttyACM0', 115200, timeout=0.1)
    time.sleep(2)
    print("Connected to Arduino on /dev/ttyACM0")
except Exception as e:
    print(f"Could not connect to Arduino: {e}")
    exit(1)

# -----------------------------
# Log file path
# -----------------------------
log_path = os.path.expanduser('~/printer_data/logs/klippy.log')
print(f"Bridge Active! Monitoring {log_path}")

# -----------------------------
# Command map
# -----------------------------
COMMANDS = {
    "RUN_MOTOR_FORWARD": b'F',
    "RUN_MOTOR_REVERSE": b'R',
    "RUN_MOTOR_UP":      b'U',
    "RUN_MOTOR_DOWN":    b'D',
    "RUN_MOTOR_E+":      b'E',
    "RUN_MOTOR_E-":      b'e',
    "RUN_MOTOR_Y+":      b'Y',
    "RUN_MOTOR_Y-":      b'y',
}

STOP_FLAG = False

def stop_all():
    global STOP_FLAG
    STOP_FLAG = True
    print(">>> STOP RECEIVED — Halting all motors!")
    arduino.write(b'S')
    time.sleep(0.1)
    STOP_FLAG = False

# -----------------------------
# Regex to extract the command
# -----------------------------
pattern = re.compile(r"RUN_MOTOR_[A-Za-z0-9\+\-]+")

# -----------------------------
# Tail the log file
# -----------------------------
with open(log_path, 'r', errors='ignore') as f:
    f.seek(0, os.SEEK_END)

    while True:
        line = f.readline()
        if not line:
            time.sleep(0.05)
            continue

        line = line.strip()
        print("LOG:", line)

        # Extract command using regex
        match = pattern.search(line)
        if not match:
            continue

        cmd_key = match.group(0)
        print("FOUND:", cmd_key)

        # STOP command
        if cmd_key == "RUN_MOTOR_STOP":
            stop_all()
            continue

        # Motor commands
        if cmd_key in COMMANDS:
            serial_byte = COMMANDS[cmd_key]
            print(f">>> Trigger: {cmd_key}  →  Sending {serial_byte}")

            for _ in range(200):
                if STOP_FLAG:
                    break
                arduino.write(serial_byte)
                time.sleep(0.005)
