// HttpFetcher on top of HTTPClient + WiFiClientSecure.
//  * setInsecure(): no certificate validation (accepted trade-off: the data is
//    public, and pinning certificates would break when the APIs rotate them).
//  * useHTTP10(true): no chunked transfer encoding, so ArduinoJson can parse
//    the raw stream directly – responses are never buffered in RAM.
#pragma once

#include "byte_source.h"

class EspHttp : public core::HttpFetcher {
 protected:
  bool doGet(const char* url, Consumer& consume) override;
};
