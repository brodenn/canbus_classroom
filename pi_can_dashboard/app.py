from flask import Flask, render_template, jsonify, request
from threading import Thread, Lock
from collections import deque
import can
import csv
import os
import time

app = Flask(__name__)
buffer = deque(maxlen=100)
buffer_lock = Lock()

# CAN constants
LED_CONTROL_ID = 0x170
LED_STATUS_ID = 0x171
AIRBAG_ID = 0x666
led_state = 0
last_airbag_life = None
last_airbag_life_time = time.time()
AIRBAG_TIMEOUT = 5  # seconds

# ID label mapping
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
    "0x451": "Blinker Ack",
    "0x666": "Airbag / SRS"
}

# Logging setup
LOG_PATH = "logs/can_log.csv"
os.makedirs("logs", exist_ok=True)
if not os.path.exists(LOG_PATH):
    with open(LOG_PATH, "w", newline='') as f:
        csv.writer(f).writerow(["timestamp", "id", "label", "data"])

def log_to_csv(msg):
    with open(LOG_PATH, "a", newline='') as f:
        csv.writer(f).writerow([
            msg["timestamp"],
            msg["id"],
            ID_LABELS.get(msg["id"].lower(), "Unknown"),
            msg["data"]
        ])

# CAN bus setup
can_bus = can.interface.Bus(channel='can0', interface='socketcan')

def can_listener():
    global led_state, last_airbag_life, last_airbag_life_time
    print("🔌 Starting CAN listener thread")
    try:
        for msg in can_bus:
            if msg is None:
                continue

            entry = {
                "id": hex(msg.arbitration_id),
                "data": msg.data.hex(),
                "timestamp": msg.timestamp
            }

            # LED status update
            if msg.arbitration_id == LED_STATUS_ID and msg.data:
                led_state = msg.data[0]

            # Airbag/SRS monitoring
            if msg.arbitration_id == AIRBAG_ID and len(msg.data) >= 2:
                airbag_status = msg.data[0]
                airbag_life = msg.data[1]

                # Detect life signal change
                if airbag_life != last_airbag_life:
                    last_airbag_life = airbag_life
                    last_airbag_life_time = time.time()

                # Emergency reactions can be handled here
                if airbag_status in [0x44, 0x66]:
                    print("🚨 Airbag triggered! Status:", hex(airbag_status))

            with buffer_lock:
                buffer.append(entry)
            log_to_csv(entry)
            print("📥", entry)

    except Exception as e:
        print("❌ CAN listener error:", e)

@app.route("/")
def index():
    return render_template("index.html")

@app.route("/api/can")
def api_can():
    with buffer_lock:
        data = list(buffer)
    return jsonify([{
        "id": msg["id"],
        "label": ID_LABELS.get(msg["id"].lower(), "Unknown"),
        "data": decode_data(msg["id"].lower(), msg["data"]),
        "timestamp": msg["timestamp"]
    } for msg in data])

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

def decode_data(id_str, hex_data):
    try:
        bytes_list = [hex_data[i:i+2] for i in range(0, len(hex_data), 2)]

        if id_str == "0x2c2":
            status = []
            if len(bytes_list) >= 2:
                b0, b1 = bytes_list[0].lower(), bytes_list[1].lower()
                status.append({
                    "01": "⬅️ Left", "02": "➡️ Right",
                    "08": "💡 High Beam", "04": "🔦 Flash",
                    "00": "Neutral"
                }.get(b0, ""))
                if b0 == "02" and b1 == "88":
                    status.append("🌀 Wiper High")
                elif b0 == "02" and b1 == "85":
                    status.append("🧹 Wiper Low")
                elif b0 == "00" and b1 == "82":
                    status.append("🌧️ Auto Wiper")
            return " | ".join([s for s in status if s]) or hex_data

        elif id_str == "0x459":
            return {
                "8800": "🔒 Hood Closed",
                "8804": "🛑 Hood Open",
                "8120": "🌀 Wiping Active"
            }.get(hex_data, hex_data)

        elif id_str == "0x451" and len(bytes_list) >= 2:
            return {
                "81": "⬅️ Left Ack",
                "82": "➡️ Right Ack",
                "80": "↔️ None"
            }.get(bytes_list[1].lower(), hex_data)

        elif id_str == "0x140":
            val = int(hex_data, 16) / 256
            return f"{val:.1f} °C"

        elif id_str == "0x150":
            return "➡️ RIGHT" if hex_data == "01" else "⬅️ LEFT"

        elif id_str == "0x171":
            return "ON" if hex_data == "01" else "OFF"

        elif id_str == "0x120":
            return "⚠️ LOW"

        elif id_str == "0x130":
            return "🔴 CRASH!"

        elif id_str == "0x666":
            if len(bytes_list) >= 2:
                status, life = bytes_list[0], bytes_list[1]
                meaning = {
                    "11": "✅ Allt lugnt",
                    "44": "💥 Sidokrock – Sidoskydd aktiverat",
                    "66": "💥 Frontalkrock – Airbags utlösta"
                }.get(status, "❓ Okänd status")
                return f"{meaning} | Life: {life}"
            return hex_data

    except Exception as e:
        print(f"⚠️ Decode error for {id_str}: {e}")
        return hex_data

    return hex_data

if __name__ == "__main__":
    Thread(target=can_listener, daemon=True).start()
    print("🚀 Flask app running at http://0.0.0.0:5000")
    app.run(host="0.0.0.0", port=5000)
