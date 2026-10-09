#!/usr/bin/env python3
"""Fake sensor node: lets you run the whole demo without any hardware."""
import argparse
import json
import random
import time

import paho.mqtt.client as mqtt

IR_CODES = ["0x20DF10EF", "0x20DF40BF", "0x20DFC03F"]
MESSAGES = ["Take your keys", "Dentist at 15:30", "Rain expected tonight", "Water the plants"]


def main() -> None:
    p = argparse.ArgumentParser(description="Simulated MQTT Magic Mirror sensor node")
    p.add_argument("--host", default="localhost")
    p.add_argument("--port", type=int, default=1883)
    p.add_argument("--id", default="sim-livingroom")
    p.add_argument("--interval", type=float, default=3.0, help="seconds between sensor readings")
    p.add_argument("--user", default=None)
    p.add_argument("--password", default=None)
    a = p.parse_args()

    base = f"mirror/{a.id}"
    c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=a.id)
    if a.user:
        c.username_pw_set(a.user, a.password or "")
    c.will_set(f"{base}/status", "offline", qos=1, retain=True)
    c.reconnect_delay_set(1, 30)
    c.connect(a.host, a.port, keepalive=30)
    c.loop_start()
    c.publish(f"{base}/status", "online", qos=1, retain=True)
    c.publish(f"{base}/presence", "1", qos=1, retain=True)

    temp, hum, n = 22.0, 45.0, 0
    print(f"Simulating '{a.id}' -> {a.host}:{a.port}  (Ctrl+C to stop)")
    try:
        while True:
            n += 1
            temp += random.uniform(-0.2, 0.2)
            hum = min(80.0, max(25.0, hum + random.uniform(-0.6, 0.6)))
            c.publish(f"{base}/sensors", json.dumps({"temp": round(temp, 1), "hum": round(hum, 1)}))
            if n % 8 == 0:
                present = random.random() > 0.35
                c.publish(f"{base}/presence", "1" if present else "0", qos=1, retain=True)
                print("presence ->", present)
            if n % 11 == 0:
                c.publish(f"{base}/ir", json.dumps({"protocol": "NEC", "code": random.choice(IR_CODES)}))
                print("ir button pressed")
            if n % 14 == 0:
                c.publish("mirror/message", json.dumps({"text": random.choice(MESSAGES), "ttl": 20}))
                print("message sent")
            time.sleep(a.interval)
    except KeyboardInterrupt:
        pass
    finally:
        c.publish(f"{base}/status", "offline", qos=1, retain=True).wait_for_publish()
        c.loop_stop()
        c.disconnect()


if __name__ == "__main__":
    main()
