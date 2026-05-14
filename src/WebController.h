#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "WeatherModel.h"
#include "Config.h"

class WebController {
public:
    WebController();
    void begin(WeatherModel* model);
    void handleClient();

private:
    void setupRoutes();
    
    WeatherModel* weatherModel;
    WebServer server;
    DNSServer dnsServer;
    const IPAddress AP_IP;
};