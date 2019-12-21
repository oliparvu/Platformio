#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <FS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>

//// ###### User configuration space for AC library classes ##########

#include <ir_Coolix.h> // replace library based on your AC unit model, check https://github.com/markszabo/IRremoteESP8266
#include "RemoteDebug.h"        //https://github.com/JoaoLopesF/RemoteDebug
#include <PubSubClient.h>

#define auto_mode kCoolixAuto
#define cool_mode kCoolixCool
#define dry_mode kCoolixDry
#define heat_mode kCoolixHeat
#define fan_mode kCoolixFan
#define fan_auto kCoolixFanAuto
#define fan_min kCoolixFanMin
#define fan_med kCoolixFanMed
#define fan_hi kCoolixFanMax
#ifndef STASSID
#define STASSID "Chillout"
#define STAPSK  "marlboro"
#endif

#define HOST_NAME "remotedebug"

/*MQTT*/
constexpr auto MQTT_SERVER = "oliparvu.go.ro";
constexpr auto MQTT_PORT = 1883;
#define mqtt_user "123"      
#define mqtt_password "123"  
#define device_name  "AcRemote"
#define small_device_name "AcRemo" //6characters small name
constexpr char Topic[50] = "main/acremote";
/*End MQTT*/


const uint16_t kIrLed = 2;  // ESP8266 GPIO pin to use. Recommended: 4 (D2).
//IRCoolixAC ac(kIrLed);  // Library initialization, change it according to the imported library file.
IRCoolixAC ac(kIrLed,true);  // inverted

/*
  if (inverted) {
    outputOn = LOW;
    outputOff = HIGH;
  } else {
    outputOn = HIGH;
    outputOff = LOW;
  }
*/

/// ##### End user configuration ######

const char* ssid = STASSID;
const char* password = STAPSK;


struct state {
	uint8_t temperature = 22, fan = 0, operation = 0;
	bool powerStatus;
};

//settings
char deviceName[] = "AC Remote Control";


///HTTP Server Functions



String getContentType(String filename)
 {
	 // convert the file extension to the MIME type
	if (filename.endsWith(".html")) return "text/html";
	else if (filename.endsWith(".css")) return "text/css";
	else if (filename.endsWith(".js")) return "application/javascript";
	else if (filename.endsWith(".ico")) return "image/x-icon";
	else if (filename.endsWith(".gz")) return "application/x-gzip";
	return "text/plain";
}