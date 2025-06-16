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

AIRBAG_ID = 0x666
last_airbag_life = None
last_airbag_life_time = time.time()

ID_LABELS = {
    "0x30":  "Ambient Temp & Humidity (ATU)",
    "0x100": "Battery Management System (BMS)",
    "0x110": "High Beam / Flash",
    "0x150": "Blinker Switch",
    "0x2c2": "Right Stalk / Wiper / Lights",
    "0x450": "Hazard Light Switch (HLS)",
    "0x451": "Blinker Ack",
    "0x459": "Hood & Wiper Feedback",
    "0x460": "Fläkt",
    "0x666": "Airbag / SRS"
}

LOG_PATH = "logs/can_log.csv"
os.makedirs("logs", exist_ok=True)
if not os.path.exists(LOG_PATH):
    with open(LOG_PATH, "w", newline='') as f:
        csv.writer(f).writerow(["timestamp", "id", "label", "data"])

def log_to_csv(msg):
    with open(LOG_PATH, "a", newline='') as f:
        csv.writer(f).writerow([
            msg["timestamp"], msg["id"], msg["label"], msg["data"]
        ])

can_bus = can.interface.Bus(channel='can0', interface='socketcan')

def can_listener():
    global last_airbag_life, last_airbag_life_time
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

            if msg.arbitration_id == AIRBAG_ID and len(msg.data) >= 2:
                airbag_status = msg.data[0]
                airbag_life = msg.data[1]
                if airbag_life != last_airbag_life:
                    last_airbag_life = airbag_life
                    last_airbag_life_time = time.time()
                if airbag_status in [0x44, 0x66]:
                    print("🚨 Airbag triggered! Status:", hex(airbag_status))

            with buffer_lock:
                buffer.append(entry)
            print("📅", entry)

    except Exception as e:
        print("❌ CAN listener error:", e)

@app.route("/")
def index():
    return render_template("index.html")

@app.route("/api/can")
def api_can():
    with buffer_lock:
        data = list(buffer)

    response = []
    for msg in data:
        id_str = msg["id"].lower()
        base_label = ID_LABELS.get(id_str, "Unknown")
        decoded = decode_data(id_str, msg["data"])

        if isinstance(decoded, list):
            for label, value in decoded:
                response.append({
                    "id": msg["id"],
                    "label": label,
                    "data": value,
                    "timestamp": msg["timestamp"]
                })
                log_to_csv({
                    "timestamp": msg["timestamp"],
                    "id": msg["id"],
                    "label": label,
                    "data": value
                })
        else:
            response.append({
                "id": msg["id"],
                "label": base_label,
                "data": decoded,
                "timestamp": msg["timestamp"]
            })
            log_to_csv({
                "timestamp": msg["timestamp"],
                "id": msg["id"],
                "label": base_label,
                "data": decoded
            })

    return jsonify(response)

def decode_data(id_str, hex_data):
    try:
        bytes_list = [hex_data[i:i+2] for i in range(0, len(hex_data), 2)]

        if id_str == "0x2c2" and len(bytes_list) >= 2:
            b0, b1 = bytes_list[0].lower(), bytes_list[1].lower()
            decoded = []

            if b0 == "01": decoded.append(("Blinker", "⬅️ Left"))
            elif b0 == "02": decoded.append(("Blinker", "➡️ Right"))
            if b0 == "08": decoded.append(("High Beam / Flash", "🔥 High Beam"))
            elif b0 == "04": decoded.append(("High Beam / Flash", "🔦 Flash"))

            if b0 == "00" and b1 == "82":
                decoded.append(("Wiper", "🌧️ Auto Wiper"))
            elif b0 == "02" and b1 == "85":
                decoded.append(("Wiper", "🪚 Wiper Low"))
            elif b0 == "02" and b1 == "88":
                decoded.append(("Wiper", "🌀 Wiper High"))
            elif b0 == "00" and b1 == "90":
                decoded.append(("Wiper", "🧴 Spolarvätska"))

            if len(bytes_list) > 2:
                sensitivity = {
                    "01": "Låg känslighet",
                    "05": "Medel känslighet",
                    "09": "Hög känslighet",
                    "0d": "Väldigt hög känslighet"
                }.get(bytes_list[2].lower())
                if sensitivity:
                    decoded.append(("Wiper Sensitivity", sensitivity))

            return decoded if decoded else [("Right Stalk", hex_data)]

        elif id_str == "0x459":
            result = []
            if hex_data.startswith("8800"):
                result.append(("Hood", "🔒 Hood Closed"))
            elif hex_data.startswith("8804"):
                result.append(("Hood", "🛑 Hood Open"))
            elif hex_data.startswith("8120"):
                result.append(("Wiper", "🌀 Wiping Active"))
            return result

        elif id_str == "0x451" and len(bytes_list) >= 2:
            ack = {
                "81": "⬅️ Left Ack",
                "82": "➡️ Right Ack",
                "80": "↔️ None"
            }.get(bytes_list[1].lower(), hex_data)
            return ack

        elif id_str == "0x150":
            return "➡️ RIGHT" if hex_data == "01" else "⬅️ LEFT"

        elif id_str == "0x666" and len(bytes_list) >= 2:
            status, life = bytes_list[0], bytes_list[1]
            meaning = {
                "11": "✅ Allt lugnt",
                "44": "💥 Sidokrock – Sidoskydd aktiverat",
                "66": "💥 Frontalkrock – Airbags utlösta"
            }.get(status, "❓ Okänd status")
            return f"{meaning} | Life: {life}"

        elif id_str == "0x450" and len(bytes_list) >= 2:
            return "🚨 HAZARD ON" if bytes_list[1].lower() == "83" else "🚨 HAZARD OFF"

        elif id_str == "0x100":
            if len(bytes_list) >= 2:
                return "🔋 LOW VOLTAGE" if bytes_list[1] == "01" else "🔋 OK"
            return f"RAW: {bytes_list}"

        elif id_str == "0x30":
            if len(bytes_list) >= 6:
                temp_raw = int("".join(bytes_list[:5]), 16)
                humidity = int(bytes_list[5], 16)
                temp_c = temp_raw / 100.0
                return f"🌡️ {temp_c:.2f} °C | 💧 {humidity}%"
            return f"RAW: {bytes_list}"

        elif id_str == "0x460" and len(bytes_list) >= 1:
            return "🌀 Fläkt På" if bytes_list[0] == "01" else "❄️ Fläkt Av"

    except Exception as e:
        print(f"⚠️ Decode error for {id_str}: {e}")
        return f"ERR: {hex_data}"

    return hex_data

if __name__ == "__main__":
    Thread(target=can_listener, daemon=True).start()
    print("🚀 Flask app running at http://0.0.0.0:5000")
    app.run(host="0.0.0.0", port=5000)
