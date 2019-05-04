#!/bin/bash

# The contents of $wifi_cfg_in should be these three lines:
#    ssid
#    password
#    static IPv4 address (use 0.0.0.0 for dynamic IP)
# If static IPv4 is set e.g. 192.168.1.4, a gateway of 192.168.1.1 is assumed.

piodev="1A86:7523"

[ ! -e src/ledsetup.h ] && touch src/ledsetup.h
wifi_cfg_in="web/wifi.cfg"
wifi_cfg_out="src/wificonfig.h"

wifi_ssid=$(head -n 1 $wifi_cfg_in 2>/dev/null)
wifi_pass=$(head -n 2 $wifi_cfg_in 2>/dev/null | tail -n 1)
wifi_ip=$(tail -n 1 $wifi_cfg_in 2>/dev/null)
echo ${wifi_ssid:-"wlan"} | sed "s/\(.*\)/const char* _WIFI_SSID = \"\0\";/" > $wifi_cfg_out
echo ${wifi_pass:-"password"} | sed "s/\(.*\)/const char* _WIFI_PASSWORD = \"\0\";/" >> $wifi_cfg_out
echo ${wifi_ip:-"0.0.0.0"} | sed "s/\./, /g" | sed "s/\(.*\)/const IPAddress _WIFI_IP(\0);/" >> $wifi_cfg_out
echo ${wifi_ip:-"0.0.0.0"} | sed "s/\./, /g" | sed "s/[0-9]\{1,\}$/1/" | sed "s/\(.*\)/const IPAddress _WIFI_GATEWAY(\0);/" >> $wifi_cfg_out

xxd -i web/index.html | sed 's/\([0-9a-f]\)$/\0, 0x00/' > src/index.h
xxd -i web/favicon.ico > src/favicon.h

port=$(pio device list | grep -B 2 "VID:PID=$piodev" | head -n 1)
[ $port ] && pio run -t upload --upload-port $port || pio run
