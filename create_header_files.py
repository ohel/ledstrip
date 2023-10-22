import binascii
import json
import os.path
import re
import traceback

CONFIG_FILE = 'configs/current.json'

def readConfigFile():
    if os.path.isfile(CONFIG_FILE):
        with open(CONFIG_FILE, 'r') as f:
            contents = f.read()
        return json.loads(contents)
    else:
        raise Exception('Missing config file: ' + CONFIG_FILE)

def createIndexHtmlHeader(file_in: str) -> None:
    with open(file_in, 'r') as f:
        file_str = f.read()
    config = readConfigFile()
    file_str = file_str.replace('__LEDSTRIP__', config['title'])
    file_bytes = list(map(hex, bytearray(bytes(file_str, 'utf-8'))))
    file_bytes.append('0x00')
    createHeaderFile('src/index_html.h', 'index_html', file_bytes)

def binaryFileToBytes(file_in: str) -> []:
    file_bytes = []
    with open(file_in, 'rb') as f:
        for byte in iter(lambda: f.read(1), b''):
            file_bytes.append('0x' + binascii.hexlify(byte).decode())
    return file_bytes

def createHeaderFile(file_out: str, var_name: str, file_bytes: []) -> None:
    str_out = 'unsigned char ' + var_name + '_bytes[] = {'
    # The 12 bytes per line is just for nice output.
    bytecount = 12
    for byte in file_bytes:
        if bytecount == 12:
            str_out += '\n  '
            bytecount = 0
        str_out += byte + ","
        if bytecount != 11: str_out += ' '
        bytecount += 1
    str_out = str_out[:-2]
    str_out += '\n};\n'
    str_out += 'char* ' + var_name + ' = reinterpret_cast<char*>(&' + var_name + '_bytes[0]);'

    with open(file_out, 'w') as f:
        f.write(str_out)

def createLEDConfig(header_file):
    config = readConfigFile()
    # If static IPv4 is set e.g. 192.168.1.4, a gateway of 192.168.1.1 is assumed.
    with open(header_file, 'w') as f:
        f.write('#pragma once\n')
        f.write('const byte _FIRST_LED_INDEX = ' + '1;\n' if config['first_led_is_sacrificial'] else '0;\n')
        f.write('const byte _NUMBER_OF_LEDS = ' + str(config['number_of_leds']) + ';\n')
        f.write('const float _CURRENT_LIMIT_MA = ' + str(config['current_limit_in_milliamperes']) + ';')

def createWifiConfig(header_file):
    config = readConfigFile()
    # If static IPv4 is set e.g. 192.168.1.4, a gateway of 192.168.1.1 is assumed.
    gateway = re.sub(r'[0-9]{1,}$', '1', config['ip']).replace('.', ',')
    with open(header_file, 'w') as f:
        f.write('const char* _wifi_ssid = "' + config['ssid'] + '";\n')
        f.write('const char* _wifi_password = "' + config['password'] + '";\n')
        f.write('const IPAddress _wifi_ip(' + config['ip'].replace('.', ',') + ');\n')
        f.write('const IPAddress _wifi_gateway(' + gateway + ');\n')
        f.write('const char* _wifi_hostname = "' + config['hostname'] + '";')

try:
    createIndexHtmlHeader('web/index.html')
    createHeaderFile('src/favicon.h', 'favicon', binaryFileToBytes('web/favicon.ico'))
    createWifiConfig('src/wifi_cfg.h')
    createLEDConfig('src/led_cfg.h')
except Exception:
    traceback.print_exc()
    exit(1)
