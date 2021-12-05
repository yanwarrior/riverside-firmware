#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "ThingSpeak.h"

IPAddress local_ip(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

bool stateWiFiConnectionProgress  = false;
String deviceName = "RiverSide";
String accessPointSSID = deviceName + " - " + ESP.getChipId();
String accessPointPassword = "12345678";
float vref                        = 3.3;
float resolution              = vref / 1023.0;
float temperature             = 0.0;
unsigned long channelNumber   = 0;
int data                      = 0;
String apiKeyThingSpeak       = "";
bool thingSpeakStatus         = false;

String ssid           = "";
String password       = "";
String apSSID         = "RiverSide IoT - Knobytez";
String apPassword     = "12345678";
bool wifiOpen         = false;
String apIP           = "192.168.1.1";
int myTone            = 0;

WiFiClient client;
AsyncWebServer server(80);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
  <head>
    <title>RiverSide WiFi Manager</title>
    <style>
      *{
        margin: 0;
        padding: 0;
        outline: 0;
        font-family: 'Open Sans', sans-serif;
      }
      body{
        height: 100vh;
        background-size: cover;
        background-position: center;
        background-repeat: no-repeat;
      }

      .container{
        position: absolute;
        left: 50%;
        top: 50%;
        transform: translate(-50%,-50%);
        padding: 20px 25px;
        width: 300px;

        background-color: rgba(0,0,0,.7);
        box-shadow: 0 0 10px rgba(255,255,255,.3);
      }
      .container h1{
        text-align: left;
        color: #fafafa;
        margin-bottom: 30px;
        text-transform: uppercase;
        border-bottom: 4px solid #2979ff;
      }
      .container label{
        text-align: left;
        color: #90caf9;
      }
      .container form input{
        width: calc(100% - 20px);
        padding: 8px 10px;
        margin-bottom: 15px;
        border: none;
        background-color: transparent;
        border-bottom: 2px solid #2979ff;
        color: #fff;
        font-size: 20px;
      }
      .container form button{
        width: 100%;
        padding: 5px 0;
        border: none;
        background-color:#2979ff;
        font-size: 18px;
        color: #fafafa;
      }
    </style>
  </head>
   
  <body>
    <div class="container">
      <h1>RiverSide</h1>
      <form action="/wifi/connect">
        <label>SSID</label><br>
        <input name="ssid" type="text"><br>
        <label>Password</label><br>
        <input name="password" type="password"><br>
        <button>Log in</button>
      </form>
    </div>   
  </body>
</html>
)rawliteral";


const char river_side_literals[] PROGMEM = R"rawliteral(

█▀█ █ █░█ █▀▀ █▀█
█▀▄ █ ▀▄▀ ██▄ █▀▄
█▀ █ █▀▄ █▀▀
▄█ █ █▄▀ ██▄
𝚋𝚢 𝚁𝚒𝚟𝚎𝚛𝙻𝚊𝚋𝚜, 2021

Version 1.0.0
)rawliteral";

void httpConnectWiFi(AsyncWebServerRequest *request) {
  if (request->hasParam("ssid") && request->hasParam("password")) {
    ssid = request->getParam("ssid")->value();
    password = request->getParam("password")->value();
    stateWiFiConnectionProgress = true;
    request->send(200, "text/plain", "Connecting WiFi");
  } else {
    request->send(200, "text/plain", "SSID & Password required!");
  }
}


void systemConnectWiFi() {
  if (stateWiFiConnectionProgress) {
    delay(1000);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      Serial.println("Try to connect...");
      delay(1000);
    }
    Serial.println("WiFi connected!");
    Serial.println(WiFi.localIP());
    stateWiFiConnectionProgress = false;
    
    Serial.println(WiFi.localIP());
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    digitalWrite(LED_BUILTIN, LOW);
  }
}


void systemSetupRiverSide() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  // configure wifi ap
  WiFi.softAPConfig(local_ip, gateway, subnet);
  WiFi.softAP(accessPointSSID, accessPointPassword); 

  Serial.println(river_side_literals);
  Serial.println("http://" + apIP);
  delay(100);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });
  
  server.on("/builtin/led/on", HTTP_GET, builtinLEDON);
  server.on("/builtin/led/off", HTTP_GET, builtinLEDOFF);
  server.on("/sensors/lm35", HTTP_GET, getTemperature);
  server.on("/thingspeak/write", HTTP_GET, writeThingSpeak);
  server.on("/thingspeak/stop", HTTP_GET, stopThingSpeak);
  server.on("/voices/tone", HTTP_GET, beep);
  server.on("/wifi/connect", HTTP_GET, httpConnectWiFi);
  server.on("/wifi/ip", HTTP_GET, getWifiIP);
  server.on("/wifi/ssid", HTTP_GET, getWifiSSID);
 
  server.begin();
  delay(200);
  Serial.println(accessPointSSID);
}


String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;
 
  for(int i=0; i<=maxIndex & found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  } 
 
  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void getWifiIP(AsyncWebServerRequest *request) {
  Serial.print("[wifi-ip] your WiFi IP ");
  Serial.println(WiFi.localIP());
  request->send(200, "text/plain", WiFi.localIP().toString().c_str());
}

void getWifiSSID(AsyncWebServerRequest *request) {
  Serial.print("[ssid] your SSID ");
  Serial.println(ssid);
  request->send(200, "text/plain", ssid);
}

void builtinLEDON(AsyncWebServerRequest *request) {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  delay(1000);
  Serial.println("[led] LED is ON");
  request->send(200, "text/plain", "[led] LED is ON");
}

void builtinLEDOFF(AsyncWebServerRequest *request) {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1000);
  Serial.println("[led] LED is OFF");
  request->send(200, "text/plain", "[led] LED is OFF");
}

void getTemperature(AsyncWebServerRequest *request) {
  if (request->hasParam("pin")) {
    if (request->getParam("pin")->value() == "A0") {
      temperature = analogRead(A0);
      temperature = (temperature * resolution);
      temperature = temperature * 100.0;
      Serial.print("[temperature] Temp is ");
      Serial.print(String(temperature));
      Serial.println(" Celcius");
      request->send(200, "text/plain", String(temperature));
    } else {
      Serial.println("[temperature] You must set analog pin (A0)!");
      request->send(400, "text/plain", "[temperature] You must set analog pin (A0)!");
    }
    delay(150);
  } else {
    Serial.println("[temperature] Required analog pin name!");
    request->send(400, "text/plain", "[temperature] Required analog pin name!");
  }
}

void syncWriteThingSpeak() {
  ThingSpeak.begin(client);
  int apiKeyLen = apiKeyThingSpeak.length() + 1;
  char apiKey[apiKeyLen];
  apiKeyThingSpeak.toCharArray(apiKey, apiKeyLen);
  int status = ThingSpeak.writeField(channelNumber, 1, data, apiKey);
  Serial.println("ThingSpeak worked!");
  Serial.print("Status: ");
  Serial.println(status);
  Serial.print("Channel: ");
  Serial.println(channelNumber);
  Serial.print("Data: ");
  Serial.println(data);
  Serial.print("ApiKey: ");
  Serial.println(apiKeyThingSpeak);
  delay(1000);
}

void writeThingSpeak(AsyncWebServerRequest *request) {
  if (request->hasParam("api_key") && request->hasParam("channel_number") && request->hasParam("data")) {
    apiKeyThingSpeak = request->getParam("api_key")->value();
    
    channelNumber = request->getParam("channel_number")->value().toInt();
    data = request->getParam("data")->value().toInt();
    thingSpeakStatus = true;
    Serial.println("[thingspeak] Writing thingspeak.io!");
    request->send(200, "text/plain", "[thingspeak] Writing thingspeak.io!");
  } else {
    Serial.println("[thingspeak] Please set your api key and channel number!");
    request->send(400, "text/plain", "[thingspeak] Please set your api key and channel number!");
  }
}


void stopThingSpeak (AsyncWebServerRequest *request) {
  thingSpeakStatus = false;
  Serial.println("[thingspeak] Stoping thingspeak.io!");
  request->send(200, "text/plain", "[thingspeak] Stoping thingspeak.io!");
}

void beep(AsyncWebServerRequest *request) {
  if (request->hasParam("tone")) {
    int speaker = D7;
    pinMode(speaker, OUTPUT);
    myTone = request->getParam("tone")->value().toInt();
    if (myTone == 1) {
      tone(speaker, 262, 500);
      delay(100);
    } else if (myTone == 2) {
      tone(speaker, 294, 500);
      delay(100);
    } else if (myTone == 3) {
      tone(speaker, 330, 500);
      delay(100);
    } else if (myTone == 4) {
      tone(speaker, 349, 500);
      delay(100);
    } else if (myTone == 5) {
      tone(speaker, 395, 500);
      delay(100);
    } else if (myTone == 6) {
      tone(speaker, 440, 500);
      delay(100);
    } else if (myTone == 7) {
      tone(speaker, 494, 500);
      delay(100);
    } else if (myTone == 8) {
      tone(speaker, 523, 500);
      delay(100);
    } else {
      tone(speaker, 262, 500);
      delay(100); 
    }
    Serial.println("[beep] Beep is running!");
    request->send(200, "text/plain", "[beep] Beep is running!");

  } else {
    Serial.println("[beep] Beep is invalid data");
    request->send(400, "text/plain", "[beep] Beep is invalid data");
  }
}


void loginWiFiStation(AsyncWebServerRequest *request) {
  
  if (request->hasParam("ssid") && request->hasParam("password")) {
    ssid =  request->getParam("ssid")->value();
    password = request->getParam("password")->value();
    WiFi.disconnect();
//    WiFi.mode(WIFI_STA);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    wifiOpen = true;
    request->send(200, "text/plain", "Waiting connection");
  }
  request->send(400, "text/plain", "Ups!");
}



void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.begin(115200);
  systemSetupRiverSide();
}   

void loop() {
  if (thingSpeakStatus && WiFi.status() == WL_CONNECTED) {
    syncWriteThingSpeak();
  }

  systemConnectWiFi();
}
