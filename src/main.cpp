#define FASTLED_ESP8266_D1_PIN_ORDER

#include <Arduino.h>
#include <FastLED.h>
#define NUM_LEDS 2
#define DATA_PIN 5

#define BREATHE_MINVALUE 100
#define BREATHE_MAXVALUE 254

#define GLYPH_STARTUP_TIME 220
#define GLYPH_DISABLED -1
#define GLYPH_BREATHING 0

#define ANIM_TIMER_START 900
#define ANIM_EVENT_ONE 850
#define ANIM_EVENT_TWO 523


CRGB leds[NUM_LEDS];
int led_startup_timers[NUM_LEDS];

int breatheValue = BREATHE_MAXVALUE;
int breatheDelta = -1;

int animTimer = ANIM_TIMER_START;

void setup() {
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);

  led_startup_timers[0] = GLYPH_DISABLED;
  led_startup_timers[1] = GLYPH_DISABLED;
}

void loop() {
    CHSV breatheColor = CHSV(100, 255, breatheValue);
    if (breatheValue < BREATHE_MINVALUE) breatheDelta = 1;
    if (breatheValue > BREATHE_MAXVALUE) breatheDelta = -1;
    breatheValue += breatheDelta;

    //if timer is -1, set to black
    //if timer is zero, set to color w/ breath value
    //if timer is zero, set to white faded toward color
    for (int led = 0; led < NUM_LEDS; led++) {
        if (led_startup_timers[led] == GLYPH_DISABLED) {
            leds[led] = CRGB::Black;
        }
        else if (led_startup_timers[led] == GLYPH_BREATHING) {
            leds[led] = breatheColor;
        }
        else {
            int whiteoutAmount = map(led_startup_timers[led], GLYPH_STARTUP_TIME, 0, 0, 255);
            leds[led] = CHSV(100, whiteoutAmount, breatheValue);

            led_startup_timers[led] -= 1;
        }
    }

    //run througha big timer to play a couple different events
    animTimer--;
    if (animTimer < 0) {
        //reset
        led_startup_timers[0] = GLYPH_DISABLED;
        led_startup_timers[1] = GLYPH_DISABLED;
        animTimer = ANIM_TIMER_START;
    }
    if (animTimer == ANIM_EVENT_ONE) {
        led_startup_timers[0] = GLYPH_STARTUP_TIME;
    }
    if (animTimer == ANIM_EVENT_TWO) {
        led_startup_timers[1] = GLYPH_STARTUP_TIME;
    }

   
    FastLED.show();
    delay(40);
 



}