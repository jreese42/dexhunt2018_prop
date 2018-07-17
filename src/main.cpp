#define FASTLED_ESP8266_D1_PIN_ORDER

#include <Arduino.h>
#include <FastLED.h>
#define NUM_LEDS 8
#define DATA_PIN 5

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#define BREATHE_TIMER 85
#define GLYPH_STARTUP_TIME 40

#define GAME_TICK_TIME 70

// const char* wifi_ssid = "DexHunt2018";
// const char* wifi_pw = "dexhunt2018";
const char* wifi_ssid = "Halp!";
const char* wifi_pw = "stripher";
ESP8266WebServer server(80);

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

unsigned int gameTimerMs = 100000;

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
    delay(GAME_TICK_TIME);
 
}

void sendResponse(int code, const char* str) {
    server.sendHeader("Connection", "close");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(code, "text/plain", str);
}
void handleRoot() {
    sendResponse(200, "DEX Hunt 2018");
}

void handleNotFound(){
    sendResponse(404, "Not Found"); // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
  }

void endpoint_enableLed() {
    if (!server.hasArg("ledNum") || !server.hasArg("enabled")){ 
        sendResponse(400, "Missing Parameters");
        return;
    }
    int idx = server.arg("ledNum").toInt();
    int enabled = server.arg("enabled").toInt();
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
        sendResponse(200, "OK");
    } else {
        sendResponse(400, "Invalid Parameters");
        return;
    }
}

void endpoint_setTimeRemaining() {
    if (!server.hasArg("seconds")){ 
        sendResponse(400, "Missing Parameters");
        return;
    }
    int seconds = server.arg("seconds").toInt();
    gameTimerMs = seconds * 1000;
    Serial.write("Setting Time Remaining: ");
    serialWriteValue(gameTimerMs);
    Serial.write("\n");
    sendResponse(200, "OK");
}

void endpoint_getGameStatus() {
    int gameStatusCount = 0;
    int i;
    for (i = 0; i < NUM_LEDS; i++) {
        if (gameStatus[i]) gameStatusCount++;
    }
    char timestr[16];
    sprintf(timestr, "%d", gameStatusCount);
    Serial.write("Game Status: ");
    serialWriteValue(gameStatusCount);
    Serial.write("\n");
    sendResponse(200, timestr);
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
//   WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_pw);

  server.client().setNoDelay(true);

  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
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
        }
    } else if (wifiStatus == WL_DISCONNECTED) {
        if (wifiIsConnected) {
            wifiIsConnected = false;
            server.stop();
        }
    }
    //     Serial.println("");
    //     Serial.println("Your ESP is connected!");  
    //     Serial.println("Your IP address is: ");
    //     Serial.println(WiFi.localIP());  
    //  }
    server.handleClient();
}