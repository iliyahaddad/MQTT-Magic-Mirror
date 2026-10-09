from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
html=(root/'mirror/index.html').read_text(encoding='utf-8')
assert './mqtt-ws.js' in html, 'local MQTT client missing'
assert 'cdn.jsdelivr.net' not in html, 'runtime CDN dependency remains'
for token in ('mirror/weather','tempHigh','alertCooldownMs','data-ir','fa-IR-u-ca-persian','cfg.views.indexOf(viewKeys[event.key])','ArrowLeft'):
    assert token in html, f'missing UI feature: {token}'
fw=(root/'firmware/src/main.cpp').read_text(encoding='utf-8')
for token in ('ArduinoOTA.setPassword','publishDiscovery','mqtt.subscribe("mirror/command")','IR_TV_POWER'):
    assert token in fw, f'missing firmware feature: {token}'
js=(root/'mirror/mqtt-ws.js').read_text(encoding='utf-8')
for token in ('0xC0','this.subs.forEach','receive(chunk)','this.packet(0x40','QoS 2 is not supported'):
    assert token in js, f'mqtt-ws.js missing: {token}'
print('smoke checks passed')
