# Changes

## 0.7.0

* feature: Add an **HTTP webserver** (`OPENKNX_WEBSERVER`) with WebSocket console (`OPENKNX_WEBCONSOLE`) and multi-platform support (ESP32 `httpd`, RP2040 lwIP `NO_SYS=1`)
  * Asset management (`addStylesheet`/`addJavaScript`) with cache-busting, shared base stylesheet/navigation
  * Web console streams the logger ring buffer over WebSocket, with history replay for new clients
* feature: Add a **file manager** (`OPENKNX_WEBFS`) for LittleFS — directory listing, streaming download, chunked upload, delete, mkdir
* feature: Add an **MQTT client and broker** (`OPENKNX_MQTT`) — no external dependencies
  * Client mode connects to an external broker, local echo for subscribed callbacks
  * Broker mode: auth, retained messages (capped), QoS 0+1, keep-alive, per-client subscription limits
  * ESP32 runs broker/client in its own FreeRTOS task; RP2040 in the main loop
* feature: Add an **HTTP(S) webclient** (`OPENKNX_WEBCLIENT`) — parallel request slots, response object, certificate verification, ESP32 and RP2040
* feature: Add a **JSON Reader/Writer** for parsing and serialization, used by the webserver/MQTT features
* feature: Memory optimizations — PSRAM allocation for MQTT retained messages/buffers and webserver buffers where available
* feature: Add web **group monitor** (`OPENKNX_WEBMONITOR`) — ETS-style live KNX telegram view at `/groupmonitor`
  * TP only, gated on `MASK_VERSION == 0x07B0`; requires `OPENKNX_WEBSERVER`
  * Taps all bus frames in parallel via the TP-UART `registerReceivedFrame` hook, bypassing the KNX stack (no filtering)
  * Decodes source/destination, APCI type (Read/Write/Response) and payload hex, broadcasts as JSON over WS `/groupmonitor`
  * Read-only; no own buffer/loop — frame callback broadcasts directly; new clients see telegrams from connect-time on
* feature: Add `PingHandler` — non-blocking ICMP ping with queue and parallel slot management
  * Up to `OPENKNX_PING_PARALLEL` (default: 5) concurrent pings, unlimited queue
  * Configurable timeout via `OPENKNX_PING_TIMEOUT` (default: 1000 ms)
  * Callback signature: `void(IPAddress, bool reachable, uint32_t rttMs)`
  * RP2040: lwip raw API (`raw_sendto_if_src`), accurate RTT via callback timestamp
  * ESP32: lwip BSD socket API, thread-safe DNS via FreeRTOS queue
  * DNS resolution support (`ping("hostname", callback)`) on both platforms
  * Console command: `ping <ip|hostname>` with 2 s timeout
  * API via `openknxNetwork.ping(target, callback [, timeoutMs])`

## 0.6.0: 2026-05-15

* Initial Changelog started
* fix: ensure NTP server is stopped only if enabled and adjust sync mode
* fix: update WiFi settings handling to prevent disconnect on reboot
* fix: ensure NTP server is stopped only if enabled and adjust sync mode
* enhancment: use new function property wrapper
   