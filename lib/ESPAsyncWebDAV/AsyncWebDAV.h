#include <Arduino.h>
#include <ESPAsyncWebServer.h>

enum DavResourceType { DAV_RESOURCE_NONE, DAV_RESOURCE_FILE, DAV_RESOURCE_DIR };
enum DavDepthType { DAV_DEPTH_NONE, DAV_DEPTH_CHILD, DAV_DEPTH_ALL };

class AsyncWebDAV : public AsyncWebHandler {
  using FS = fs::FS;

protected:
  FS _fs;
  String _url;

public:
  AsyncWebDAV(const String &url, FS &fs);

  virtual bool canHandle(AsyncWebServerRequest *request) override final;
  virtual void handleRequest(AsyncWebServerRequest *request) override final;
  virtual void handleBody(AsyncWebServerRequest *request, unsigned char *data,
                          size_t len, size_t index,
                          size_t total) override final;
  virtual bool isRequestHandlerTrivial() override final { return false; }
  const char *url() const { return _url.c_str(); }

private:
  void handlePropfind(const String &path, DavResourceType resource,
                      AsyncWebServerRequest *request);
  void handleGet(const String &path, DavResourceType resource,
                 AsyncWebServerRequest *request);
  void handlePut(const String &path, DavResourceType resource,
                 AsyncWebServerRequest *request, unsigned char *data,
                 size_t len, size_t index, size_t total);
  void handleLock(const String &path, DavResourceType resource,
                  AsyncWebServerRequest *request);
  void handleUnlock(const String &path, DavResourceType resource,
                    AsyncWebServerRequest *request);
  void handleMkcol(const String &path, DavResourceType resource,
                   AsyncWebServerRequest *request);
  void handleMove(const String &path, DavResourceType resource,
                  AsyncWebServerRequest *request);
  void handleDelete(const String &path, DavResourceType resource,
                    AsyncWebServerRequest *request);
  void handleHead(DavResourceType resource, AsyncWebServerRequest *request);
  void handleNotFound(AsyncWebServerRequest *request);
  void sendPropResponse(AsyncResponseStream *response, boolean recursing,
                        File *curFile);
  String urlToUri(String url);
};
