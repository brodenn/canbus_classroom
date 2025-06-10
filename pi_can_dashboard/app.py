from flask import Flask, render_template, jsonify, request
from threading import Lock
from collections import deque
import can, csv, os
from gpiozero import Button
import atexit

app = Flask(__name__)
buffer = deque(maxlen=100)
buffer_lock = Lock()

LED_CONTROL_ID = 0x170
LED_STATUS_ID = 0x171
led_state = 0

ID_LABELS = {
    "0x321": "STM32 Test",
    "0x110": "High Beam",
    "0x120": "Battery Warning",
    "0x130": "Crash Trigger",
    "0x140": "Temperature Sensor",
    "0x150": "Blinker",
    "0x160": "Button B1",
    "0x170": "LED Control",
    "0x171": "LED Status",
    "0x2c2": "Right Stalk / Wiper / Lights",
    "0x459": "Hood & Wiper Feedback",
    "0x451": "Blinker Ack"
}

LOG_PATH = "logs/can_log.csv"
os.makedirs("logs", exist_ok=True)
if not os.path.exists(LOG_PATH):
    with open(LOG_PATH, "w", newline="") as f:
        csv.writer(f).writerow(["timestamp", "id", "label", "data"])

def log_to_csv(entry):
    with open(LOG_PATH, "a", newline="") as f:
        csv.writer(f).writerow([
            entry["timestamp"], entry["id"],
            ID_LABELS.get(entry["id"].lower(), "Unknown"),
            entry["data"]
        ])

can_bus = can.interface.Bus(channel="can0", interface="socketcan")
INT_PIN = 25  # BCM pin for MCP2515 INT
button = Button(INT_PIN, pull_up=True)

def handle_can_interrupt():
    global led_state
    msg = can_bus.recv(timeout=0.1)
    if not msg:
        return
    entry = {
        "id": hex(msg.arbitration_id),
        "data": msg.data.hex(),
        "timestamp": msg.timestamp
    }
    if msg.arbitration_id == LED_STATUS_ID and msg.data:
        led_state = msg.data[0]
    with buffer_lock:
        buffer.append(entry)
    log_to_csv(entry)
    print("📥 CAN via INT:", entry)

button.when_pressed = handle_can_interrupt

@atexit.register
def cleanup():
    button.close()

@app.route("/")
def index():
    return render_template("index.html")

@app.route("/api/can")
def api_can():
    with buffer_lock:
        data = list(buffer)
    return jsonify([
        {
            "id": m["id"], "label": ID_LABELS.get(m["id"].lower(), "Unknown"),
            "data": m["data"], "timestamp": m["timestamp"]
        } for m in data
    ])

@app.route("/api/led", methods=["POST"])
def toggle_led():
    global led_state
    new_state = 0 if led_state else 1
    msg = can.Message(arbitration_id=LED_CONTROL_ID, data=[new_state], is_extended_id=False)
    try:
        can_bus.send(msg)
        print(f"✅ Sent 0x170 data {new_state}")
        return "", 204
    except can.CanError as e:
        print("❌ CAN send failed:", e)
        return "CAN send failed", 500

if __name__ == "__main__":
    print("🚀 Flask CAN Dashboard using gpiozero on GPIO 25")
    app.run(host="0.0.0.0", port=5000)
