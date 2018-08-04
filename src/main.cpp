#define FASTLED_ESP8266_D1_PIN_ORDER
#define FASTLED_ALLOW_INTERRUPTS 0

#include <Arduino.h>
#include <FastLED.h>
#define NUM_LEDS 8
#define DATA_PIN 5

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESPAsyncWebServer.h>

#define BREATHE_TIMER 85
#define GLYPH_STARTUP_TIME 40

#define GAME_TICK_TIME 70

const char* wifi_ssid = "DexHunt2018";
const char* wifi_pw = "dexhunt2018prop";

AsyncWebServer server(80);

CRGB leds[NUM_LEDS];
CRGB currBreatheColorDisabled = CRGB::Black;
CRGB currBreatheColorEnabled = CRGB::Black;
int led_startup_timers[NUM_LEDS];

typedef enum LEDState {
    led_state_breatheoff,
    led_state_breatheon,
    led_state_breathestartup
} LEDState;
LEDState ledState[NUM_LEDS] = {led_state_breatheoff};
bool gameStatus[NUM_LEDS] = {false};

int breatheTimer = 0;
int breatheDelta = 1;

unsigned int gameTimerMs = 50000000; //13hrs
// unsigned int gameTimerMs = 90000;//1.5min

void setup_wifi();
void manage_wifi();
bool gameStillRunning();

void serialWriteValue(int value) {
    char str[256];
    sprintf(str, "%d", value);
    Serial.write(str);
}

void setup() {
  Serial.begin(9600); 
  setup_wifi();

  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  for (int led = 0; led < NUM_LEDS; led++) {
    led_startup_timers[led] = 0;
  }
}

void loop() {
    manage_wifi();

    EVERY_N_MILLISECONDS(GAME_TICK_TIME) {
        if (gameStillRunning()) {
            if (breatheDelta == 1 && breatheTimer < BREATHE_TIMER) {
                currBreatheColorDisabled = CRGB(0, 20, 255);
                currBreatheColorEnabled = CRGB(0, 255, 0);
            }
            if (breatheDelta == -1 && breatheTimer > 0) {
                currBreatheColorDisabled = CRGB(0, 50, 50);
                currBreatheColorEnabled = CRGB(50, 50, 0);
            }
    
            //Switch the timer direction if needed then increment
            if (breatheTimer >= BREATHE_TIMER) breatheDelta = -1;
            if (breatheTimer <= 0) breatheDelta = 1;
            breatheTimer += breatheDelta;
    
            for (int led = 0; led < NUM_LEDS; led++) {
                if (ledState[led] == LEDState::led_state_breatheoff) {
                    leds[led] = nblend(leds[led], currBreatheColorDisabled, 8);
                }
                else if (ledState[led] == LEDState::led_state_breatheon) {
                    leds[led] = nblend(leds[led], currBreatheColorEnabled, 8);
                }
                else {
                    int whiteoutAmount = map(led_startup_timers[led], GLYPH_STARTUP_TIME, 0, 0, 255);
                    leds[led] = nblend(leds[led], CRGB::White, whiteoutAmount);
        
                    led_startup_timers[led] -= 1;
                    if (led_startup_timers[led] <= 0) {
                        ledState[led] = LEDState::led_state_breatheon;
                    }
                }
            }
        } else {
            //Game Over
            if (breatheDelta == 1 && breatheTimer < BREATHE_TIMER) {
                currBreatheColorEnabled = CRGB(255, 0, 50);
            }
            if (breatheDelta == -1 && breatheTimer > 0) {
                currBreatheColorEnabled = CRGB(100, 0, 20);
            }
    
            //Switch the timer direction if needed then increment
            if (breatheTimer >= BREATHE_TIMER) breatheDelta = -1;
            if (breatheTimer <= 0) breatheDelta = 1;
            breatheTimer += breatheDelta;
    
            for (int led = 0; led < NUM_LEDS; led++) {
                leds[led] = nblend(leds[led], currBreatheColorEnabled, 8);
            }
        }
       
        FastLED.show();
    }
    
 
}

void handleRoot(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "oh hello there");
    Serial.write("Dex Hunt 2018 Root");
}

void endpoint_enableLed(AsyncWebServerRequest *request) {
    int idx = -1;
    int enabled = -1;
    int paramsCount = request->params();
    for (int i = 0; i < paramsCount; i++) {
        AsyncWebParameter* p = request->getParam(i);
        if (p->name() == "ledNum") {
            idx = p->value().toInt();
        }
        if (p->name() == "enabled") {
            enabled = p->value().toInt();
        }
    }

    if (idx == -1 || enabled == -1) {
        request->send(400, "text/plain", "Missing Params");
        return;
    }

    if (idx >= 0 && idx < NUM_LEDS) {
        if (enabled == 0) {
            led_startup_timers[idx] = 0;
            gameStatus[idx] = false;
            ledState[idx] = LEDState::led_state_breatheoff;
            Serial.write("Disable LED ");
            serialWriteValue(idx);
            Serial.write("\n");
        }
        else {
            led_startup_timers[idx] = GLYPH_STARTUP_TIME;
            gameStatus[idx] = true;
            ledState[idx] = LEDState::led_state_breathestartup;
            Serial.write("Enable LED ");
            serialWriteValue(idx);
            Serial.write("\n");
        }
        request->send(200, "text/plain", "OK");
    } else {
        request->send(400, "text/plain", "Invalid Params");
        return;
    }
}

void endpoint_setTimeRemaining(AsyncWebServerRequest *request) {
    int seconds = 0;
    int paramsCount = request->params();
    for (int i = 0; i < paramsCount; i++) {
        AsyncWebParameter* p = request->getParam(i);
        if (p->name() == "seconds") {
            seconds = p->value().toInt();
        }
    }

    gameTimerMs = seconds * 1000;

    Serial.write("Setting Time Remaining: ");
    serialWriteValue(gameTimerMs);
    Serial.write("\n");
    request->send(200, "text/plain", "OK");
}

void endpoint_getGameStatus(AsyncWebServerRequest *request) {
    int gameStatusCount = 0;
    int i;
    for (i = 0; i < NUM_LEDS; i++) {
        if (gameStatus[i]) gameStatusCount++;
    }
    char timestr[16];
    sprintf(timestr, "%d", gameStatusCount);
    request->send(200, "text/plain", timestr);
    Serial.write("Game Status: ");
    serialWriteValue(gameStatusCount);
    Serial.write("\n");
}

bool gameStillRunning() {
    if (gameTimerMs >= GAME_TICK_TIME) gameTimerMs -= GAME_TICK_TIME;
    else if (gameTimerMs != 0) {
        gameTimerMs = 0;
        Serial.write("Time is up!\n");
    }

    return (gameTimerMs != 0);
}

void setup_wifi() {
  WiFi.begin(wifi_ssid, wifi_pw);

  server.on("/", handleRoot);
  server.on("/setEnabled", endpoint_enableLed);
  server.on("/setTimeRemaining", endpoint_setTimeRemaining);
  server.on("/getGameStatus", endpoint_getGameStatus);

  Serial.write("Server is listening\n");
}

bool wifiIsConnected = false;
void manage_wifi() {
    int wifiStatus = WiFi.status();
    if(wifiStatus == WL_CONNECTED){
        if (!wifiIsConnected) {
            wifiIsConnected = true;
            server.begin();
            Serial.write("Connected. Your IP address is: ");
            Serial.write(WiFi.localIP());
            Serial.write("\n");
        }
    } else if (wifiStatus == WL_DISCONNECTED) {
        if (wifiIsConnected) {
            wifiIsConnected = false;
            //server.stop();
        }
    }
    // server.handleClient();
}