#include "AsyncWebDAV.h"
#include <Arduino.h>
#include <DateTime.h>
#include <ESPAsyncWebServer.h>
#include <Hash.h>

AsyncWebDAV::AsyncWebDAV(const String &url, FS &fs) : _fs(fs), _url(url) {
  // Ensure leading '/'
  if (_url.length() == 0 || _url[0] != '/')
    _url = "/" + _url;
}

bool AsyncWebDAV::canHandle(AsyncWebServerRequest *request) {
  if (request->url().startsWith(_url)) {
    if (request->method() == HTTP_PROPFIND ||
        request->method() == HTTP_PROPPATCH) {
      request->addInterestingHeader("depth");
      return true;
    }
    if (request->method() == HTTP_MOVE) {
      request->addInterestingHeader("destination");
      return true;
    }
    if (request->method() == HTTP_GET || request->method() == HTTP_HEAD ||
        request->method() == HTTP_OPTIONS || request->method() == HTTP_PUT ||
        request->method() == HTTP_LOCK || request->method() == HTTP_UNLOCK ||
        request->method() == HTTP_MKCOL || request->method() == HTTP_DELETE ||
        request->method() == HTTP_COPY) {
      return true;
    }
  }
  return false;
}

void AsyncWebDAV::handleRequest(AsyncWebServerRequest *request) {
  // parse the url to a proper path
  String path = request->url().substring(_url.length());
  if (path.isEmpty()) {
    path = "/";
  }
  if (!path.equals("/") && path.endsWith("/")) {
    path = path.substring(0, path.length() - 1);
  }

  // check resource type on local storage
  DavResourceType resource = DAV_RESOURCE_NONE;
  File baseFile = _fs.open(path, FILE_READ);

  if (baseFile) {
    resource = baseFile.isDirectory() ? DAV_RESOURCE_DIR : DAV_RESOURCE_FILE;
    baseFile.close();
  }

  // route the request
  if (request->method() == HTTP_PROPFIND ||
      request->method() == HTTP_PROPPATCH) {
    return handlePropfind(path, resource, request);
  }
  if (request->method() == HTTP_GET) {
    return handleGet(path, resource, request);
  }
  if (request->method() == HTTP_HEAD || request->method() == HTTP_OPTIONS) {
    return handleHead(resource, request);
  }
  if (request->method() == HTTP_PUT) {
    if (_fs.exists(path)) {
      return request->send(200);
    } else {
      File f = _fs.open(path, "a");
      if (f) {
        f.close();
        return request->send(201);
      } else {
        return request->send(500);
      }
    }
  }
  if (request->method() == HTTP_LOCK) {
    return handleLock(path, resource, request);
  }
  if (request->method() == HTTP_UNLOCK) {
    return handleUnlock(path, resource, request);
  }
  if (request->method() == HTTP_MKCOL) {
    return handleMkcol(path, resource, request);
  }
  if (request->method() == HTTP_MOVE) {
    return handleMove(path, resource, request);
  }
  if (request->method() == HTTP_DELETE) {
    return handleDelete(path, resource, request);
  }

  return handleNotFound(request);
}

void AsyncWebDAV::handleBody(AsyncWebServerRequest *request,
                             unsigned char *data, size_t len, size_t index,
                             size_t total) {
  // parse the url to a proper path
  String path = request->url().substring(_url.length());
  if (path.isEmpty()) {
    path = "/";
  }
  if (!path.equals("/") && path.endsWith("/")) {
    path = path.substring(0, path.length() - 1);
  }

  // check resource type on local storage
  DavResourceType resource = DAV_RESOURCE_NONE;
  File baseFile = _fs.open(path, FILE_READ);
  if (baseFile) {
    resource = baseFile.isDirectory() ? DAV_RESOURCE_DIR : DAV_RESOURCE_FILE;
    baseFile.close();
  }

  // route the request
  if (request->method() == HTTP_PUT) {
    return handlePut(path, resource, request, data, len, index, total);
  }
}

void AsyncWebDAV::handlePropfind(const String &path, DavResourceType resource,
                                 AsyncWebServerRequest *request) {
  // check whether file or dir exists
  if (resource == DAV_RESOURCE_NONE) {
    return handleNotFound(request);
  }

  // check depth header
  DavDepthType depth = DAV_DEPTH_NONE;
  AsyncWebHeader *depthHeader = request->getHeader("Depth");
  if (depthHeader) {
    if (depthHeader->value().equals("1")) {
      depth = DAV_DEPTH_CHILD;
    } else if (depthHeader->value().equals("infinity")) {
      depth = DAV_DEPTH_ALL;
    }
  }

  // prepare response
  File baseFile = _fs.open(path, FILE_READ);
  AsyncResponseStream *response =
      request->beginResponseStream("application/xml");
  response->setCode(207);

  response->print("<?xml version=\"1.0\"?>");
  response->print("<d:multistatus xmlns:d=\"DAV:\">");
  sendPropResponse(response, false, &baseFile);
  if (resource == DAV_RESOURCE_DIR && depth == DAV_DEPTH_CHILD) {
    File root = _fs.open(path);
    File childFile = root.openNextFile();
    while (childFile) {
      sendPropResponse(response, true, &childFile);
      childFile.close();
      childFile = root.openNextFile();
    }
  }
  response->print("</d:multistatus>");

  baseFile.close();
  return request->send(response);
}

void AsyncWebDAV::handleGet(const String &path, DavResourceType resource,
                            AsyncWebServerRequest *request) {
  if (resource != DAV_RESOURCE_FILE) {
    return handleNotFound(request);
  }

  AsyncWebServerResponse *response = request->beginResponse(_fs, path);
  response->addHeader("Allow",
                      "PROPFIND,OPTIONS,DELETE,COPY,MOVE,HEAD,POST,PUT,GET");
  request->send(response);
}

void AsyncWebDAV::handlePut(const String &path, DavResourceType resource,
                            AsyncWebServerRequest *request, unsigned char *data,
                            size_t len, size_t index, size_t total) {
  if (resource == DAV_RESOURCE_DIR) {
    return handleNotFound(request);
  }

  File file;
  if (!index) {
    file = _fs.open(path, FILE_WRITE);
  } else {
    file = _fs.open(path, FILE_APPEND);
  }
  file.write(data, len);
  file.close();
}

void AsyncWebDAV::handleLock(const String &path, DavResourceType resource,
                             AsyncWebServerRequest *request) {
  if (resource == DAV_RESOURCE_NONE) {
    return handleNotFound(request);
  }

  AsyncResponseStream *response =
      request->beginResponseStream("application/xml; charset=utf-8");
  response->setCode(200);
  response->addHeader("Allow", "PROPPATCH,PROPFIND,OPTIONS,DELETE,UNLOCK,COPY,"
                               "LOCK,MOVE,HEAD,POST,PUT,GET");
  response->addHeader("Lock-Token",
                      "urn:uuid:26e57cb3-834d-191a-00de-000042bdecf9");

  response->print("<?xml version=\"1.0\" encoding=\"utf-8\"?>");
  response->print("<D:prop xmlns:D=\"DAV:\">");
  response->print("<D:lockdiscovery>");
  response->print("<D:activelock>");
  response->print("<D:locktype><write/></D:locktype>");
  response->print("<D:lockscope><exclusive/></D:lockscope>");
  response->print("<D:locktoken><D:href>urn:uuid:26e57cb3-834d-191a-00de-"
                  "000042bdecf9</D:href></D:locktoken>");
  response->printf("<D:lockroot><D:href>%s</D:href></D:lockroot>",
                   (String("http://") + request->host() + _url + path).c_str());
  response->print("<D:depth>infinity</D:depth>");
  response->printf("<D:owner><a:href xmlns:a=\"DAV:\">%s</a:href></D:owner>",
                   "todo");
  response->print("<D:timeout>Second-3600</D:timeout>");
  response->print("</D:activelock>");
  response->print("</D:lockdiscovery>");
  response->print("</D:prop>");

  request->send(response);
}

void AsyncWebDAV::handleUnlock(const String &path, DavResourceType resource,
                               AsyncWebServerRequest *request) {
  if (resource == DAV_RESOURCE_NONE) {
    return handleNotFound(request);
  }

  AsyncWebServerResponse *response = request->beginResponse(200);
  response->addHeader("Allow", "PROPPATCH,PROPFIND,OPTIONS,DELETE,UNLOCK,COPY,"
                               "LOCK,MOVE,HEAD,POST,PUT,GET");
  response->addHeader("Lock-Token",
                      "urn:uuid:26e57cb3-834d-191a-00de-000042bdecf9");
  request->send(response);
}

void AsyncWebDAV::handleMkcol(const String &path, DavResourceType resource,
                              AsyncWebServerRequest *request) {
  // does the file/dir already exist?
  int status;
  if (resource != DAV_RESOURCE_NONE) {
    status = 405;
  } else {
    // create dir and send response
    if (_fs.mkdir(path)) {
      status = 201;
    } else {
      status = 405;
    }
  }
  request->send(status);
}

void AsyncWebDAV::handleMove(const String &path, DavResourceType resource,
                             AsyncWebServerRequest *request) {
  if (resource == DAV_RESOURCE_NONE) {
    return handleNotFound(request);
  }

  AsyncWebHeader *destinationHeader = request->getHeader("destination");
  if (!destinationHeader || destinationHeader->value().isEmpty()) {
    return handleNotFound(request);
  }

  AsyncWebServerResponse *response;
  if (_fs.rename(path, urlToUri(destinationHeader->value()))) {
    response = request->beginResponse(201);
    response->addHeader("Allow", "OPTIONS,MKCOL,LOCK,POST,PUT");
  } else {
    response = request->beginResponse(500, "text/plain", "Unable to move");
  }

  request->send(response);
}

void AsyncWebDAV::handleDelete(const String &path, DavResourceType resource,
                               AsyncWebServerRequest *request) {
  // does the file/dir exist?
  if (resource == DAV_RESOURCE_NONE) {
    return handleNotFound(request);
  }

  // delete file or dir
  bool result;
  if (resource == DAV_RESOURCE_FILE) {
    result = _fs.remove(path);
  } else {
    result = _fs.rmdir(path);
  }

  // check for error
  AsyncWebServerResponse *response;
  if (result) {
    response = request->beginResponse(200);
    response->addHeader("Allow", "OPTIONS,MKCOL,LOCK,POST,PUT");
  } else {
    response = request->beginResponse(201);
  }

  request->send(response);
}

void AsyncWebDAV::handleHead(DavResourceType resource,
                             AsyncWebServerRequest *request) {
  if (resource == DAV_RESOURCE_NONE) {
    return handleNotFound(request);
  }

  AsyncWebServerResponse *response = request->beginResponse(200);
  response->addHeader("DAV", "1,2");
  response->addHeader("MS-Author-Via", "DAV");
  if (resource == DAV_RESOURCE_FILE) {
    response->addHeader("Allow",
                        "PROPFIND,OPTIONS,DELETE,COPY,MOVE,HEAD,POST,PUT,GET");
  }
  if (resource == DAV_RESOURCE_DIR) {
    response->addHeader("Allow", "PROPFIND,OPTIONS,DELETE,COPY,MOVE");
  }
  request->send(response);
}

void AsyncWebDAV::handleNotFound(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse(404);
  response->addHeader("Allow", "OPTIONS,MKCOL,POST,PUT");
  response->addHeader("DAV", "1,2");
  response->addHeader("MS-Author-Via", "DAV");
  request->send(response);
}

String AsyncWebDAV::urlToUri(String url) {
  if (url.startsWith("http://")) {
    int uriStart = url.indexOf('/', 7);
    return url.substring(uriStart + 4);
  } else if (url.startsWith("https://")) {
    int uriStart = url.indexOf('/', 8);
    return url.substring(uriStart + 4);
  } else {
    return url.substring(_url.length());
  }
}

void AsyncWebDAV::sendPropResponse(AsyncResponseStream *response,
                                   boolean recursing, File *curFile) {
  String fullPath = curFile->name();
  if (fullPath.substring(0, 1) != "/") {
    fullPath = String("/") + fullPath;
  }
  if (curFile->isDirectory() &&
      fullPath.substring(fullPath.length() - 1, fullPath.length()) != "/") {
    fullPath += "/";
  }
  fullPath = _url + fullPath;
  fullPath.replace(" ", "%20");

  // get file modified time
  time_t lastWrite = curFile->getLastWrite();
  DateTimeClass dt(lastWrite);
  String fileTimeStamp = dt.format("%a, %d %b %Y %H:%M:%S GMT");

  // send response
  response->print("<d:response>");
  response->printf("<d:href>%s</d:href>", fullPath.c_str());
  response->print("<d:propstat>");
  response->print("<d:prop>");

  // last modified
  response->printf("<d:getlastmodified>%s</d:getlastmodified>",
                   fileTimeStamp.c_str());

  // File root = _fs.open("/");

  // quota
  // response->printf("<d:quota-used-bytes>%d</d:quota-used-bytes>",
  // root.size());
  // response->printf("<d:quota-available-bytes>%d</d:quota-available-bytes>",
  // root.size() - root.size());

  if (curFile->isDirectory()) {
    // resource type
    response->print("<d:resourcetype><d:collection/></d:resourcetype>");
  } else {
    // etag
    response->printf("<d:getetag>%s</d:getetag>",
                     sha1(fullPath + fileTimeStamp).c_str());

    // resource type
    response->print("<d:resourcetype/>");

    // content length
    response->printf("<d:getcontentlength>%d</d:getcontentlength>",
                     curFile->size());

    // content type
    response->print("<d:getcontenttype>text/plain</d:getcontenttype>");
  }
  response->print("</d:prop>");
  response->print("<d:status>HTTP/1.1 200 OK</d:status>");
  response->print("</d:propstat>");

  response->print("</d:response>");
}
