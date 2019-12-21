#include "AC_remote.h"


File fsUploadFile;
RemoteDebug Debug;
state acState;
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdateServer;
WiFiClient espClient;
PubSubClient client(espClient);

void handleFileUpload() 
{ // upload a new file to the SPIFFS
	HTTPUpload& upload = server.upload();
	if (upload.status == UPLOAD_FILE_START) {
		String filename = upload.filename;
		if (!filename.startsWith("/")) filename = "/" + filename;
		Serial.print("handleFileUpload Name: "); Serial.println(filename);
		fsUploadFile = SPIFFS.open(filename, "w");            // Open the file for writing in SPIFFS (create if it doesn't exist)
		filename = String();
	}
	else if (upload.status == UPLOAD_FILE_WRITE) {
		if (fsUploadFile)
			fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
	}
	else if (upload.status == UPLOAD_FILE_END) {
		if (fsUploadFile) {                                   // If the file was successfully created
			fsUploadFile.close();                               // Close the file again
			Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
			server.sendHeader("Location", "/success.html");     // Redirect the client to the success page
			server.send(303);
		}
		else {
			server.send(500, "text/plain", "500: couldn't create file");
		}
	}
}
bool handleFileRead(String path) 
{ 
	// send the right file to the client (if it exists)
	Serial.println("handleFileRead: " + path);

	if (path.endsWith("/")) path += "index.html";          // If a folder is requested, send the index file
	
	String contentType = getContentType(path);             // Get the MIME type
	String pathWithGz = path + ".gz";
	if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) { // If the file exists, either as a compressed archive, or normal
		if (SPIFFS.exists(pathWithGz))                         // If there's a compressed version available
			path += ".gz";                                         // Use the compressed verion
		File file = SPIFFS.open(path, "r");                    // Open the file
		size_t sent = server.streamFile(file, contentType);    // Send it to the client
		file.close();                                          // Close the file again
		Serial.println(String("\tSent file: ") + path);
		return true;
	}
	Serial.println(String("\tFile Not Found: ") + path);   // If the file doesn't exist, return false
	return false;
}
void handleNotFound() 
{
	String message = "File Not Found\n\n";
	message += "URI: ";
	message += server.uri();
	message += "\nMethod: ";
	message += (server.method() == HTTP_GET) ? "GET" : "POST";
	message += "\nArguments: ";
	message += server.args();
	message += "\n";
	for (uint8_t i = 0; i < server.args(); i++) {
		message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
	}
	server.send(404, "text/plain", message);
}
void SetAcState(state stRequestedState){
	debugA("Setting state: ");
	debugA("Temp %d",stRequestedState.temperature);

	if (acState.powerStatus) 
	{
		ac.on();
		ac.setTemp(stRequestedState.temperature);
		if (stRequestedState.operation == 0) 
		{
			ac.setMode(auto_mode);
			ac.setFan(fan_auto);
			stRequestedState.fan = 0;
		}
		else if (stRequestedState.operation == 1) {
			ac.setMode(cool_mode);
		}
		else if (stRequestedState.operation == 2) {
			ac.setMode(dry_mode);
		}
		else if (stRequestedState.operation == 3) {
			ac.setMode(heat_mode);
		}
		else if (stRequestedState.operation == 4) {
			ac.setMode(fan_mode);
		}

		if (stRequestedState.operation != 0) 
		{
			if (stRequestedState.fan == 0) {
				ac.setFan(fan_auto);
			}
			else if (stRequestedState.fan == 1) {
				ac.setFan(fan_min);
			}
			else if (stRequestedState.fan == 2) {
				ac.setFan(fan_med);
			}
			else if (stRequestedState.fan == 3) {
				ac.setFan(fan_hi);
			}
		}
	}
	else 
	{
		ac.off();
	}
	ac.send();
}
void ParseInputJson(byte* payload)
{
	DynamicJsonDocument root(1024);
	DeserializationError error = deserializeJson(root, payload);
	if (!error)
	{
		if (root.containsKey("temp")) {
			acState.temperature = (uint8_t)root["temp"];
		}

		if (root.containsKey("fan")) {
			acState.fan = (uint8_t)root["fan"];
		}

		if (root.containsKey("power")) {
			acState.powerStatus = root["power"];
		}

		if (root.containsKey("mode")) {
			acState.operation = root["mode"];
		}
		SetAcState(acState);
	}
	else
	{
		Serial.println("Error in deseria");
	}
	
}
void callback(char* topic, byte* payload, unsigned int length)
{
	Serial.print("Message arrived [");
	debugA("Message arrived [");
	
	Serial.print(topic);
	debugA("%s",topic);

	Serial.print("] ");
	debugA("] ");

	for (int i=0;i<length;i++) 
	{
		Serial.print((char)payload[i]);
		debugA("%s",(char)payload[i]);
	}
	Serial.println();
	ParseInputJson(payload);
}





void reconnect()
{
	String clientId = small_device_name;
	char espname[14];
	while (!client.connected()) {
		Serial.print("Connecting to MQTT broker ...");
		debugA("Connecting to MQTT broker ...");
		clientId += "-";
		clientId += String(random(0xffff), HEX);
		Serial.printf("MQTT connecting as client %s...\n", clientId.c_str());
		debugA("MQTT connecting as client %s...\n", clientId.c_str());
		clientId.toCharArray(espname, 12);
		if (client.connect(espname, mqtt_user, mqtt_password)) {
			client.subscribe(Topic);
			Serial.println("OK");
			debugA("OK");
		}
		else {
			debugA("KO, error : ");
			debugA("%d",client.state());
			debugA(" Wait 5 secondes before to retry");
			delay(5000);
		}
	}
}

void setup()
{
	Serial.begin(115200);
	Serial.println("Booting");
	WiFi.mode(WIFI_STA);
	WiFi.hostname("ACRemote");
	WiFi.begin(ssid, password);

	while (WiFi.waitForConnectResult() != WL_CONNECTED)
	{
		Serial.println("Connection Failed! Rebooting...");
		delay(5000);
		ESP.restart();
	}
	Debug.begin(HOST_NAME);
	delay(1000);
	ac.begin();


	delay(1000);
	
	Serial.println("mounting FS...");
	debugA("mounting FS...");

	if (!SPIFFS.begin()) {
		Serial.println("Failed to mount file system");
		return;
	}
	httpUpdateServer.setup(&server);
#pragma region HTTPServer
	server.on("/state", HTTP_PUT, []() {
		DynamicJsonDocument root(1024);
		DeserializationError error = deserializeJson(root, server.arg("plain"));
		if (error) {
			server.send(404, "text/plain", "FAIL. " + server.arg("plain"));
		}
		else {
			if (root.containsKey("temp")) {
				acState.temperature = (uint8_t)root["temp"];
			}

			if (root.containsKey("fan")) {
				acState.fan = (uint8_t)root["fan"];
			}

			if (root.containsKey("power")) {
				acState.powerStatus = root["power"];
			}

			if (root.containsKey("mode")) {
				acState.operation = root["mode"];
			}

			String output;
			serializeJson(root, output);
			server.send(200, "text/plain", output);

			delay(200);
			SetAcState(acState);

		}
		});

	server.on("/file-upload", HTTP_POST,                       // if the client posts to the upload page
		[]() {
			server.send(200);
		},                          // Send status 200 (OK) to tell the client we are ready to receive
		handleFileUpload                                    // Receive and save the file
			);

	server.on("/file-upload", HTTP_GET, []() {                 // if the client requests the upload page

		String html = "<form method=\"post\" enctype=\"multipart/form-data\">";
		html += "<input type=\"file\" name=\"name\">";
		html += "<input class=\"button\" type=\"submit\" value=\"Upload\">";
		html += "</form>";
		server.send(200, "text/html", html);
		});

	server.on("/", []() {
		server.sendHeader("Location", String("ui.html"), true);
		server.send(302, "text/plain", "");
		});

	server.on("/state", HTTP_GET, []() {
		DynamicJsonDocument root(1024);
		root["mode"] = acState.operation;
		root["fan"] = acState.fan;
		root["temp"] = acState.temperature;
		root["power"] = acState.powerStatus;
		String output;
		serializeJson(root, output);
		server.send(200, "text/plain", output);
		});


	server.on("/reset", []() {
		server.send(200, "text/html", "reset");
		delay(100);
		ESP.reset();
		});

	server.serveStatic("/", SPIFFS, "/", "max-age=86400");

	server.onNotFound(handleNotFound);

	server.begin();
#pragma endregion HTTPServer
	/*OlPa Start*/

  	client.setServer(MQTT_SERVER, MQTT_PORT);
  	client.setCallback(callback);

	/*OlPa end*/

	/*OTA Start*/
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
	Serial.println("Ready");
	Serial.print("IP address: ");
	Serial.println(WiFi.localIP());
	/*OTA END*/	
}

void loop() {
	//debugA("Looping");
	if (!client.connected()) 
	{
    	reconnect();
  	}
  	client.loop();
	Debug.handle();
	ArduinoOTA.handle();
	server.handleClient();
}