/**
  * @file EnigmaIOTGatewayOLR.ino
  * @version 0.0.1
  * @date 10/08/2020
  * @author German Martin
  * @brief Gateway based on EnigmaIoT over ESP-NOW with GPIO output module to serve as remote controller for OpenLedRace Project
  */


#include <Arduino.h>

#define CONNECT_TO_WIFI_AP 0 // We do not need Internet connection in this moment
#define NUM_NODES 4 // OLR currently supports 4 players max

#include <GwOutput_generic.h>
#include "GwOutput_OLR.h"

#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#include <Update.h>
#include <SPIFFS.h>
#include "esp_system.h"
#include "esp_event.h"
#elif defined ESP8266
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <Hash.h>
#include <SPI.h>
#endif // ESP32


#include <CayenneLPP.h>
#include <FS.h>

#include <EnigmaIOTGateway.h>
#include <helperFunctions.h>
#include <debug.h>
#include <espnow_hal.h>
#include <Curve25519.h>
#include <ChaChaPoly.h>
#include <Poly1305.h>
#include <SHA256.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>

#ifndef LED_BUILTIN
#define LED_BUILTIN 5
#endif // LED_BUILTIN

#define BLUE_LED LED_BUILTIN
#define RED_LED LED_BUILTIN

#ifdef ESP32
TimerHandle_t connectionLedTimer;
#elif defined(ESP8266)
ETSTimer connectionLedTimer;
#endif // ESP32

const int connectionLed = LED_BUILTIN;
boolean connectionLedFlashing = false;

#if CONNECT_TO_WIFI_AP != 0
#error Please define CONNECT_TO_WIFI_AP as 0
#endif

void flashConnectionLed (void* led) {
	//digitalWrite (*(int*)led, !digitalRead (*(int*)led));
	digitalWrite (LED_BUILTIN, !digitalRead(LED_BUILTIN));
}

void startConnectionFlash (int period) {
#ifdef ESP32
	if (!connectionLedFlashing) {
		connectionLedFlashing = true;
		connectionLedTimer = xTimerCreate ("led_flash", pdMS_TO_TICKS (period), pdTRUE, (void*)0, flashConnectionLed);
		xTimerStart (connectionLedTimer, 0);
	}
#elif defined (ESP8266)
	ets_timer_disarm (&connectionLedTimer);
	if (!connectionLedFlashing) {
		connectionLedFlashing = true;
		ets_timer_arm_new (&connectionLedTimer, period, true, true);
	}
#endif // ESP32
}

void stopConnectionFlash () {
#ifdef ESP32
	if (connectionLedFlashing) {
		connectionLedFlashing = false;
		xTimerStop (connectionLedTimer, 0);
		xTimerDelete (connectionLedTimer, 0);
	}
#elif defined(ESP8266)
	if (connectionLedFlashing) {
		connectionLedFlashing = false;
		ets_timer_disarm (&connectionLedTimer);
		digitalWrite (connectionLed, HIGH);
	}
#endif // ESP32
}

void wifiManagerExit (boolean status) {
	GwOutput.configManagerExit (status);
}

void wifiManagerStarted () {
	GwOutput.configManagerStart (&EnigmaIOTGateway);
}

void processRxControlData (char* macStr, uint8_t* data, uint8_t length) {
	if (data) {
		GwOutput.outputControlSend (macStr, data, length);
	}
}

void processRxData (uint8_t* mac, uint8_t* buffer, uint8_t length, uint16_t lostMessages, bool control, gatewayPayloadEncoding_t payload_type, char* nodeName = NULL) {
	//uint8_t *addr = mac;
	char* payload;
	size_t pld_size;
	const int PAYLOAD_SIZE = 512;

	payload = (char*)malloc (PAYLOAD_SIZE);


	char mac_str[18];
	mac2str (mac, mac_str);
	DEBUG_DBG ("%s --> %s", mac_str, printHexBuffer(buffer, length));
	if (control) {
		processRxControlData (mac_str, buffer, length);
		return;
	}
	//char* netName = EnigmaIOTGateway.getNetworkName ();
	if (payload_type == CAYENNELPP) {
		const int capacity = JSON_ARRAY_SIZE (25) + 25 * JSON_OBJECT_SIZE (4);
		DynamicJsonDocument jsonBuffer (capacity);
		//StaticJsonDocument<capacity> jsonBuffer;
		JsonArray root = jsonBuffer.createNestedArray ();
		CayenneLPP* cayennelpp = new CayenneLPP (MAX_DATA_PAYLOAD_SIZE);

		cayennelpp->decode ((uint8_t*)buffer, length, root);
		cayennelpp->CayenneLPP::~CayenneLPP ();
		free (cayennelpp);

		pld_size = serializeJson (root, payload, PAYLOAD_SIZE);
		DEBUG_DBG ("Received CayenneLPP payload from %s --> %.*s", mac_str, pld_size, payload);
	} else if (payload_type == RAW) {

		DEBUG_DBG ("Received raw payload from %s --> %.*s", mac_str, length, buffer);
		if (length <= PAYLOAD_SIZE) {
			memcpy (payload, buffer, length);
			pld_size = length;
		} else { // This will not happen but may lead to errors in case of using another physical transport
			memcpy (payload, buffer, PAYLOAD_SIZE);
			pld_size = PAYLOAD_SIZE;
		}
	}

	GwOutput.outputDataSend (mac_str, payload, pld_size);
	DEBUG_INFO ("Published data message from %s: %.*s", mac_str, pld_size, payload);
	if (lostMessages > 0) {
		pld_size = snprintf (payload, PAYLOAD_SIZE, "%u", lostMessages);
		GwOutput.outputDataSend (mac_str, payload, pld_size, GwOutput_data_type::lostmessages);
		DEBUG_VERBOSE ("Published MQTT from %s: %*.s", mac_str, pld_size, payload);
	}
	pld_size = snprintf (payload, PAYLOAD_SIZE, "{\"per\":%e,\"lostmessages\":%u,\"totalmessages\":%u,\"packetshour\":%.2f}",
							EnigmaIOTGateway.getPER ((uint8_t*)mac),
							EnigmaIOTGateway.getErrorPackets ((uint8_t*)mac),
							EnigmaIOTGateway.getTotalPackets ((uint8_t*)mac),
							EnigmaIOTGateway.getPacketsHour ((uint8_t*)mac));
	//GwOutput.outputDataSend (mac_str, payload, pld_size, GwOutput_data_type::status);
	//DEBUG_INFO ("Published MQTT from %s: %s", mac_str, payload);
	free (payload);
}

void onDownlinkData (uint8_t* address, char* nodeName, control_message_type_t msgType, char* data, unsigned int len) {
	char *buffer;
	unsigned int bufferLen = len;

	DEBUG_INFO ("DL Message for " MACSTR ". Type 0x%02X", MAC2STR (address), msgType);
	DEBUG_DBG ("Data: %.*s", len, data);

	buffer = (char*)malloc (len + 1);
	sprintf (buffer, "%.*s", len, data);
	bufferLen ++;

	if (!EnigmaIOTGateway.sendDownstream (address, (uint8_t*)buffer, bufferLen, msgType)) {
		DEBUG_ERROR ("Error sending esp_now message to " MACSTR, MAC2STR (address));
	} else {
		DEBUG_DBG ("Esp-now message sent or queued correctly");
	}

	free (buffer);
}

void newNodeConnected (uint8_t* mac, uint16_t node_id, char* nodeName = NULL) {
	char macstr[18];
	mac2str (mac, macstr);
	//Serial.printf ("New node connected: %s\n", macstr);

	if (!GwOutput.newNodeSend (macstr, node_id)) {
		DEBUG_WARN ("Error sending new node %s", macstr);
	} else {
		DEBUG_DBG ("New node %s message sent", macstr);
	}
}

void nodeDisconnected (uint8_t * mac, gwInvalidateReason_t reason) {
	char macstr[18];
	mac2str (mac, macstr);
	//Serial.printf ("Node %s disconnected. Reason %u\n", macstr, reason);
	if (!GwOutput.nodeDisconnectedSend (macstr, reason)) {
		DEBUG_WARN ("Error sending node disconnected %s reason %d", macstr, reason);
	} else {
		DEBUG_DBG ("Node %s disconnected message sent. Reason %d", macstr, reason);
	}
}

#ifdef ESP32
void EnigmaIOTGateway_handle (void * param) {
	for (;;) {
		EnigmaIOTGateway.handle ();
		vTaskDelay (0);
	}
}

void GwOutput_handle (void* param) {
	for (;;) {
		GwOutput.loop ();
		vTaskDelay (0);
	}
}

TaskHandle_t xEnigmaIOTGateway_handle = NULL;
TaskHandle_t gwoutput_handle = NULL;
#endif // ESP32

void setup () {
	Serial.begin (115200); Serial.println (); Serial.println ();
#ifdef ESP8266
	ets_timer_setfn (&connectionLedTimer, flashConnectionLed, (void*)&connectionLed);
#elif defined ESP32
	
#endif
	//pinMode (BUILTIN_LED, OUTPUT);
	//digitalWrite (BUILTIN_LED, HIGH);
	startConnectionFlash (100);


	if (!GwOutput.loadConfig ()) {
		DEBUG_WARN ("Error reading config file");
	}

	EnigmaIOTGateway.setRxLed (BLUE_LED);
	EnigmaIOTGateway.setTxLed (RED_LED);
	EnigmaIOTGateway.onNewNode (newNodeConnected);
	EnigmaIOTGateway.onNodeDisconnected (nodeDisconnected);
	EnigmaIOTGateway.onWiFiManagerStarted (wifiManagerStarted);
	EnigmaIOTGateway.onWiFiManagerExit (wifiManagerExit);
	EnigmaIOTGateway.onDataRx (processRxData);
	EnigmaIOTGateway.begin (&Espnow_hal,NULL,false); // Disable counter check for performance

#if CONNECT_TO_WIFI_AP == 1
	WiFi.mode (WIFI_AP_STA);
	WiFi.begin ();
	EnigmaIOTGateway.configWiFiManager ();
#else
	//WiFi.mode (WIFI_AP);
#endif // CONNECT_TO_WIFI_AP


	WiFi.softAP (EnigmaIOTGateway.getNetworkName (), EnigmaIOTGateway.getNetworkKey());
	stopConnectionFlash ();

	DEBUG_INFO ("STA MAC Address: %s", WiFi.macAddress ().c_str());
	DEBUG_INFO ("AP MAC Address: %s", WiFi.softAPmacAddress().c_str ());
	DEBUG_INFO ("BSSID Address: %s", WiFi.BSSIDstr().c_str ());
	   
	DEBUG_INFO ("IP address: %s", WiFi.localIP ().toString ().c_str ());
	DEBUG_INFO ("AP IP address: %s", WiFi.softAPIP ().toString ().c_str ());
	DEBUG_INFO ("WiFi Channel: %d", WiFi.channel ());

	DEBUG_INFO ("WiFi SSID: %s", WiFi.SSID ().c_str ());
	DEBUG_INFO ("Network Name: %s", EnigmaIOTGateway.getNetworkName ());

	GwOutput.setDlCallback (onDownlinkData);
	GwOutput.begin ();

#ifdef ESP32
	//xTaskCreate (EnigmaIOTGateway_handle, "handle", 10000, NULL, 1, &xEnigmaIOTGateway_handle);
	//xTaskCreatePinnedToCore (EnigmaIOTGateway_handle, "handle", 4096, NULL, 0, &xEnigmaIOTGateway_handle, 1);
	//xTaskCreatePinnedToCore (GwOutput_handle, "gwoutput", 10000, NULL, 2, &gwoutput_handle, 1);
#endif
	}

void loop () {

	GwOutput.loop ();
	EnigmaIOTGateway.handle ();

}
