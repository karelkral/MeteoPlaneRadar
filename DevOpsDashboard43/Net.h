// =============================================================================
//  DevOpsDashboard43 (from DevOpsDashboard, based on MeteoPlaneRadar by chiptron.cz)
//  One place for "fetch this URL and hand me the body".
//
//  Every request needs the same things: a check that there is enough internal
//  RAM for a TLS handshake (https:// only), a TLS client that verifies the
//  server against the CA bundle built into the core, the response as a complete
//  string rather than a stream, and the "Date" header fed to the clock on the
//  way past.
// =============================================================================
#pragma once
#include <Arduino.h>

// Values of the `httpCode` output of Net_GetString():
//   200, 401, 404, ...  the HTTP status the server answered with
//   NET_CODE_NONE       nothing was sent: no WiFi link or not enough heap
//   -1 ... -11          HTTPClient transport errors (connection, TLS, timeout)
#define NET_CODE_NONE      0
#define NET_CODE_NOT_JSON  (-100)   // 200, but the Content-Type is not JSON
#define NET_CODE_BAD_BODY  (-101)   // 200, but the body is empty or too big

// GET the URL and put the whole body in `out`.
//   tag       - short name for the serial log ("PREDPOVED", "RAINVIEWER", ...)
//   hdrName / hdrValue - one extra request header, e.g. the API key. The value
//               is never logged.
//   requireJson - a 200 whose Content-Type is not application/json fails with
//               NET_CODE_NOT_JSON (a web page where an API was expected)
//   httpCode  - receives one of the codes above
// Returns false on no link, not enough heap, a non-200 status or an empty body;
// the reason is printed once, so a caller can just retry later.
// Not thread-safe: the body buffer is shared, so call it from one task only.
bool Net_GetString(const char* url, String& out, const char* tag,
                   const char* hdrName = nullptr, const char* hdrValue = nullptr,
                   bool requireJson = false, int* httpCode = nullptr);

// GET the URL into a caller-supplied buffer - for binary payloads (PNG tiles)
// where a String would double the memory needed.
// On success *outLen holds the number of bytes written.
bool Net_GetBinary(const char* url, uint8_t* buf, size_t cap, size_t* outLen,
                   const char* tag);

// --- Keep-alive session -----------------------------------------------------
//
// A TLS handshake needs ~45 kB of internal RAM, which the device does not
// reliably have while the web server is running - so a burst of one handshake
// per radar tile ended up with tiles refused by the heap guard and drawn black.
// Between Begin and End, Net_GetBinary reuses a single connection. Always pair
// them: an open session holds its buffers until closed.
void Net_SessionBegin();
void Net_SessionEnd();

// Called from loop() while a long transfer runs, so the watchdog stays fed.
void Net_SetPollFn(void (*fn)());
