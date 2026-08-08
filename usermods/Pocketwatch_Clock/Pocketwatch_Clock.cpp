#include "wled.h"

/*
 * Pocketwatch Clock - a real WLED EFFECT, plus a physical button that
 * temporarily shows the clock over a running playlist.
 * ---------------------------------------------------------------------
 * THE EFFECT
 * "Pocketwatch Clock" shows up in your normal WLED effects list, right
 * alongside Rainbow, Chase, etc. Select it like any other effect:
 *   - Color 1 (the regular color wheel) = the hour-hand color
 *   - Intensity slider                  = max brightness of the center glow
 *   - "Breathe" checkbox                = pulse the center pixel once/sec
 *
 * Assumes a 7-pixel NeoPixel Jewel wired center-out:
 *   pixel 0    = center
 *   pixels 1-6 = outer ring
 *
 * THE BUTTON
 * Wire a momentary push button between a spare GPIO pin and GND. On a
 * press, this usermod:
 *   1. Applies the "Clock" preset (your Pocketwatch Clock effect)
 *   2. Waits the configured duration (default 10s)
 *   3. Applies the "Playlist" preset again, which resumes cycling
 *
 * SETUP (all done in the WLED web UI, no recompiling):
 *   1. Set up your rotating look as a WLED Playlist, save it as a preset,
 *      note its preset number.
 *   2. Select the "Pocketwatch Clock" effect, dial in your color/
 *      brightness, save THAT as a separate preset, note its number.
 *   3. Go to Config -> Usermods -> "Pocketwatch Clock" and fill in:
 *        Button Pin           = the GPIO you wired the button to
 *        Playlist Preset ID   = the preset number from step 1
 *        Clock Preset ID      = the preset number from step 2
 *        Clock Duration (ms)  = how long to show the clock (10000 = 10s)
 */

static constexpr int JEWEL_CENTER_INDEX = 0;  // center pixel
static constexpr int JEWEL_RING_START   = 1;  // first outer-ring pixel
static constexpr int JEWEL_RING_COUNT   = 6;  // number of outer-ring pixels

static void mode_pocketwatch_clock(void) {
  if (SEGLEN < JEWEL_RING_START + JEWEL_RING_COUNT) {
    SEGMENT.fill(SEGCOLOR(0));
    return;
  }

  int h = hour(localTime) % 12;   // 0-11
  int m = minute(localTime);      // 0-59
  int s = second(localTime);      // 0-59

  // ---------- Hour hand: smoothly blended across the outer ring ----------
  uint32_t hourColor = SEGCOLOR(0); // controlled by the normal WLED color wheel

  float hourPos = ((float)h + m / 60.0f) / 12.0f * JEWEL_RING_COUNT;
  int   idxA    = ((int)hourPos) % JEWEL_RING_COUNT;
  int   idxB    = (idxA + 1) % JEWEL_RING_COUNT;
  float frac    = hourPos - (int)hourPos;

  for (int i = 0; i < JEWEL_RING_COUNT; i++) {
    SEGMENT.setPixelColor(JEWEL_RING_START + i, (uint32_t)0);
  }
  SEGMENT.setPixelColor(JEWEL_RING_START + idxA,
      color_blend(hourColor, (uint32_t)0, (uint8_t)(frac * 255)));
  SEGMENT.setPixelColor(JEWEL_RING_START + idxB,
      color_blend((uint32_t)0, hourColor, (uint8_t)(frac * 255)));

  // ---------- Center pixel: hue sweeps through the hour ----------
  uint16_t hue16 = (uint16_t)(((m * 60 + s) / 3600.0f) * 65535.0f);
  byte rgb[3];
  colorHStoRGB(hue16, 255, rgb);
  uint32_t minuteColor = RGBW32(rgb[0], rgb[1], rgb[2], 0);

  uint8_t centerBrightness = SEGMENT.intensity; // Intensity slider = max brightness
  if (SEGMENT.check1) { // "Breathe" checkbox
    float phase = (s * 1000 + (millis() % 1000)) / 1000.0f;
    uint8_t breath = (uint8_t)(127 + 127 * sinf(phase * 2.0f * PI));
    centerBrightness = scale8(centerBrightness, breath);
  }
  SEGMENT.setPixelColor(JEWEL_CENTER_INDEX, color_blend((uint32_t)0, minuteColor, centerBrightness));
}
static const char _data_FX_MODE_POCKETWATCH_CLOCK[] PROGMEM = "Pocketwatch Clock@,Center Brightness,,,,Breathe;Hour;;;o1=1,ix=160";

class PocketwatchClockUsermod : public Usermod {
  private:
    // ---- configurable via Config -> Usermods, no recompiling needed ----
    int8_t   buttonPin         = 3;      // GPIO the button is wired to (Xiao D1 by default)
    uint8_t  playlistPresetID  = 1;      // preset # of your rotating playlist
    uint8_t  clockPresetID     = 2;      // preset # of the Pocketwatch Clock look
    uint32_t clockShowMillis   = 10000;  // how long to show the clock, in ms

    // ---- runtime state ----
    bool     initDone           = false;
    bool     stableButtonState  = HIGH;  // HIGH = not pressed (pull-up, active low)
    bool     lastRawReading     = HIGH;
    uint32_t lastDebounceTime   = 0;
    static constexpr uint32_t debounceDelay = 50;

    bool     clockActive        = false;
    uint32_t clockActiveUntil   = 0;

  public:
    void setup() override {
      if (buttonPin >= 0) pinMode(buttonPin, INPUT_PULLUP);
      strip.addEffect(255, &mode_pocketwatch_clock, _data_FX_MODE_POCKETWATCH_CLOCK);
      initDone = true;
    }

    void loop() override {
      if (!initDone || buttonPin < 0) return;

      // --- debounce the button ---
      bool reading = digitalRead(buttonPin);
      if (reading != lastRawReading) lastDebounceTime = millis();
      lastRawReading = reading;

      if (millis() - lastDebounceTime > debounceDelay && reading != stableButtonState) {
        stableButtonState = reading;
        if (stableButtonState == LOW) { // confirmed press
          applyPreset(clockPresetID);
          clockActive = true;
          clockActiveUntil = millis() + clockShowMillis;
        }
      }

      // --- auto-return to the playlist after the clock's had its time ---
      if (clockActive && millis() > clockActiveUntil) {
        clockActive = false;
        applyPreset(playlistPresetID);
      }
    }

    void addToConfig(JsonObject& root) override {
      JsonObject top = root.createNestedObject(F("Pocketwatch Clock"));
      top[F("Button Pin (-1 = no button)")] = buttonPin;
      top[F("Playlist Preset ID")]          = playlistPresetID;
      top[F("Clock Preset ID")]             = clockPresetID;
      top[F("Clock Duration (ms)")]         = clockShowMillis;
    }

    bool readFromConfig(JsonObject& root) override {
      JsonObject top = root[F("Pocketwatch Clock")];
      bool configComplete = !top.isNull();
      configComplete &= getJsonValue(top[F("Button Pin (-1 = no button)")], buttonPin, 3);
      configComplete &= getJsonValue(top[F("Playlist Preset ID")], playlistPresetID, 1);
      configComplete &= getJsonValue(top[F("Clock Preset ID")], clockPresetID, 2);
      configComplete &= getJsonValue(top[F("Clock Duration (ms)")], clockShowMillis, 10000);
      return configComplete;
    }

    uint16_t getId() override {
      return USERMOD_ID_UNSPECIFIED;
    }
};

static PocketwatchClockUsermod pocketwatch_clock;
REGISTER_USERMOD(pocketwatch_clock);
