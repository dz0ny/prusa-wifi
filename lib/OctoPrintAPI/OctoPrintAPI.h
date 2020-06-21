#include <Arduino.h>
#include <ESPAsyncWebServer.h>

class OctoPrintAPI : public AsyncWebHandler {
  using FS = fs::FS;

protected:
  FS _fs;
  uint32_t _startTime;
  String _uploadedFullname;

public:
  OctoPrintAPI(FS &fs);

  virtual bool canHandle(AsyncWebServerRequest *request) override final;
  virtual void handleRequest(AsyncWebServerRequest *request) override final;
  virtual void handleUpload(AsyncWebServerRequest *request,
                            const String &filename, size_t index, uint8_t *data,
                            size_t len, bool final) override final;
  virtual bool isRequestHandlerTrivial() override final { return false; }

private:
  bool on(AsyncWebServerRequest *r, WebRequestMethodComposite method,
          const char *uri);
  void handleNotFound(AsyncWebServerRequest *request);
  void handleGetAPIVersion(AsyncWebServerRequest *request);
  void handlePOSTFilesLocal(AsyncWebServerRequest *request);
  void handleGETConnection(AsyncWebServerRequest *request);
};
