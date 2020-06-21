
#include "AsyncJson.h"
#include "SD_MMC.h"
#include <AsyncWebDAV.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include <ESPmDNS.h>
#include <OctoPrintAPI.h>
#include <WiFi.h>

#define VERSION "1.3.10"
#define SKETCH_VERSION "2.x-localbuild"

AsyncWebServer server(80);
AsyncEventSource events("/events");
DNSServer dns;

const char *hostName = "PrusaWIFI";

void mDNSInit() {

  if (!MDNS.begin(hostName))
    return;

  // OctoPrint API
  // Unfortunately, Slic3r doesn't seem to recognize it
  MDNS.addService("octoprint", "tcp", 80);
  MDNS.addServiceTxt("octoprint", "tcp", "path", "/");
  MDNS.addServiceTxt("octoprint", "tcp", "api", VERSION);
  MDNS.addServiceTxt("octoprint", "tcp", "version", VERSION);
  MDNS.addServiceTxt("octoprint", "tcp", "model", "ESP32");
  MDNS.addServiceTxt("octoprint", "tcp", "vendor", hostName);
  MDNS.addServiceTxt("octoprint", "tcp", "mac", WiFi.macAddress());

  MDNS.addService("http", "tcp", 80);
  MDNS.addServiceTxt("http", "tcp", "path", "/");
  MDNS.addServiceTxt("http", "tcp", "api", VERSION);
  MDNS.addServiceTxt("http", "tcp", "version", SKETCH_VERSION);
  MDNS.addServiceTxt("http", "tcp", "model", "ESP32");
  MDNS.addServiceTxt("http", "tcp", "vendor", hostName);
  MDNS.addServiceTxt("http", "tcp", "mac", WiFi.macAddress());
}

void onWiFiEvent(WiFiEvent_t event) {
  switch (event) {
  case SYSTEM_EVENT_STA_START:
    Serial.println("WIFI: Connecting...");
    break;
  case SYSTEM_EVENT_STA_CONNECTED:
    Serial.println("WIFI: Connected! Waiting for IP...");
    break;
  case SYSTEM_EVENT_STA_LOST_IP:
    Serial.println("WIFI: Lost IP address...");
    break;
  case SYSTEM_EVENT_STA_GOT_IP:
    Serial.println("WIFI: Got IP!");
    Serial.print("WIFI: IP Address: ");
    Serial.println(WiFi.localIP());
    mDNSInit();
    break;
  default:
    break;
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.setHostname(hostName);
  WiFi.onEvent(onWiFiEvent);

  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("Card Mount Failed");
    return;
  }

  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }

  server.addHandler(new AsyncWebDAV("/drive", SD_MMC));
  server.addHandler(new OctoPrintAPI(SD_MMC));

  server.serveStatic("/", SD_MMC, "/ui/")
      .setDefaultFile("index.html")
      .setCacheControl("max-age=600");

  events.onConnect([](AsyncEventSourceClient *client) {
    client->send("hello!", NULL, millis(), 1000);
  });

  server.addHandler(&events);

  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });

  AsyncWiFiManager wifiManager(&server, &dns);
  // wifiManager.resetSettings(); // Uncomment this to reset the settings on

  wifiManager.setDebugOutput(false);
  wifiManager.autoConnect("AutoConnectAP");

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  server.begin();
}

void loop() {}
