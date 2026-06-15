#!/usr/bin/env python3
"""OneNET 物模型 MQTT 上报 - OneJSON 标准格式"""
import sys, os, time, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from onetoken import generate_token, PRODUCT_ID, DEVICE_NAME
import paho.mqtt.client as mqtt

BROKER = os.environ.get("ONENET_MQTT_BROKER", "183.230.40.96")
PORT = int(os.environ.get("ONENET_MQTT_PORT", "1883"))
TOPIC = f"$sys/{PRODUCT_ID}/{DEVICE_NAME}/thing/property/post"
REPLY_TOPIC = f"$sys/{PRODUCT_ID}/{DEVICE_NAME}/thing/property/post/reply"
SEAT_STATE_FILE = "/tmp/seat_state.json"
SEAT_MAP = {"free": 0, "occupied": 1, "items_only": 2, "violation": 3}

def get_seat_state():
    try:
        if os.path.exists(SEAT_STATE_FILE):
            with open(SEAT_STATE_FILE) as f: s=json.load(f)
            return SEAT_MAP.get(s.get("state","free"),0), s.get("timer",0), s.get("persons",0)
    except: pass
    return 0,0,0

def on_connect(c,u,f,rc):
    if rc==0:
        print("[在线] Connected")
        c.subscribe(REPLY_TOPIC, qos=1)

def on_message(c,u,msg):
    try:
        data=json.loads(msg.payload)
        code=data.get("code","?")
        mid=data.get("id","?")
        print(f"  [REPLY] id={mid} code={code} {data.get('msg','')}")
    except: pass

def main():
    running=True;count=0
    while running:
        try:
            token=generate_token()
            client=mqtt.Client(client_id=DEVICE_NAME,clean_session=True,protocol=mqtt.MQTTv311)
            client.username_pw_set(PRODUCT_ID,token)
            client.on_connect=on_connect
            client.on_message=on_message
            client.connect(BROKER,PORT,keepalive=120)
            client.loop_start()
            time.sleep(1)

            reconnect=False
            while running and not reconnect:
                if not client.is_connected():
                    print("[WARN] Lost");reconnect=True;break

                seat_val,timer_val,person_val=get_seat_state()
                ts=int(time.time()*1000)

                # OneJSON 标准格式（每个属性含 value + time）
                payload=json.dumps({
                    "id":str(int(time.time())),
                    "version":"1.0",
                    "params":{
                        "online_status":{"value":1,"time":ts},
                        "person_count":{"value":person_val,"time":ts},
                        "seat_status":{"value":seat_val,"time":ts},
                        "timing":{"value":timer_val,"time":ts}
                    }})
                client.publish(TOPIC,payload,qos=1)
                count+=1
                sn=["free","occupied","items_only","violation"]
                print(f"[{count:04d}] seat={sn[seat_val]} p={person_val} t={timer_val}s")
                time.sleep(10)

            client.loop_stop()
            try:client.disconnect()
            except:pass

        except KeyboardInterrupt:running=False;print("\nStop")
        except Exception as e:print(f"[ERR] {e}");time.sleep(3)
    print("Done")

if __name__=="__main__":
    try:main()
    except KeyboardInterrupt:print("\nDone")
