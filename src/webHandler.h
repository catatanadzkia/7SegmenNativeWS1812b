#ifndef WEB_HANDLER_H
#define WEB_HANDLER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

extern AsyncWebServer server;
 
void setupServer();

#endif