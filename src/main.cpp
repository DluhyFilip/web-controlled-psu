//zahrnutí všech potřebných knihoven
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"
#include <Arduino_JSON.h>
#include <math.h>
//určení názvu a hesla pro vytvořenou wifi síť
const char* ssid = "napájecí zdroj";
const char* password = "12345678";
//definice používaných vstupů a výstupů
#define DAC_CH1 25
#define DAC_CH2 26
#define led 32
//vytvoření serveru na portu 80
AsyncWebServer server(80);
//vytvoření instance pro odeslání hodnot měřeného proudu na webovou stránku
AsyncEventSource events("/events");
//proměnné pro zápis zadávaných hodnot
const char* PARAM_INPUT_1 = "input1";
const char* PARAM_INPUT_2 = "input2";
//vytvoření potřebných proměnných
float prepocetnapeti;
float prepocetproudu;
float napeti;
float proud;
String input1;
String input2;

JSONVar readings;
//proměnné pro časování odelílání hodnot proudu na server
unsigned long lastTime = 0;
unsigned long timerDelay = 1000;
//funkce pro připojení SPIFFS
void initSPIFFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println("An error has occurred while mounting SPIFFS");
  } else {
    Serial.println("SPIFFS mounted successfully");
  }
}
//funkce getsensorReadings() měří proud protékající obvodem a uložení zadané hodnoty napětí
String getSensorReadings() {
  readings["napeti"] = String(input1);
  readings["proud"] = String((analogRead(33) * 3.15 / 4095) * 1000);
  String jsonString = JSON.stringify(readings);
  return jsonString;
}

void setup() {
  pinMode(led, OUTPUT);

  Serial.begin(115200);
//vytvoření wifi sítě s názvem a heslem uložený v proměnných ssid a password
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  digitalWrite(led, HIGH);
  Serial.print("AP IP address: ");
  Serial.println(IP);

  initSPIFFS();
//pokyn pro webserver (pokud přijde "/" webserver si vyžádá soubor "/index" uložený ve SPIFFS)
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(SPIFFS, "/index.html", "text/html");
  });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(SPIFFS, "/style.css", "text/css");
  });

  server.on("/readings", HTTP_GET, [](AsyncWebServerRequest* request) {
    String json = getSensorReadings();
    request->send(200, "application/json", json);
    json = String();
  });


  server.on("/", HTTP_POST, [](AsyncWebServerRequest* request) {
    int params = request->params();
    for (int i = 0; i < params; i++) {
      AsyncWebParameter* p = request->getParam(i);
      if (p->isPost()) {
        //zápis zadané hodnoty do proměnné input1
        if (p->name() == PARAM_INPUT_1) {
          input1 = p->value().c_str();
          //přepočet zadané hodnoty na digitální číslo pro D/A převodník
          prepocetnapeti = input1.toFloat();
          prepocetnapeti = prepocetnapeti / 10;

          napeti = 0.0053725 * prepocetnapeti * prepocetnapeti + 82.147 * prepocetnapeti - 7.0481;
          napeti = round(napeti);
          //omezení hodnoty digitálního čísla
          if (napeti > 239) {
            napeti = 239;
          }
          if (napeti < 0) {
            napeti = 0;
          }
          //zápis digitálního čísla na D/A převodník
          dacWrite(DAC_CH2, napeti);
          Serial.println(napeti, 6);
        }

        if (p->name() == PARAM_INPUT_2) {
          input2 = p->value().c_str();

          prepocetproudu = input2.toFloat();
          prepocetproudu = prepocetproudu / 1000;

          proud = 0.0053725 * prepocetproudu * prepocetproudu + 82.147 * prepocetproudu - 7.0481;
          proud = round(proud);

          if (proud > 239) {
            proud = 239;
          }
          if (proud < 0) {
            proud = 0;
          }

          dacWrite(DAC_CH1, proud);
          Serial.println(proud, 6);
        }
      }
    }
    request->send(SPIFFS, "/index.html", "text/html");
  });
  //kontrola připojení instance pro odeslání hodnot měřeného proudu na webovou stránku
  events.onConnect([](AsyncEventSourceClient* client) {
    if (client->lastId()) {
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    client->send("hello!", NULL, millis(), 10000);
  });
  server.addHandler(&events);
  
  server.begin();
}

void loop() {
  //odeslání naměřené hodnoty na server každou 1 sekundu
  if ((millis() - lastTime) > timerDelay) {
    events.send("ping", NULL, millis());
    events.send(getSensorReadings().c_str(), "new_readings", millis());
    lastTime = millis();
  }
}