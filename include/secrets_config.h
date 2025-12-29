#pragma once

#if __has_include("secrets.h")
#include "secrets.h"
#endif

#ifndef IDRYER_WIFI_SSID
#define IDRYER_WIFI_SSID "Ru"
// #define IDRYER_WIFI_SSID "YOUR_WIFI_SSID"
#endif

#ifndef IDRYER_WIFI_PASSWORD
#define IDRYER_WIFI_PASSWORD "Anastasia"
// #define IDRYER_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#endif


#ifndef IDRYER_PORTAL_HOST
#define IDRYER_PORTAL_HOST "portal.idryer.org"
#endif

#ifndef IDRYER_PORTAL_PORT
#define IDRYER_PORTAL_PORT 443
#endif

#ifndef IDRYER_PORTAL_USE_TLS
#define IDRYER_PORTAL_USE_TLS 1
#endif

#ifndef IDRYER_API_BASE
#define IDRYER_API_BASE "https://portal.idryer.org/api"
#endif

#ifndef IDRYER_SOCKET_PATH
#define IDRYER_SOCKET_PATH "/socket.io/?EIO=4"
#endif

#ifndef IDRYER_SOCKET_NAMESPACE
#define IDRYER_SOCKET_NAMESPACE "/"
#endif

#ifndef IDRYER_HTTP_TIMEOUT_MS
#define IDRYER_HTTP_TIMEOUT_MS 8500
#endif

#ifndef MQTT_BROKER
#define MQTT_BROKER "mqtt.idryer.org"
#endif

#ifndef MQTT_PORT
#define MQTT_PORT 8883
#endif

#ifndef MQTT_USE_TLS
#define MQTT_USE_TLS 1
#endif
