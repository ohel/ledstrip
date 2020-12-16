#!/bin/bash

# The contents of $wifi_cfg_in should be these four lines:
#    ssid
#    password
#    static IPv4 address (use 0.0.0.0 for dynamic IP)
#    hostname for mDNS (host.local) and router info
# If static IPv4 is set e.g. 192.168.1.4, a gateway of 192.168.1.1 is assumed.

piodev="1A86:7523"

echo Use web/wifi.cfg to setup WiFi.
echo Use web/title.cfg to override index.html titles.

[ ! -e src/ledsetup.h ] && touch src/ledsetup.h
wifi_cfg_in="web/wifi.cfg"
wifi_cfg_out="src/wificonfig.h"

wifi_ssid=$(head -n 1 $wifi_cfg_in 2>/dev/null)
wifi_pass=$(head -n 2 $wifi_cfg_in 2>/dev/null | tail -n 1)
wifi_ip=$(head -n 3 $wifi_cfg_in 2>/dev/null | tail -n 1)
wifi_hostname=$(tail -n 1 $wifi_cfg_in 2>/dev/null)
echo ${wifi_ssid:-"wlan"} | sed "s/\(.*\)/const char* _WIFI_SSID = \"\0\";/" > $wifi_cfg_out
echo ${wifi_pass:-"password"} | sed "s/\(.*\)/const char* _WIFI_PASSWORD = \"\0\";/" >> $wifi_cfg_out
echo ${wifi_ip:-"0.0.0.0"} | sed "s/\./, /g" | sed "s/\(.*\)/const IPAddress _WIFI_IP(\0);/" >> $wifi_cfg_out
echo ${wifi_ip:-"0.0.0.0"} | sed "s/\./, /g" | sed "s/[0-9]\{1,\}$/1/" | sed "s/\(.*\)/const IPAddress _WIFI_GATEWAY(\0);/" >> $wifi_cfg_out
echo ${wifi_hostname:-"ledstrip"} | sed "s/\(.*\)/const char* _WIFI_HOSTNAME = \"\0\";/" >> $wifi_cfg_out

if [ -e web/title.cfg ]
then
    tmpfile=$(mktemp)
    cp web/index.html $tmpfile
    title=$(cat web/title.cfg)
    sed -i "s/__LEDSTRIP__/$title/g" web/index.html
fi

xxd -i web/index.html | sed 's/\([0-9a-f]\)$/\0, 0x00/' > src/index.h
xxd -i web/favicon.ico > src/favicon.h

[ -e web/title.cfg ] && [ -e $tmpfile ] && cp $tmpfile web/index.html && rm $tmpfile 2>/dev/null

port=$(pio device list | grep -B 2 "VID:PID=$piodev" | head -n 1)
[ $port ] && pio run -t upload --upload-port $port || pio run
