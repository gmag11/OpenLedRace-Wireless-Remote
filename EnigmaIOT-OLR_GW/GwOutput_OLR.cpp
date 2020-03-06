/**
  * @file GwOutput_dummy.cpp
  * @version 0.8.1
  * @date 17/01/2020
  * @author German Martin
  * @brief Dummy Gateway output module
  *
  * Module to serve as start of EnigmaIOT gateway projects
  */

#include <Arduino.h>
#include "GwOutput_OLR.h"
#include <ESPAsyncWebServer.h>
#include <helperFunctions.h>
#include <debug.h>
#include <ArduinoJson.h>

#ifdef ESP32
#include <SPIFFS.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_tls.h"
#elif defined(ESP8266)
#include <Hash.h>
#endif // ESP32

#include <FS.h>


GatewayOutput_dummy GwOutput;

typedef struct {
    char address[18];
    int button;
    time_t last_button_pressed = 0;
} olr_controller_t;

olr_controller_t controllers[NUM_NODES];

#ifdef ESP8266
int leds[NUM_NODES] = { 15, 13, 12, 14 }; //{ D8,D7,D6,D5 } // ESP8266
#elif defined ESP32
int leds[NUM_NODES] = { 14, 27, 26, 25 }; // ESP32
#endif
bool LED_ON = 0;

void GatewayOutput_dummy::configManagerStart (EnigmaIOTGatewayClass* enigmaIotGw) {

}

bool GatewayOutput_dummy::saveConfig () {
    return true;
}

bool GatewayOutput_dummy::loadConfig () {
    return true;
}


void GatewayOutput_dummy::configManagerExit (bool status) {
    
}

bool GatewayOutput_dummy::begin () {
    DEBUG_INFO ("Begin");
    for (int i = 0; i < NUM_NODES; i++) {
        controllers[i].button = leds[i];
        pinMode (leds[i], OUTPUT);
        digitalWrite (leds[i], !LED_ON);
    }
    return true;
}

int findLed (char* address) {
    for (int i = 0; i < NUM_NODES; i++) {
        if (!strcmp (address, controllers[i].address)) {
            return i;
        }
    }
    return -1;
}


void GatewayOutput_dummy::loop () {
    //const int LED_ON_TIME = 10;
    //for (int i = 0; i < NUM_NODES; i++) {
    //    if (digitalRead (controllers[i].button)) {
    //        if (millis () - controllers[i].last_button_pressed > LED_ON_TIME) {
    //            digitalWrite (controllers[i].button, LOW);
    //        }
    //    }
    //}

}

bool GatewayOutput_dummy::outputDataSend (char* address, char* data, uint8_t length, GwOutput_data_type_t type) {
    int node_id = findLed (address);
    DEBUG_INFO ("Output data send. Node %d. Data %.*s", node_id, length, data);
    //DEBUG_WARN ("Node_id data: %d", node_id);
    const int capacity = JSON_ARRAY_SIZE (1) + JSON_OBJECT_SIZE (4);
    DynamicJsonDocument jsonBuffer (capacity);
    deserializeJson (jsonBuffer, data);
    JsonObject root_0 = jsonBuffer[0];
    int button = root_0["value"];

    if (button > 0) {
        digitalWrite (controllers[node_id].button, LED_ON);
        controllers[node_id].last_button_pressed = millis ();
    } else {
        digitalWrite (controllers[node_id].button, !LED_ON);
        DEBUG_DBG ("Button released. T = %d", millis () - controllers[node_id].last_button_pressed);
    }
    return true;
}

bool GatewayOutput_dummy::outputControlSend (char* address, uint8_t* data, uint8_t length) {
    DEBUG_INFO ("Output control send. Address %s. Data %s", address, printHexBuffer(data, length));
    return true;
}

bool GatewayOutput_dummy::newNodeSend (char* address, uint16_t node_id) {
    DEBUG_WARN ("New node: %s NodeID: %d", address, node_id);
    if (node_id < NUM_NODES) {
        strlcpy (controllers[node_id].address, address, 18);
    }
    //DEBUG_WARN ("Controller: %s, Node: %s", controllers[node_id].address, address);
    //digitalWrite (controllers[node_id].led, HIGH);
    return true;
}

bool GatewayOutput_dummy::nodeDisconnectedSend (char* address, gwInvalidateReason_t reason) {
    DEBUG_WARN ("Node %s disconnected. Reason %d", address, reason);
    return true;
}