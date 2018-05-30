#define FASTLED_ESP8266_D1_PIN_ORDER

#include <Arduino.h>
#include <FastLED.h>
#define NUM_LEDS 2
#define DATA_PIN 5

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#define BREATHE_TIMER 85

#define GLYPH_STARTUP_TIME 40
#define GLYPH_DISABLED -1
#define GLYPH_BREATHING 0

#define ANIM_TIMER_START 950
#define ANIM_EVENT_ONE 650
#define ANIM_EVENT_TWO 375

const char* wifi_ssid = "DexHunt2018";
const char* wifi_pw = "dexhunt2018";
ESP8266WebServer server(80);

CRGB leds[NUM_LEDS];
CRGB currBreatheColorDisabled = CRGB::Black;
CRGB currBreatheColorEnabled = CRGB::Black;
int led_startup_timers[NUM_LEDS];

int breatheTimer = 0;
int breatheDelta = 1;

int animTimer = ANIM_TIMER_START;
int animMode = 1;

void setup_wifi();
void manage_wifi();

void setup() {
  Serial.begin(9600); 
  setup_wifi();

  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  led_startup_timers[0] = GLYPH_DISABLED;
  led_startup_timers[1] = GLYPH_DISABLED;

  animMode = 1;
}

void loop() {
    manage_wifi();
    if (animMode == 1) {
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

        //if timer is -1, set to black
        //if timer is zero, set to color w/ breath value
        //if timer is zero, set to white faded toward color
        for (int led = 0; led < NUM_LEDS; led++) {

            // leds[led] = ColorFromPalette(myPal, paletteHeatIndex);
            // leds[led] = CRGB::Red;
            if (led_startup_timers[led] == GLYPH_DISABLED) {
                leds[led] = nblend(leds[led], currBreatheColorDisabled, 8);
            }
            else if (led_startup_timers[led] == GLYPH_BREATHING) {
                leds[led] = nblend(leds[led], currBreatheColorEnabled, 8);
            }
            else {
                int whiteoutAmount = map(led_startup_timers[led], GLYPH_STARTUP_TIME, 0, 0, 255);
                leds[led] = nblend(leds[led], CRGB::White, whiteoutAmount);
    
                led_startup_timers[led] -= 1;
            }
        }
    
        //run through a big timer to play a couple different events
        animTimer--;
        if (animTimer < 0) {
            //reset
            led_startup_timers[0] = GLYPH_DISABLED;
            led_startup_timers[1] = GLYPH_DISABLED;
            // ledPalette[0] = green_gp;
            // ledPalette[1] = green_gp;
            animTimer = ANIM_TIMER_START;
        }
        if (animTimer == ANIM_EVENT_ONE) {
            led_startup_timers[0] = GLYPH_STARTUP_TIME;
            // ledPalette[0] = blue_gp;
        }
        if (animTimer == ANIM_EVENT_TWO) {
            led_startup_timers[1] = GLYPH_STARTUP_TIME;
            // ledPalette[1] = blue_gp;
        }
    }
    
   
    FastLED.show();
    delay(70);
 
}

void sendResponse(int code, const char* str) {
    server.sendHeader("Connection", "close");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(code, "text/plain", str);
}
void handleRoot() {
    sendResponse(200, "Hello");
}

void handleNotFound(){
    sendResponse(404, "Not Found"); // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
  }

void endpoint_setLeds() {

    if (!server.hasArg("ledNum") || !server.hasArg("r") || !server.hasArg("g") || !server.hasArg("b")){
        sendResponse(400, "Missing Parameters");
        return;
    }
    int idx = server.arg("ledNum").toInt();
    int r = server.arg("r").toInt();
    int g = server.arg("g").toInt();
    int b = server.arg("b").toInt();
    if (idx >= 0 && idx < NUM_LEDS && 
        r >= 0 && r < 256 &&
        g >= 0 && g < 256 &&
        b >= 0 && b < 256) {
            animMode = 0; //stop animations
            leds[idx] = CRGB(r,g,b);

            sendResponse(200, "OK");
    } else {
        sendResponse(400, "Invalid Parameters");
        return;
    }
}

void endpoint_enableLed() {
    animMode = 1;
    
    if (!server.hasArg("ledNum") || !server.hasArg("enabled")){ 
        sendResponse(400, "Missing Parameters");
        return;
    }
    int idx = server.arg("ledNum").toInt();
    int enabled = server.arg("enabled").toInt();
    if (idx >= 0 && idx < NUM_LEDS) {
        if (enabled == 0)
            led_startup_timers[idx] = GLYPH_DISABLED;
        else
            led_startup_timers[idx] = GLYPH_STARTUP_TIME;
        sendResponse(200, "OK");
    } else {
        sendResponse(400, "Invalid Parameters");
        return;
    }
}

void setup_wifi() {
//   WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_pw);

//   if (MDNS.begin("esp8266")) {
//     Serial.println("MDNS responder started");
//   }

  server.client().setNoDelay(true);

  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.on("/setRGB", endpoint_setLeds);
  server.on("/setEnabled", endpoint_enableLed);
//   server.begin();
  Serial.write("Server is listening");
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