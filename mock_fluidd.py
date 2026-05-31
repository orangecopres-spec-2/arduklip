import os
import time
from flask import Flask, render_template_string, redirect, url_for

LOG_FILE = os.path.expanduser("~/printer_data/logs/klippy.log")

app = Flask(__name__)

BUTTONS = [
    ("MOTOR_FORWARD", "RUN_MOTOR_FORWARD"),
    ("MOTOR_REVERSE", "RUN_MOTOR_REVERSE"),
    ("MOTOR_UP",      "RUN_MOTOR_UP"),
    ("MOTOR_DOWN",    "RUN_MOTOR_DOWN"),
    ("MOTOR_E+",      "RUN_MOTOR_E+"),
    ("MOTOR_E-",      "RUN_MOTOR_E-"),
    ("MOTOR_Y+",      "RUN_MOTOR_Y+"),
    ("MOTOR_Y-",      "RUN_MOTOR_Y-"),
]

def log(msg):
    ts = time.strftime("%Y-%m-%d %H:%M:%S")
    with open(LOG_FILE, "a") as f:
        f.write(f"{ts} arduklip {msg}\n")

TEMPLATE = """
<!doctype html>
<html>
<head>
<title>arduklip</title>
<style>
body { font-family: Arial; background: #1e1e1e; color: white; text-align: center; }
button {
    padding: 15px 25px;
    margin: 10px;
    font-size: 18px;
    border-radius: 8px;
    border: none;
    cursor: pointer;
}
.motor { background: #3a8dde; }
.stop  { background: #d9534f; color: white; font-weight: bold; }
.container { margin-top: 40px; }
</style>
</head>
<body>
<h1>arduklip Motor Control</h1>

<div class="container">
{% for name, code in buttons %}
<form method="post" action="{{ url_for('run', code=code) }}" style="display:inline;">
    <button class="motor">{{ name }}</button>
</form>
{% endfor %}
</div>

<div class="container">
<form method="post" action="{{ url_for('stop') }}">
    <button class="stop">STOP</button>
</form>
</div>

</body>
</html>
"""

@app.route("/")
def index():
    return render_template_string(TEMPLATE, buttons=BUTTONS)

@app.route("/run/<code>", methods=["POST"])
def run(code):
    log(code)
    return redirect(url_for("index"))

@app.route("/stop", methods=["POST"])
def stop():
    log("RUN_MOTOR_STOP")
    return redirect(url_for("index"))

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=7126, debug=True)
