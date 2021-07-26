#include <Arduino.h>
#include <ArduinoOTA.h>
#include "RemoteDebug.h" 
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266httpUpdate.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <version.h>



#define STASSID "Chillout"
#define STAPSK  "marlboro"
#define HOST_NAME "Bedroom_Sen"
#define SECONDS 1000000
#define USE_SERIAL Serial

#define MQTT_SERVER "192.168.0.199"
#define MQTT_PORT 1883
#define mqtt_user "123"      
#define mqtt_password "123"  
//#define device_name  "Mesh01"
#define small_device_name "Mesh01" //6characters small name

#define MQTT_MAX_RETRIES 5 //how many times to retry is mqtt server is down

#define DHTPIN 2     // Digital pin connected to the DHT sensor

// Uncomment the type of sensor in use:
#define DHTTYPE    DHT11     // DHT 11
//#define DHTTYPE    DHT22     // DHT 22 (AM2302)
//#define DHTTYPE    DHT21     // DHT 21 (AM2301)

DHT dht(DHTPIN, DHTTYPE);

// current temperature & humidity, updated in loop()
float t = 0.0;
float h = 0.0;
constexpr char Topic[50] = "main/living/temperature/bedroom";
constexpr char Device_Name[7] = "Mesh01";


RemoteDebug Debug;
// GPIO where the DS18B20 is connected to
const int oneWireBus = 3;     

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);

// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);
ADC_MODE(ADC_VCC);
WiFiClient espClient;
PubSubClient MQTTclient(espClient);

int mqttRetryCounter = 0;

void MqttConnect()
{
	String clientId = small_device_name;
	char espname[14];
	
	while ((!MQTTclient.connected()) &&
			(mqttRetryCounter < MQTT_MAX_RETRIES))
	{
		Serial.print("Connecting to MQTT broker ...");
		debugA("Connecting to MQTT broker ...");
		clientId += "-";
		clientId += String(random(0xffff), HEX);
		Serial.printf("MQTT connecting as client %s...\n", clientId.c_str());
			debugA("MQTT connecting as client %s...\n", clientId.c_str());
		clientId.toCharArray(espname, 12);
		if (MQTTclient.connect(espname, mqtt_user, mqtt_password)) {
			Serial.println("OK");
			debugA("OK");
		}
		else {
			Serial.print("KO, error : ");
			debugA("KO error:");
			Serial.print(MQTTclient.state());
			
			Serial.println(" Wait 5 secondes before to retry");
			debugA("wait5");
			mqttRetryCounter++;
			delay(5000);
		}
	}
}



void setup()
{
	Serial.begin(115200);
	pinMode(oneWireBus, OUTPUT);
	pinMode(DHTPIN, OUTPUT);
#pragma region WIFI
  WiFi.hostname(HOST_NAME);
  WiFi.mode(WIFI_STA);
  WiFi.begin(STASSID, STAPSK);

  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
#pragma endregion


Debug.setResetCmdEnabled(true);
Debug.begin(HOST_NAME);

MQTTclient.setServer(MQTT_SERVER, MQTT_PORT);
  	
#pragma region OTA
	ArduinoOTA.onStart([]() {
		String type;
		if (ArduinoOTA.getCommand() == U_FLASH) {
			type = "sketch";
		}
		else { // U_FS
			type = "filesystem";
		}

		// NOTE: if updating FS this would be the place to unmount FS using FS.end()
		Serial.println("Start updating " + type);
		});
	ArduinoOTA.onEnd([]() {
		Serial.println("\nEnd");
		});
	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
		});
	ArduinoOTA.onError([](ota_error_t error) {
		Serial.printf("Error[%u]: ", error);
		if (error == OTA_AUTH_ERROR) {
			Serial.println("Auth Failed");
		}
		else if (error == OTA_BEGIN_ERROR) {
			Serial.println("Begin Failed");
		}
		else if (error == OTA_CONNECT_ERROR) {
			Serial.println("Connect Failed");
		}
		else if (error == OTA_RECEIVE_ERROR) {
			Serial.println("Receive Failed");
		}
		else if (error == OTA_END_ERROR) {
			Serial.println("End Failed");
		}
		});
	ArduinoOTA.begin();
#pragma endregion

sensors.begin();
dht.begin();


}

void CheckForUpdates()
{
	WiFiClient myhttpClient;
	
	debugA("Searching for updated version...");
	
	if(myhttpClient.connect("192.168.0.199",80))
	{
		debugA("Connected to update server");

		// Make an HTTP GET request
		myhttpClient.println("GET /Mesh/versioning HTTP/1.1");
		myhttpClient.print("Host: ");
		myhttpClient.println("192.168.0.199");
		myhttpClient.println("Connection: close");
		myhttpClient.println();

		//get file contents
		String ret = myhttpClient.readString();

		//last line cotains current version
		int latestVersion = ret.substring(ret.lastIndexOf("\n")+1,ret.length()).toInt();
		int currentVersion = (String (BUILD_NUMBER).toInt());
		
		String hexname = String(Device_Name) + "_v" + String(latestVersion) + ".bin";
		
		if (currentVersion < latestVersion )
		{
			debugA("Trying update from v%d to v%d",currentVersion,latestVersion);	

			t_httpUpdate_return ret = ESPhttpUpdate.update(myhttpClient, "192.168.0.199", 80, "/Mesh/"+hexname);			
			switch(ret) 
			{
				case HTTP_UPDATE_FAILED:
					debugA("[update] Update failed.");
					break;
				case HTTP_UPDATE_NO_UPDATES:
					debugA("[update] Update no Update.");
					break;
				case HTTP_UPDATE_OK:
					debugA("[update] Update ok."); // may not be called since we reboot the ESP
					break;
			}
		}
		else
		{
			debugA("No Update. Current v%d, latest v%d",currentVersion,latestVersion);	
		}
		//debugA("Start:");				
		//debugA("extras: %s",ret.c_str());
		//debugA("zecimal: %d",ret.toInt());
		//debugA("End");
		//ret.substring(ret.lastIndexOf("\n"),ret.length());

	}
	else
	{
		debugA("cant connect");
	}
	//create furmware name: currentVersion+1

	/*
	
	}*/

}

int taskCounter = 0;

void loop() {
  
	taskCounter++ ;
	mqttRetryCounter = 0;

	debugA("Starting task %d timestamp %d",taskCounter,millis());
	//Serial.print("Starting task %d timestamp %d",taskCounter,millis());
	Serial.print("Starting task");
	Serial.println(taskCounter);

	debugA("  CheckUpdate %d",millis());
	CheckForUpdates();

  	if (!MQTTclient.connected())
	{
		MqttConnect();
	}

	debugA("  Runnables %d",millis());
	ArduinoOTA.handle();
	Debug.handle();
	MQTTclient.loop();
	
	debugA("  Onewire %d",millis());
	sensors.requestTemperatures(); 
	float temperatureC = sensors.getTempCByIndex(0);
	//debugA("Temp C: %f",temperatureC);
	
	debugA("  DHT %d",millis());
	 // Read Humidity
    float newH = dht.readHumidity();
	 // if humidity read failed, don't change h value 
    if (isnan(newH)) {
      //debugA("Failed to read from DHT sensor!");
    }
    else {
      h = newH;
	 // debugA("Hum %: %f",h);
	}

	debugA("  Json+mqtt %d",millis());
	char JSONmessageBuffer[100];
	  	  
	StaticJsonDocument<600> JSONbuffer;
	JSONbuffer["Temperature"] = temperatureC;
	JSONbuffer["Humidity"] = h;
	JSONbuffer["Supply"] = ESP.getVcc()/1000.0f;

	serializeJson(JSONbuffer, JSONmessageBuffer, sizeof(JSONmessageBuffer));
	debugA("%s",JSONmessageBuffer);
	MQTTclient.publish(Topic, JSONmessageBuffer, false);




	//debugA("Supply V: %f",(float)ESP.getVcc()/1024.00f);

	delay(100);
	ESP.deepSleep(300 * SECONDS);
	delay(100);
}