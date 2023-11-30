#include <AsyncWebServer_ESP32_ENC.h>
#include <ESP32Servo.h>
#include <Ticker.h>
#include <WiFi.h>
#include <asyncHTTPrequest.h>
#include <esp_camera.h>

//! ESP32 Wrover Module
#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 21
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27

#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 19
#define Y4_GPIO_NUM 18
#define Y3_GPIO_NUM 5
#define Y2_GPIO_NUM 4
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

#define SENSOR_PIN 33
#define SERVO1_PIN 12
#define LED2_PIN 2
#define LED3_PIN 32
#define LED4_PIN 14
#define LED5_PIN 13
#define LED6_PIN 15

#define CAMERA_MODEL_WROVER_KIT // Has PSRAM

const char *ssid = "ssid_name";
const char *passwd = "password";
const char *iaUrl = "http://URL-API-IA/check-capture";

#include "webpage.h"

AsyncWebServer serverWeb(80);
AsyncWebSocket serverWs("/ws");
AsyncWebSocketClient *streamClient;

asyncHTTPrequest clientWeb;

Ticker onceTickerApi1;
Ticker onceTickerApi2;
Ticker onceTickerApi3;
Ticker onceTickerApi4;
Ticker onceTickerApi5;
Ticker onceTickerApi6;
Ticker onceTickerSensor;
Servo myservo1;

bool isCaptureToTake = false;

void callbackSendCaptureToTake()
{
    isCaptureToTake = false;
}

void callbackResetServo()
{
    Serial.printf("Reset SERVO\n");
    myservo1.write(0);
}

void callbackResetLED(int LED_PIN)
{
    Serial.printf("Reset LED %d\n", LED_PIN);
    digitalWrite(LED_PIN, LOW);
}

void clientHandleCaptureToTake(void *optParm, asyncHTTPrequest *request, int readyState)
{
    if (readyState == 4) {
        Serial.printf("%s\n", request->responseText());
    }
}

void sendCaptureToTake()
{
    Serial.printf("Send capture to take...\n");
    if (clientWeb.readyState() == 0 || clientWeb.readyState() == 4) {
        clientWeb.open("GET", iaUrl);
        clientWeb.send();
    }
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    switch (type) {
    case WS_EVT_CONNECT:
        Serial.printf("stream[Server: %s][ClientID: %u] WSClient connected\n", server->url(), client->id());
        streamClient = client;
        break;
    case WS_EVT_DISCONNECT:
        Serial.printf("stream[Server: %s][ClientID: %u] WSClient disconnected\n", server->url(), client->id());
        streamClient = NULL;
        break;
    case WS_EVT_ERROR:
        Serial.printf("stream[Server: %s][ClientID: %u] error(%u): %s\n", server->url(), client->id(), *((uint16_t *)arg), (char *)data);
        break;
    case WS_EVT_PONG:
        Serial.printf("stream[Server: %s][ClientID: %u] pong[%u]: %s\n", server->url(), client->id(), len, (len) ? (char *)data : "");
        break;
    case WS_EVT_DATA:
        AwsFrameInfo *info = (AwsFrameInfo *)arg;

        if (info->final && info->index == 0 && info->len == len) {
            Serial.printf("stream[Server: %s][ClientID: %u] %s-message[len: %llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT) ? "text" : "binary", info->len);

            if (info->opcode == WS_TEXT) {
                data[len] = 0;
                Serial.printf("%s\n", (char *)data);
            } else {
                for (size_t i = 0; i < info->len; i++) {
                    Serial.printf("%02x ", data[i]);
                }

                Serial.printf("\n");
            }
        } else {
            if (info->index == 0) {
                if (info->num == 0)
                    Serial.printf("stream[Server: %s][ClientID: %u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
                Serial.printf("stream[Server: %s][ClientID: %u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
            }

            Serial.printf("stream[Server: %s][ClientID: %u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT) ? "text" : "binary", info->index, info->index + len);

            if (info->message_opcode == WS_TEXT) {
                data[len] = 0;
                Serial.printf("%s\n", (char *)data);
            } else {
                for (size_t i = 0; i < len; i++)
                    Serial.printf("%02x ", data[i]);
                Serial.printf("\n");
            }

            if ((info->index + len) == info->len)
                Serial.printf("stream[Server: %s][ClientID: %u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        }
        break;
    }
}

void apiHandleNotFound(AsyncWebServerRequest *request)
{
    Serial.printf("GET %s - NOT FOUND\n", request->url().c_str());

    String msg = "File Not Found\n\n";

    msg += "URI: ";
    msg += request->url();
    msg += "\nMethod: ";
    msg += (request->method() == HTTP_GET) ? "GET" : "POST";
    msg += "\nArguments: ";
    msg += request->args();
    msg += "\n";

    for (uint8_t i = 0; i < request->args(); i++) {
        msg += " " + request->argName(i) + ": " + request->arg(i) + "\n";
    }

    request->send(404, "text/plain", msg);
}

void apiHandleStream(AsyncWebServerRequest *request)
{
    Serial.printf("GET /stream\n");

    request->send(200, "text/html", webpageCont);
}

void apiHandleCapture(AsyncWebServerRequest *request)
{
    Serial.printf("GET /capture\n");

    camera_fb_t *fb = esp_camera_fb_get();

    if (!fb) {
        Serial.printf("Camera capture failed\n");
        request->send(500, "text/plain", "Camera capture failed");
        return;
    }

    Serial.printf("Format: %d, Width: %d, Height: %d\n", fb->format, fb->width, fb->height);
    Serial.printf("Timestamp: %d, Size: %d\n", fb->timestamp, fb->len);

    request->send_P(200, "image/jpeg", fb->buf, fb->len);

    esp_camera_fb_return(fb);

    Serial.printf("Capture OK\n");
}

void apiHandleRoot(AsyncWebServerRequest *request)
{
    Serial.printf("GET /\n");

    request->send(200, "text/html", "<h1>ESP32-CAM</h1>");
}

void apiHandleCameraFailed(AsyncWebServerRequest *request)
{
    Serial.printf("GET /camera_failed\n");

    request->send(500, "text/plain", "Camera capture failed");
}

void apiHandleBiodegradable(AsyncWebServerRequest *request)
{
    Serial.printf("GET /biodegradable\n");

    digitalWrite(LED2_PIN, HIGH);
    onceTickerApi1.once_ms(5000, callbackResetLED, LED2_PIN);
    request->send(200, "text/html", "<h1>Biodegradable</h1>");
}

void apiHandleCardboard(AsyncWebServerRequest *request)
{
    Serial.printf("GET /cardboard\n");

    myservo1.write(180);
    onceTickerApi2.once_ms(5000, callbackResetServo);
    request->send(200, "text/html", "<h1>Cardboard</h1>");
}

void apiHandleGlass(AsyncWebServerRequest *request)
{
    Serial.printf("GET /glass\n");

    digitalWrite(LED3_PIN, HIGH);
    onceTickerApi3.once_ms(5000, callbackResetLED, LED3_PIN);
    request->send(200, "text/html", "<h1>Glass</h1>");
}

void apiHandleMetal(AsyncWebServerRequest *request)
{
    Serial.printf("GET /metal\n");

    digitalWrite(LED4_PIN, HIGH);
    onceTickerApi4.once_ms(5000, callbackResetLED, LED4_PIN);
    request->send(200, "text/html", "<h1>Metal</h1>");
}

void apiHandlePaper(AsyncWebServerRequest *request)
{
    Serial.printf("GET /paper\n");

    digitalWrite(LED5_PIN, HIGH);
    onceTickerApi5.once_ms(5000, callbackResetLED, LED5_PIN);
    request->send(200, "text/html", "<h1>Paper</h1>");
}

void apiHandlePlastic(AsyncWebServerRequest *request)
{
    Serial.printf("GET /plastic\n");
    digitalWrite(LED6_PIN, HIGH);
    onceTickerApi6.once_ms(5000, callbackResetLED, LED6_PIN);
    request->send(200, "text/html", "<h1>Plastic</h1>");
}

void setupWebRoads(bool isCameraFail)
{
    Serial.printf("Setting up serverWeb Roads\n");

    if (isCameraFail) {
        serverWeb.on("/", HTTP_GET, [](AsyncWebServerRequest *request) { apiHandleCameraFailed(request); });
        serverWeb.on("/capture", HTTP_GET, [](AsyncWebServerRequest *request) { apiHandleCameraFailed(request); });
        serverWeb.on("/stream", HTTP_GET, [](AsyncWebServerRequest *request) { apiHandleCameraFailed(request); });
        serverWeb.on("/biodegradable", HTTP_GET, [](AsyncWebServerRequest *request) { apiHandleCameraFailed(request); });
        serverWeb.on("/cardboard", HTTP_GET, [](AsyncWebServerRequest *request) { apiHandleCameraFailed(request); });
        serverWeb.on("/glass", HTTP_GET, [](AsyncWebServerRequest *request) { apiHandleCameraFailed(request); });
        serverWeb.on("/metal", HTTP_GET, [](AsyncWebServerRequest *request) { apiHandleCameraFailed(request); });
        serverWeb.on("/paper", HTTP_GET, [](AsyncWebServerRequest *request) { apiHandleCameraFailed(request); });
        serverWeb.on("/plastic", HTTP_GET, [](AsyncWebServerRequest *request) { apiHandleCameraFailed(request); });
        serverWeb.onNotFound(apiHandleNotFound);
        return;
    }

    serverWeb.on("/", HTTP_GET, [](AsyncWebServerRequest *request) { apiHandleRoot(request); });
    serverWeb.on("/capture", HTTP_GET, [](AsyncWebServerRequest *request) { apiHandleCapture(request); });
    serverWeb.on("/stream", HTTP_GET, [](AsyncWebServerRequest *request) { apiHandleStream(request); });
    serverWeb.on("/biodegradable", HTTP_GET, [](AsyncWebServerRequest *request) { apiHandleBiodegradable(request); });
    serverWeb.on("/cardboard", HTTP_GET, [](AsyncWebServerRequest *request) { apiHandleCardboard(request); });
    serverWeb.on("/glass", HTTP_GET, [](AsyncWebServerRequest *request) { apiHandleGlass(request); });
    serverWeb.on("/metal", HTTP_GET, [](AsyncWebServerRequest *request) { apiHandleMetal(request); });
    serverWeb.on("/paper", HTTP_GET, [](AsyncWebServerRequest *request) { apiHandlePaper(request); });
    serverWeb.on("/plastic", HTTP_GET, [](AsyncWebServerRequest *request) { apiHandlePlastic(request); });
    serverWeb.onNotFound(apiHandleNotFound);

    serverWs.onEvent(onWsEvent);

    Serial.printf("Init serverWeb Roads OK\n");
}

void setupConfigCamera()
{
    Serial.printf("Setting up camera\n");

    camera_config_t cameraConfig;

    // camera config
    cameraConfig.ledc_channel = LEDC_CHANNEL_0;
    cameraConfig.ledc_timer = LEDC_TIMER_0;

    cameraConfig.pin_d0 = Y2_GPIO_NUM;
    cameraConfig.pin_d1 = Y3_GPIO_NUM;
    cameraConfig.pin_d2 = Y4_GPIO_NUM;
    cameraConfig.pin_d3 = Y5_GPIO_NUM;
    cameraConfig.pin_d4 = Y6_GPIO_NUM;
    cameraConfig.pin_d5 = Y7_GPIO_NUM;
    cameraConfig.pin_d6 = Y8_GPIO_NUM;
    cameraConfig.pin_d7 = Y9_GPIO_NUM;
    cameraConfig.pin_xclk = XCLK_GPIO_NUM;
    cameraConfig.pin_pclk = PCLK_GPIO_NUM;
    cameraConfig.pin_vsync = VSYNC_GPIO_NUM;
    cameraConfig.pin_href = HREF_GPIO_NUM;
    cameraConfig.pin_sccb_sda = SIOD_GPIO_NUM;
    cameraConfig.pin_sccb_scl = SIOC_GPIO_NUM;
    cameraConfig.pin_pwdn = PWDN_GPIO_NUM;
    cameraConfig.pin_reset = RESET_GPIO_NUM;

    cameraConfig.xclk_freq_hz = 20000000;
    cameraConfig.frame_size = FRAMESIZE_240X240;
    cameraConfig.pixel_format = PIXFORMAT_JPEG;
    cameraConfig.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    cameraConfig.fb_location = CAMERA_FB_IN_PSRAM;
    cameraConfig.jpeg_quality = 12;
    cameraConfig.fb_count = 1;

    // camera init
    esp_err_t err = esp_camera_init(&cameraConfig);

    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x\n", err);
        return;
    }

    // initial sensors are flipped vertically and colors are a bit saturated
    sensor_t *sensor = esp_camera_sensor_get();

    if (sensor->id.PID == OV3660_PID) {
        sensor->set_vflip(sensor, 1);       // flip it back
        sensor->set_brightness(sensor, 1);  // up the brightness just a bit
        sensor->set_saturation(sensor, -2); // lower the saturation
    }

    Serial.printf("Camera init ok\n");
}

void setupWifiConnection()
{
    Serial.printf("Connecting to %s", ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, passwd);

    for (int cpt = 0; cpt < 10 && WiFi.status() != WL_CONNECTED; cpt++) {
        delay(1000);
        Serial.printf(".");
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.printf("\nConnection failed\n");
        return;
    }

    Serial.printf("\nConnected, IP address: %s\n", WiFi.localIP().toString().c_str());
}

bool checkCameraCapture()
{
    Serial.printf("Check Camera Buffer\n");

    camera_fb_t *fb = esp_camera_fb_get();

    if (!fb) {
        Serial.printf("Camera capture failed\n");
        return true;
    }

    Serial.printf("Format: %d, Width: %d, Height: %d\n", fb->format, fb->width, fb->height);
    Serial.printf("Timestamp: %d, Size: %d\n", fb->timestamp, fb->len);

    esp_camera_fb_return(fb);

    Serial.printf("Capture OK\n");
    return false;
}

void setup()
{
    delay(5000);

    Serial.begin(115200);
    Serial.setDebugOutput(true);

    Serial.println();
    Serial.printf("Connected\n");

    setupConfigCamera();
    setupWifiConnection();
    if (checkCameraCapture())
        setupWebRoads(true);
    else
        setupWebRoads(false);

    serverWeb.addHandler(&serverWs);
    serverWeb.begin();
    Serial.printf("HTTP serverWeb started\n");

    clientWeb.onReadyStateChange(clientHandleCaptureToTake);
    Serial.printf("HTTP clientWeb started\n");

    pinMode(SENSOR_PIN, INPUT);
    pinMode(LED2_PIN, OUTPUT);
    pinMode(LED3_PIN, OUTPUT);
    pinMode(LED4_PIN, OUTPUT);
    pinMode(LED5_PIN, OUTPUT);
    pinMode(LED6_PIN, OUTPUT);
    myservo1.attach(SERVO1_PIN);
}

void loop()
{
    if (streamClient) {
        if (streamClient->canSend()) {
            camera_fb_t *fb = esp_camera_fb_get();

            if (!fb) {
                Serial.printf("Camera capture failed\n");
                streamClient = NULL;
                return;
            }

            streamClient->binary(fb->buf, fb->len);

            esp_camera_fb_return(fb);
        }
    }
    if (digitalRead(SENSOR_PIN) == LOW && !isCaptureToTake) {
        sendCaptureToTake();
        isCaptureToTake = true;
        onceTickerSensor.once_ms(7000, callbackSendCaptureToTake);
    }
}