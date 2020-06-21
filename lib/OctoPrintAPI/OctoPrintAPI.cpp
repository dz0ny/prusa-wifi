#include "OctoPrintAPI.h"
#include "ArduinoJson.h"
#include <Arduino.h>

#define VERSION "1.3.10"

OctoPrintAPI::OctoPrintAPI(FS &fs) : _fs(fs), _startTime(0) {}

bool OctoPrintAPI::canHandle(AsyncWebServerRequest *request) {

  return request->url().startsWith("/api");
}

bool OctoPrintAPI::on(AsyncWebServerRequest *r,
                      WebRequestMethodComposite method, const char *uri) {
  String path = r->url();
  Serial.printf("%s %s\n", r->methodToString(), path.c_str());
  return path.equals("/api/version") && r->method() == HTTP_GET;
}

void OctoPrintAPI::handleRequest(AsyncWebServerRequest *r) {

  if (on(r, HTTP_GET, "/api/version")) {
    return handleGetAPIVersion(r);
  }
  if (on(r, HTTP_POST, "/api/files/local")) {
    return handlePOSTFilesLocal(r);
  }
  if (on(r, HTTP_GET, "/api/connection")) {
    return handleGETConnection(r);
  }
  return handleNotFound(r);
}

void OctoPrintAPI::handleGetAPIVersion(AsyncWebServerRequest *request) {
  // http://docs.octoprint.org/en/master/api/version.html
  // HTTP/1.1 200 OK
  // Content-Type: application/json

  // {
  //   "api": "0.1",
  //   "server": "1.3.10",
  //   "text": "OctoPrint 1.3.10"
  // }

  AsyncResponseStream *response =
      request->beginResponseStream("application/json");

  DynamicJsonDocument doc(JSON_OBJECT_SIZE(3));
  doc["api"] = VERSION;
  doc["server"] = VERSION;
  doc["text"] = "OctoPrint " VERSION;
  serializeJson(doc, *response);
  request->send(response);
};

void OctoPrintAPI::handlePOSTFilesLocal(AsyncWebServerRequest *request) {
  // https://docs.octoprint.org/en/master/api/files.html#upload-file-or-create-folder
  // HTTP/1.1 200 OK
  // Content-Type: application/json
  // Location: http://example.com/api/files/sdcard/whistle_v2.gcode

  // {
  //   "files": {
  //     "local": {
  //       "name": "whistle_v2.gcode",
  //       "path": "whistle_v2.gcode",
  //       "type": "machinecode",
  //       "typePath": ["machinecode", "gcode"],
  //       "origin": "local",
  //       "refs": {
  //         "resource": "http://example.com/api/files/local/whistle_v2.gcode",
  //         "download":
  //         "http://example.com/downloads/files/local/whistle_v2.gcode"
  //       }
  //     },
  //   },
  //   "done": false
  // }
  AsyncResponseStream *response =
      request->beginResponseStream("application/json");
  response->setCode(201);
  DynamicJsonDocument local(JSON_OBJECT_SIZE(2));
  local["name"] = _uploadedFullname;
  local["origin"] = "local";
  DynamicJsonDocument files(JSON_OBJECT_SIZE(1));
  files["local"] = local;
  DynamicJsonDocument doc(JSON_OBJECT_SIZE(2));

  doc["files"] = files;
  doc["done"] = true;
  serializeJson(doc, *response);
  response->addHeader("Connection", "close");

  request->send(response);
};

void OctoPrintAPI::handleGETConnection(AsyncWebServerRequest *request) {
  // http://docs.octoprint.org/en/master/api/version.html

  // {
  //   "current": {
  //     "state": "Operational",
  //     "port": "/dev/ttyACM0",
  //     "baudrate": 250000,
  //     "printerProfile": "_default"
  //   },
  //   "options": {
  //     "ports": ["/dev/ttyACM0", "VIRTUAL"],
  //     "baudrates": [250000, 230400, 115200, 57600, 38400, 19200, 9600],
  //     "printerProfiles": [{"name": "Default", "id": "_default"}],
  //     "portPreference": "/dev/ttyACM0",
  //     "baudratePreference": 250000,
  //     "printerProfilePreference": "_default",
  //     "autoconnect": true
  //   }
  // }

  AsyncResponseStream *response =
      request->beginResponseStream("application/json");
  response->setCode(200);
  DynamicJsonDocument current(JSON_OBJECT_SIZE(2));
  current["state"] = "Operational";
  current["port"] = "Serial1";
  current["baudrate"] = 250000;
  current["printerProfile"] = "_default";

  DynamicJsonDocument options(JSON_OBJECT_SIZE(1));
  options["ports"] = "";
  options["baudrates"] = "";
  options["printerProfiles"] = "";
  options["portPreference"] = "";
  options["baudratePreference"] = "";
  options["printerProfilePreference"] = "_default";
  options["autoconnect"] = "";

  DynamicJsonDocument doc(JSON_OBJECT_SIZE(2));
  doc["current"] = current;
  doc["options"] = options;
  serializeJson(doc, *response);
  response->addHeader("Connection", "close");
  request->send(response);
};

void OctoPrintAPI::handleUpload(AsyncWebServerRequest *request,
                                const String &filename, size_t index,
                                uint8_t *data, size_t len, bool final) {
  // http://docs.octoprint.org/en/master/api/version.html
  int pos = filename.lastIndexOf("/");
  _uploadedFullname = pos == -1 ? "/" + filename : filename.substring(pos);

  if (!index) {
    request->_tempFile = _fs.open(_uploadedFullname, FILE_WRITE);
    _startTime = millis();
  }
  if (request->_tempFile) {
    if (len) {
      request->_tempFile.write(data, len);
    }
    if (final) {
      request->_tempFile.close();
    }
  }
};

void OctoPrintAPI::handleNotFound(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse(404);
  request->send(response);
}
