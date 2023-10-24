#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "webconfig.h"
#include "wifi_cfg.h"
#include "index_html.h"
#include "favicon.h"

namespace WebConfig {
    void (*_operate)(String, byte, byte, byte);
    ESP8266WebServer _server(80);

    void handleClient() {
        _server.handleClient();
    }

    void setupWifi(void (*operate)(String, byte, byte, byte)) {
        _operate = operate;

        IPAddress subnet(255, 255, 255, 0);

        WiFi.mode(WIFI_STA);
        if (_wifi_ip.toString() == INADDR_NONE.toString()) {
            Serial.println("Using dynamic IP address.");
        } else {
            WiFi.config(_wifi_ip, _wifi_gateway, subnet);
        }
        WiFi.hostname(_wifi_hostname);
        WiFi.begin(_wifi_ssid, _wifi_password);

        int timeout = 0;
        Serial.print("Connecting to: ");
        Serial.println(_wifi_ssid);
        while (WiFi.status() != WL_CONNECTED && timeout < 20) {
            delay(1000);
            Serial.print(".");
            timeout++;
        }
        Serial.println("");
        if (WiFi.status() == WL_CONNECTED) {
            Serial.print("WiFi connected using IP address: ");
            Serial.println(WiFi.localIP());
        } else {
            Serial.println("Failed to connect to WiFi.");
        }

        _server.on("/", HTTP_GET, [](){
            Serial.println("GET: /");
            _server.send(200, "text/html", index_html);
        });

        _server.on("/favicon.ico", HTTP_GET, [](){
            Serial.println("GET: /favicon.ico");
            _server.send_P(200, "image/x-icon", favicon, sizeof(favicon_bytes));
        });

        _server.on("/", HTTP_POST, [](){
            Serial.println("POST: /");
            _server.send(200, "text/plain", "");

            String mode = _server.arg("mode");
            byte r = (byte) _server.arg("r").toInt();
            byte g = (byte) _server.arg("g").toInt();
            byte b = (byte) _server.arg("b").toInt();
            _operate(mode, r, g, b);
        });

        _server.begin();
        Serial.println("Server is running.");

        if (!MDNS.begin(_wifi_hostname)) {
            Serial.println("Failed to start mDNS responder.");
        } else {
            Serial.println("Started mDNS responder.");
        }
    }
};
