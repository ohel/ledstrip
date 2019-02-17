#!/bin/bash

# The contents of $wifi_cfg_in should be these three lines:
#    ssid
#    password
#    static IPv4 address (use 0.0.0.0 for dynamic IP)
# If static IPv4 is set e.g. 192.168.1.4, a gateway of 192.168.1.1 is assumed.

piodev="1A86:7523"

wifi_cfg_in="web/wifi.cfg"
wifi_cfg_out="src/wificonfig.h"

wifi_ssid=$(head -n 1 $wifi_cfg_in 2>/dev/null)
wifi_pass=$(head -n 2 $wifi_cfg_in 2>/dev/null | tail -n 1)
wifi_ip=$(tail -n 1 $wifi_cfg_in 2>/dev/null)
echo ${wifi_ssid:-"wlan"} | sed "s/\(.*\)/const char* ssid = \"\0\";/" > $wifi_cfg_out
echo ${wifi_pass:-"password"} | sed "s/\(.*\)/const char* password = \"\0\";/" >> $wifi_cfg_out
echo ${wifi_ip:-"0.0.0.0"} | sed "s/\./, /g" | sed "s/\(.*\)/IPAddress ip(\0);/" >> $wifi_cfg_out
echo ${wifi_ip:-"0.0.0.0"} | sed "s/\./, /g" | sed "s/[0-9]\{1,\}$/1/" | sed "s/\(.*\)/IPAddress gateway(\0);/" >> $wifi_cfg_out

xxd -i web/index.html | sed 's/\([0-9a-f]\)$/\0, 0x00/' > src/index.h
xxd -i web/favicon.ico > src/favicon.h

port=$(pio device list | grep -B 2 "VID:PID=$piodev" | head -n 1)
pio run -t upload --upload-port $port
