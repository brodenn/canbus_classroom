from flask import Flask, render_template, jsonify, request
from threading import Thread
import can
from collections import deque
import csv
import os

app = Flask(__name__)
buffer = deque(maxlen=100)

# CAN constants
LED_CONTROL_ID = 0x170
LED_STATUS_ID = 0x171
led_state = 0

# Label mapping (all lowercase keys for consistency)
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

# Log file setup
LOG_PATH = "logs/can_log.csv"
os.makedirs("logs", exist_ok=True)
if not os.path.exists(LOG_PATH):
    with open(LOG_PATH, "w", newline='') as f:
        writer = csv.writer(f)
        writer.writerow(["timestamp", "id", "label", "data"])

def log_to_csv(msg):
    with open(LOG_PATH, "a", newline='') as f:
        writer = csv.writer(f)
        writer.writerow([
            msg["timestamp"],
            msg["id"],
            ID_LABELS.get(msg["id"].lower(), "Unknown"),  # Normalize for logging
            msg["data"]
        ])

# Persistent CAN bus
can_bus = can.interface.Bus(channel='can0', interface='socketcan')

def can_listener():
    global led_state
    while True:
        msg = can_bus.recv()
        entry = {
            "id": hex(msg.arbitration_id),
            "data": msg.data.hex(),
            "timestamp": msg.timestamp
        }

        if msg.arbitration_id == LED_STATUS_ID and len(msg.data) > 0:
            led_state = msg.data[0]

        buffer.append(entry)
        log_to_csv(entry)

@app.route("/")
def index():
    return render_template("index.html")

@app.route("/api/can")
def api_can():
    def label_msg(msg):
        id_lower = msg["id"].lower()  # Normalize ID for label matching
        return {
            "id": msg["id"],
            "label": ID_LABELS.get(id_lower, "Unknown"),
            "data": msg["data"],
            "timestamp": msg["timestamp"]
        }
    return jsonify([label_msg(m) for m in buffer])

@app.route("/api/led", methods=["POST"])
def toggle_led():
    global led_state
    new_state = 0 if led_state else 1
    msg = can.Message(arbitration_id=LED_CONTROL_ID, data=[new_state], is_extended_id=False)
    try:
        can_bus.send(msg)
        print(f"✅ Sent 0x170 with data: {new_state}")
        return "", 204
    except can.CanError as e:
        print(f"❌ CAN send failed: {e}")
        return "CAN send failed", 500

if __name__ == "__main__":
    Thread(target=can_listener, daemon=True).start()
    app.run(host="0.0.0.0", port=5000)
