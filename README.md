# OFM-Network

This module provides the network functionality for the OpenKNX stack.

## Platforms

* ARDUINO_ARCH_ESP32
* ARDUINO_ARCH_RP2040

## Defines

| Arch   | Type | Stack      | Define      | Note                                                          |
| ------ | ---- | ---------- | ----------- | ------------------------------------------------------------- |
| ESP32  | WiFi | Integrated | KNX_IP_WIFI |                                                               |
| ESP32  | LAN  | Integrated | KNX_IP_LAN  | You need to set right board and presumably CONFIG_ETH_ENABLED |
| RP2040 | WiFi | Integrated | KNX_IP_WIFI |                                                               |
| RP2040 | LAN  | Integrated | KNX_IP_LAN  |                                                               |


| Define                   | Default | Description                          | Note                                         |
|--------------------------|---------|--------------------------------------|----------------------------------------------|
| OPENKNX_LED_IP           |         | LED used for IP state                | Set to info2Led to use IP LED feature        |
| OPENKNX_PING_TIMEOUT     | 1000    | Default ping timeout in milliseconds | Can be overridden per call                   |
| OPENKNX_PING_PARALLEL    | 5       | Max concurrent pings                 | Additional requests are queued automatically |

## IP LED

If OPENKNX_LED_IP is defined, the LED is representing the state of the network. 
Possible values for OPENKNX_LED_IP: info1Led, info2Led, info3Led. Recommended value: info2Led

| State                         | LED           | RGB-LED            | Note                                   |
|-------------------------------|---------------|--------------------|----------------------------------------|
| WLAN configuration missing    | Fast flashing | Red fast flashing  | Only for HW with WLA                   |
| No connection to the network  | Off           | Red                |                                        |
| No IP adress                  | Slow flashing | Yellow             |                                        |
| IP adress assigned            | On            | Green              |                                        |

## Ping

The module provides a non-blocking ping API with an internal queue and parallel slot management.

### API

```cpp
// Ping by IP address
openknxNetwork.ping(IPAddress(192, 168, 1, 1), [](IPAddress ip, bool reachable, uint32_t rttMs) {
    if (reachable) logInfo("Ping", "%s: %lu ms", ip.toString().c_str(), rttMs);
});

// Ping by hostname (DNS resolved automatically)
openknxNetwork.ping("router.local", [](IPAddress ip, bool reachable, uint32_t rttMs) {
    if (reachable) logInfo("Ping", "%s: %lu ms", ip.toString().c_str(), rttMs);
});

// With explicit timeout (ms)
openknxNetwork.ping(target, callback, 500);
```

### Console command

```
ping <ip|hostname>
```

Example: `ping 8.8.8.8` or `ping openknx.de`

## OTA

Mit dem Netzwerkmodul wird eine OTA (Over the air) Update Funktion der Firmware ermöglicht.
Das OTA muss jedoch zuerst am Gerät erlaubt werden.
Dies kann durch drücken des PROG Tasters oder über die Konsole durch den Befehl `ota` erfolgen.

### ESP32 OTA

In platformio.custom.ini muss eine Section als OTA Target angelegt werden.
In dieser muss das upload_protocol OTA und die IP-Adresse oder der Hostname des Gerätes festgelegt werden.

```ini
upload_protocol = espota
upload_port = XXX.XXX.XXX.XXX # IP Address or Hostname
```

Hinweis: Der Hostname des Gerätes kann in der ETS im Abschnitt Netzwerk unter mDNS festgelegt werden.

### RP2040 OTA

Aktuelle ist der OTA Upload beim RP2040 noch nicht getestet.
