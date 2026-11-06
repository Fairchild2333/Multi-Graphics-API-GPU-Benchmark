"""Read the production firmware check API using the stock OTA client's format.

Does not provide real device identifiers, register a device, install updates,
or query internal/test endpoints. Client constants are read from the local
decompiled stock application and are not printed or saved in responses.
"""
from pathlib import Path
import hashlib
import json
import re
import sys
import time
import urllib.request

ROOT = Path(__file__).resolve().parent
CONST = (ROOT / 'OTA-app-decompiled/sources/com/blackshark/ota/util/OtaConstant.java').read_text(encoding='utf-8')

def client_constant(name):
    match = re.search(r'\b' + re.escape(name) + r'\s*=\s*"([^"]*)"', CONST)
    if not match:
        raise ValueError('Missing stock client constant: ' + name)
    return match.group(1)

def query(version):
    payload = {'IMEI': '', 'DeviceId': '', 'RegID': '', 'Channel': 'CN',
               'Carrier': '00', 'Model': 'KTUS-A0', 'Version': version, 'Special': 0}
    body = json.dumps(payload, separators=(',', ':'))
    stamp = str(int(time.time() * 1000))
    app_id = client_constant('APP_ID')
    signature = hashlib.sha1(('AppID' + app_id + 'AppKey' + client_constant('APPKEY')
                              + 'Body' + body + 'Timestamp' + stamp).encode()).hexdigest()
    request = urllib.request.Request('https://ota-api.blackshark.com/upgrade_check/',
        data=body.encode(), method='POST', headers={
            'Content-Type': 'application/x-javascript', 'AppID': app_id,
            'Model': 'SHARK KTUS-A0', 'Timestamp': stamp, 'Sign': signature,
            'User-Agent': 'com.blackshark.ota'})
    with urllib.request.urlopen(request, timeout=20) as response:
        result = json.load(response)
    (ROOT / ('ota-query-' + version + '.json')).write_text(
        json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(version, json.dumps(result, ensure_ascii=False), flush=True)
    return result

if __name__ == '__main__':
    for version in sys.argv[1:]:
        query(version)
