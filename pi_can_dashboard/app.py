from flask import Flask, render_template, jsonify, request
from threading import Lock
from collections import deque
import can
import csv
import os
import atexit
import lgpio

app = Flask(__name__)
buffer = deque(maxlen=100)
buffer_lock = Lock()

# CAN constants
LED_CONTROL_ID = 0x170
LED_STATUS_ID = 0x171
led_state = 0

# Label mapping
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

# Logging
LOG_PATH = "logs/can_log.csv"
os.makedirs("logs", exist_ok=True)
if not os.path.exists(LOG_PATH):
    with open(LOG_PATH, "w", newline="") as f:
        csv.writer(f).writerow(["timestamp", "id", "label", "data"])

def log_to_csv(entry):
    with open(LOG_PATH, "a", newline="") as f:
        csv.writer(f).writerow([
            entry["timestamp"],
            entry["id"],
            ID_LABELS.get(entry["id"].lower(), "Unknown"),
            entry["data"]
        ])

# CAN setup
can_bus = can.interface.Bus(channel='can0', interface='socketcan')

# GPIO setup with rpi-lgpio
INT_PIN = 25
gpio_handle = lgpio.gpiochip_open(0)
lgpio.gpio_claim_input(gpio_handle, INT_PIN)

# Interrupt callback
def can_interrupt_callback(chip, gpio, level, tick):
    global led_state
    try:
        msg = can_bus.recv(timeout=0.05)
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
        print("📥 CAN received:", entry)

    except Exception as e:
        print("🔥 CAN callback error:", e)

# Attach interrupt correctly
lgpio.gpio_claim_alert(gpio_handle, lgpio.SET_PULL_UP, INT_PIN, lgpio.FALLING_EDGE)
lgpio.set_alert_func(gpio_handle, INT_PIN, can_interrupt_callback)

# Cleanup at exit
@atexit.register
def cleanup():
    print("🧹 Cleaning up GPIO")
    lgpio.gpiochip_close(gpio_handle)

# Flask routes
@app.route("/")
def index():
    return render_template("index.html")

@app.route("/api/can")
def api_can():
    with buffer_lock:
        data = list(buffer)

    def label_msg(msg):
        id_lower = msg["id"].lower()
        return {
            "id": msg["id"],
            "label": ID_LABELS.get(id_lower, "Unknown"),
            "data": msg["data"],
            "timestamp": msg["timestamp"]
        }

    return jsonify([label_msg(m) for m in data])

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

# Run server
if __name__ == "__main__":
    print("🚀 Starting Flask CAN Dashboard with rpi-lgpio on GPIO 25")
    app.run(host="0.0.0.0", port=5000)
