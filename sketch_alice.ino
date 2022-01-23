#define DECODE_NEC

#include <ArduinoMqttClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include "IRremote.h"

DynamicJsonBuffer jsonBuffer(2048);
JsonObject *root;

ESP8266WebServer server(80);

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

const char* autoConnectSSID = "MqttAP";
const char* autoConnectPass = "12345678";

typedef struct {
  int pin;
  int normal;
  int status;
  char *mqtt;
  long prevent;
} INPUT_EVENT;

int inputEventsLength = 0;
INPUT_EVENT *inputEvents;

void stop(void){
  while(true){
    ESP.wdtFeed();   // пнуть собаку
    yield();
  }
}

void setup() {
  // put your setup code here, to run once:

  Serial.begin(115200);
  while ( ! Serial);

  WiFiManager wifiManager;
  wifiManager.setDebugOutput(false);
  wifiManager.autoConnect(autoConnectSSID, autoConnectPass);
  Serial.println(WiFi.localIP().toString());

  if( ! SPIFFS.begin()){
    Serial.println("Error mounting the file system");
    stop();
  }

  server.serveStatic("/",                    SPIFFS, "/index.html");
  //server.serveStatic("/favicon.png",        SPIFFS, "/favicon.png");
  server.serveStatic("/bootstrap.min.css",   SPIFFS, "/bootstrap.min.css");
  server.serveStatic("/angular.js",          SPIFFS, "/angular.js");
  server.serveStatic("/bootstrap.min.js",    SPIFFS, "/bootstrap.min.js");
  server.serveStatic("/jquery.min.js",       SPIFFS, "/jquery.min.js");
  server.serveStatic("/app.js",              SPIFFS, "/app.js");
  server.serveStatic("/data.json",           SPIFFS, "/data.json");

  server.on("/save", HTTP_POST, handleSave);

  server.onNotFound([]() {
    server.send(404, "text/plain", "404: Not Found");
  });

  server.begin();

  if(loadConfig()){
    prepareConfig();
    return;
  }

  Serial.println("Error loading config");
}

void prepareConfig(void) {
  inputEventsLength = 0;
  free(inputEvents);
  
  JsonArray &commands = root->get<JsonArray>("init");
  for (size_t i = 0; i < commands.size(); i++) {
    JsonObject &item = commands.get<JsonObject>(i);

    String function = item.get<const char *>("function");

    if(function.equals("output")){
      int pin = item.get<int>("pin");
      int def = 0;

      if(item.containsKey("default")){
         def = item.get<int>("default");
      }
      
      pinMode(pin, OUTPUT);
      digitalWrite(pin, def);
    }

    if(function.equals("input")){
      int pin = item.get<int>("pin");
      int normal = item.get<int>("normal");
      const char *mqtt = item.get<const char *>("mqtt");
      pinMode(pin, INPUT);

      Serial.print("Init input with mqtt[");
      Serial.print(pin, DEC);
      Serial.print("] ");
      Serial.println(mqtt);

      if(inputEventsLength == 0){
        Serial.println("malloc memory");
        inputEvents = (INPUT_EVENT *)malloc(sizeof(INPUT_EVENT));
      } else {
        Serial.println("realloc memory");
        inputEvents = (INPUT_EVENT *)realloc(inputEvents, inputEventsLength * sizeof(INPUT_EVENT));
      }

      if(inputEvents == NULL){
        Serial.println("Can't allocate memory for input events");
        while(true);
      }

      INPUT_EVENT &event = inputEvents[inputEventsLength];
      event.pin     = pin;
      event.normal  = normal ? HIGH : LOW;
      event.mqtt    = (char *)malloc(strlen(mqtt)+1);
      event.status  = normal;
      event.prevent = 0;

      strcpy(event.mqtt, mqtt);

      inputEventsLength++;
    }

    if(function.equals("ir")){
      int pin = item.get<int>("pin");
      IrSender.begin(pin, ENABLE_LED_FEEDBACK);
    }
  }
}

bool loadConfig(void) {
  File file = SPIFFS.open("/data.json", "r");
 
  if (!file) {
    Serial.println("Failed to open file for reading");
    return false;
  }

  String content = "";
  while (file.available()){
    content += file.readString();
  }

  file.close();

  root = &jsonBuffer.parseObject(content);

  if( ! root->success()) {
    Serial.println("parseObject() failed");
    return false;
  }

  return true;
}

void handleSave() {
  if(server.hasArg("data") && server.arg("data") != NULL){
    root = &jsonBuffer.parseObject(server.arg("data"));

    if( ! root->success()) {
      server.send(500, "text/plain", "parseObject() failed");
      loadConfig();
      return;
    }

    saveToFile();
    prepareConfig();
  }

  server.send(204, "text/plain", "");
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

void handleMqtt(void){
  const char* host = root->get<const char*>("host");
  const char* topic = root->get<const char*>("topic");
  int port = (int)root->get<int>("port");
  
  if(host == NULL || topic == NULL){
    return;
  }
  
  if( ! mqttClient.connected()){

    Serial.print("Connecting to ");
    Serial.print(host);
    Serial.print(":");
    Serial.println(port);
    if ( ! mqttClient.connect(host, port)) {
      Serial.print("MQTT connection failed! Error code = ");
      Serial.println(mqttClient.connectError());

      delay(1000);
      return;
    }

    Serial.print("Subscribe to ");
    Serial.println(topic);
    mqttClient.subscribe(topic);
  }

  int messageSize = mqttClient.parseMessage();
  if (messageSize) {
    String tpc = String(mqttClient.messageTopic());
    
    // we received a message, print out the topic and contents
    Serial.print("Received a message with topic '");
    Serial.print(tpc);
    Serial.print("', length ");
    Serial.print(messageSize);
    Serial.println(" bytes:");

    char data[100];
    int i = 0;

    // use the Stream interface to print the contents
    while (mqttClient.available()) {
      data[i++] = (char)mqttClient.read();
      data[i] = 0;
    }

    int number = String(data).toInt();

    // @TODO: find conmmand in root["commands"] and parse params
    JsonArray &commands = root->get<JsonArray>("commands");
    for (size_t i = 0; i < commands.size(); i++) {
      JsonObject &item = commands.get<JsonObject>(i);
  
      String cmd = item.get<const char *>("cmd");
  
      if(cmd.equals(tpc)){
        String func  = item.get<const char *>("function");
        String type  = item.get<const char *>("type");
        int pin      = 0;
        bool reverse = false;

        if(item.containsKey("reverse")){
          reverse = item.get<bool>("reverse");
        }

        if(item.containsKey("pin")){
          pin = item.get<int>("pin");
        }

        if(func.equals("output")){
          if(reverse){
            digitalWrite(pin, number > 0 ? LOW : HIGH);
          } else {
            digitalWrite(pin, number > 0 ? HIGH : LOW);
          }
        }

        if(func.equals("ir")){
          if(type.equals("digit")){
            int ch   = item.get<int>("channel");
            int freq = item.get<int>("frequency");
            for(i = 0; i < messageSize; i++){
              IrSender.sendNEC(ch, data[i] - 0x30, freq);
              delay(200);
            }
          }

          if(type.equals("command")){
            int n    = item.get<int>("number");
            int freq = item.get<int>("frequency");
            IrSender.sendNEC(n, freq);
          }

          if(type.equals("bool")){
            int freq = item.get<int>("frequency");

            if(number > 0){
              IrSender.sendNEC(item.get<int>("true"), freq);
            } else {
              IrSender.sendNEC(item.get<int>("false"), freq);
            }
            
          }
        }
      }
 
    }

  }
}

void handleSensor() {
  for(int i = 0; i < inputEventsLength; i++){
    INPUT_EVENT &event = inputEvents[i];

//    Serial.print("Event check[p:");
//    Serial.print(event.pin, DEC);
//    Serial.print(",n:");
//    Serial.print(event.normal, DEC);
//    Serial.print("] ");
//    Serial.println(event.mqtt);

    long diff = millis() - event.prevent;
    if(digitalRead(event.pin) != event.normal && diff > 1000){
      inputEvents[i].prevent = millis();

      Serial.print("Send event");
      Serial.println(event.mqtt);
  
      mqttClient.beginMessage(event.mqtt);
      if(event.status){
        event.status = 0;
        mqttClient.print(0);
      } else {
        event.status = 1;
        mqttClient.print(1);
      }
      mqttClient.endMessage();
    }
  }

}

void loop() {
  server.handleClient();
  handleMqtt();
  handleSensor();

  ESP.wdtFeed();   // пнуть собаку
  yield();
}
