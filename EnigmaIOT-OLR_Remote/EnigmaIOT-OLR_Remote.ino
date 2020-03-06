/**
  * @file enigmaiot_node_nonsleepy.ino
  * @version 0.8.1
  * @date 17/01/2020
  * @author German Martin
  * @brief Node based on EnigmaIoT over ESP-NOW, in non sleeping mode
  *
  * Sensor reading code is mocked on this example. You can implement any other code you need for your specific need
  */

#ifndef ESP8266
#error Node only supports ESP8266 platform
#endif

#include <Arduino.h>
#include <EnigmaIOTNode.h>
#include <espnow_hal.h>
#include <CayenneLPP.h>

#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <Curve25519.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include <ESPAsyncTCP.h>
#include <Hash.h>
#include <DNSServer.h>

#include <DebounceEvent.h>

#define BLUE_LED LED_BUILTIN
#define OLR_BUTTON 0 // D3

bool button_pushed = false;
bool button_released = false;
bool reset_config = false;

const time_t MAX_IDLE_TIME = 600000; // After 10 minutes force deep sleep
time_t last_activity = 0;

void callback (uint8_t pin, uint8_t event, uint8_t count, uint16_t length);

DebounceEvent button = DebounceEvent (OLR_BUTTON, callback, BUTTON_PUSHBUTTON | BUTTON_DEFAULT_HIGH | BUTTON_SET_PULLUP, 10, 25);

void callback (uint8_t pin, uint8_t event, uint8_t count, uint16_t length) {
	if (event == EVENT_PRESSED) {
		button_released = false;
		button_pushed = true;
		last_activity = millis ();
		//digitalWrite (BUILTIN_LED, LOW);
	} else if (event == EVENT_RELEASED) {
		if (length > 10000) {
			DEBUG_WARN ("Reset triggered %d ms", length);
			reset_config = true;
		}
		button_pushed = false;
		button_released = true;
		//digitalWrite (BUILTIN_LED, HIGH);
	}
}


void connectEventHandler () {
	Serial.println ("Connected");
	last_activity = millis ();
}

void disconnectEventHandler () {
	Serial.println ("Disconnected");
}

void processRxData (const uint8_t* mac, const uint8_t* buffer, uint8_t length, nodeMessageType_t command) {
	char macstr[18];
	String commandStr;

	mac2str (mac, macstr);
	Serial.println ();
	Serial.printf ("Data from %s --> %s\n", macstr, printHexBuffer (buffer, length));
	if (command == nodeMessageType_t::DOWNSTREAM_DATA_GET)
		commandStr = "GET";
	else if (command == nodeMessageType_t::DOWNSTREAM_DATA_SET)
		commandStr = "SET";
	else
		return;

	Serial.printf ("Command %s\n", commandStr.c_str ());
	Serial.printf ("Data: %s\n", printHexBuffer (buffer, length));

}

void setup () {

	Serial.begin (115200); Serial.println (); Serial.println ();
	pinMode (BLUE_LED, OUTPUT);
	digitalWrite (BLUE_LED, HIGH); // Turn off LED
	EnigmaIOTNode.setLed (BLUE_LED);
	EnigmaIOTNode.onConnected (connectEventHandler);
	EnigmaIOTNode.onDisconnected (disconnectEventHandler);
	EnigmaIOTNode.onDataRx (processRxData);

	EnigmaIOTNode.begin (&Espnow_hal, NULL, NULL, true, false);

	last_activity = millis ();
}

void loop () {
	static int last_button;
	static int diff_button;

	EnigmaIOTNode.handle ();

	CayenneLPP msg (20);
	if (button_pushed || button_released) {

		if (button_pushed) {
			msg.addDigitalOutput (0, 1);
			button_pushed = false;
		} else if (button_released) {
			msg.addDigitalOutput (0, 0);
			button_released = false;
		}

		if (!EnigmaIOTNode.sendData (msg.getBuffer (), msg.getSize ())) {
			Serial.println ("Error sending data");
		} else {
			//Serial.println ("Data sent");
		}

	}

	if (button_released || reset_config) {
		DEBUG_WARN ("Reset config");
		EnigmaIOTNode.resetConfig ();
	}

	button.loop ();

	if (millis () - last_activity >= MAX_IDLE_TIME) {
		DEBUG_WARN ("Idle time exceeded limit of %d minutes. Going to sleep mode", MAX_IDLE_TIME/60000);
		ESP.deepSleep (0);
	}

	// Test code
	//static time_t lastSensorData;
	//static const time_t SENSOR_PERIOD = 1000;
	//if (millis () - lastSensorData > SENSOR_PERIOD) {
	//	lastSensorData = millis ();
	//	
	//	// Read sensor data
	//	msg.addUnixTime (0, lastSensorData);
	//	//msg.addAnalogInput (0, (float)(ESP.getVcc ()) / 1000);
	//	//Serial.printf ("Vcc: %f\n", (float)(ESP.getVcc ()) / 1000);
	//	//msg.addTemperature (1, 20.34);
	//	// Read sensor data
	//	
	//	Serial.printf ("Trying to send: %s\n", printHexBuffer (msg.getBuffer (), msg.getSize ()));

	//	if (!EnigmaIOTNode.sendUnencryptedData (msg.getBuffer (), msg.getSize ())) {
	//		Serial.println ("---- Error sending data");
	//	} else {
	//		//Serial.println ("---- Data sent");
	//	}

	//}
}
