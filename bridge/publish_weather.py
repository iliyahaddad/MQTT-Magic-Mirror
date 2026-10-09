#!/usr/bin/env python3
"""Publish weather data already collected by a local bridge to the mirror.
Input JSON example: {"city":"Baku","temp":18.2,"hum":64,"condition":"Cloudy","wind":"12 km/h"}
No external weather API is called by this script; integrate it with your own local provider.
"""
import argparse, json, math, sys
import paho.mqtt.client as mqtt

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--host', default='127.0.0.1'); ap.add_argument('--port',type=int,default=1883)
    ap.add_argument('--user', default=None); ap.add_argument('--password', default=None)
    ap.add_argument('--file', required=True, help='JSON file with locally collected weather')
    args=ap.parse_args()
    with open(args.file,encoding='utf-8') as f: data=json.load(f)
    if not isinstance(data,dict): raise SystemExit('weather JSON must be an object')
    for key in ('temp','hum'):
        if key in data and (not isinstance(data[key],(int,float)) or not math.isfinite(data[key])): raise SystemExit(f'invalid {key}')
    if 'hum' in data and not 0 <= data['hum'] <= 100: raise SystemExit('hum must be 0..100')
    client=mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id='magic-mirror-weather-bridge')
    if args.user: client.username_pw_set(args.user,args.password or '')
    client.connect(args.host,args.port,30)
    info=client.publish('mirror/weather',json.dumps(data,ensure_ascii=False),qos=1,retain=True)
    info.wait_for_publish(); client.disconnect(); print('Published mirror/weather')
if __name__=='__main__': main()
