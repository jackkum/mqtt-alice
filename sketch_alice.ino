#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <ArduinoJson.h>

DynamicJsonBuffer jsonBuffer(2048);
JsonObject *root;

ESP8266WebServer server(80);

void setup() {
  // put your setup code here, to run once:

  if( ! SPIFFS.begin()){
    Serial.println("Error mounting the file system");
    while(true);
  }

  File file = SPIFFS.open("/data.json", "r");
 
  if (!file) {
    Serial.println("Failed to open file for reading");
    while(true);
  }

  String content = "";
  while (file.available()){
    content += file.read();
  }

  file.close();

  root = &jsonBuffer.parseObject(content);

  if( ! root->success()) {
    Serial.println("parseObject() failed");
    while(true);
  }

  server.serveStatic("/", SPIFFS, "/index.html");
  //server.serveStatic("/favicon.png", SPIFFS, "/favicon.png");
  server.serveStatic("/css/bootstrap.min.css", SPIFFS, "/css/bootstrap.min.css");
  server.serveStatic("/js/angular.js", SPIFFS, "/js/angular.js");
  server.serveStatic("/js/bootstrap.min.js", SPIFFS, "/js/bootstrap.min.js");
  server.serveStatic("/js/jquery.min.js", SPIFFS, "/js/jquery.min.js");
  server.serveStatic("/js/app.js", SPIFFS, "/js/single.js");
  
  server.serveStatic("/data.json", SPIFFS, "/data.json");

  server.onNotFound([]() {
    server.send(404, "text/plain", "404: Not Found");
  });

  server.begin();

  // mqtt
  // root["host"]
  // root["port"]
  // root["topic"]
}

bool saveToFile() {
  File file = SPIFFS.open("/data.json", "w");
 
  if (!file) {
    return false;
  }

  if (root->printTo(file) == 0) {
    return false;
  }

  file.close();

  return true;
}

void loop() {
  // put your main code here, to run repeatedly:
  server.handleClient();
}
