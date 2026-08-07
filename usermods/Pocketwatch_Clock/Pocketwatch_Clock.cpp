#include "wled.h"

/*
 * Pocketwatch Clock usermod
 * -------------------------
 * Draws a smooth analog-style clock on a 7-pixel NeoPixel Jewel:
 *   - Pixel 0 (center)      = "minute glow": color sweeps through a rainbow
 *                             over the hour, so its hue tells you roughly
 *                             how far through the hour you are.
 *   - Pixels 1-6 (outer ring) = "hour hand": one warm dot that smoothly
 *                             blends between two neighboring LEDs as the
 *                             hour progresses, instead of jumping.
 *   - Center pixel also gently "breathes" in brightness once per second,
 *     just as a subtle sign the piece is alive and ticking.
 *
 * This draws as an OVERLAY on top of whatever effect/preset is currently
 * running, exactly like WLED's built-in Analog Clock usermod - so you can
 * still use the rest of WLED normally and just toggle this on when you
 * want the clock face.
 *
 * IMPORTANT: Update JEWEL_CENTER_INDEX / JEWEL_RING_START below if your
 * physical pixel 0 isn't actually the center LED - see the readme.md
 * for a quick way to test this.
 */

class PocketwatchClockUsermod : public Usermod {
  private:
    // ---- pixel layout of your Jewel-7 (0-indexed) ----
    static constexpr int JEWEL_CENTER_INDEX = 0;  // center pixel
    static constexpr int JEWEL_RING_START   = 1;  // first outer-ring pixel
    static constexpr int JEWEL_RING_COUNT   = 6;  // number of outer-ring pixels

    // ---- configurable via the WLED usermod settings page ----
    bool     enabled      = true;
    uint32_t hourColor    = 0xFF3C00; // warm orange hour dot
    uint8_t  minuteBright = 160;      // max brightness of the center "minute" glow
    bool     breatheSeconds = true;   // pulse the center pixel once per second

    // ---- runtime ----
    bool     initDone        = false;
    uint32_t lastOverlayDraw = 0;
    static constexpr uint32_t refreshDelay = 33; // ~30fps is plenty for a slow clock

  public:
    void setup() override {
      initDone = true;
    }

    void loop() override {
      // Keep the overlay refreshing even if no effect is actively animating.
      if (enabled && millis() - lastOverlayDraw > refreshDelay) {
        strip.trigger();
      }
    }

    void handleOverlayDraw() override {
      if (!enabled) return;
      lastOverlayDraw = millis();

      int h = hour(localTime) % 12;   // 0-11
      int m = minute(localTime);      // 0-59
      int s = second(localTime);      // 0-59

      // ---------- Hour hand: smoothly blended across the outer ring ----------
      // Position 0.0 - JEWEL_RING_COUNT, continuous (includes minutes for smoothness)
      float hourPos = ((float)h + m / 60.0f) / 12.0f * JEWEL_RING_COUNT;
      int   idxA    = ((int)hourPos) % JEWEL_RING_COUNT;
      int   idxB    = (idxA + 1) % JEWEL_RING_COUNT;
      float frac    = hourPos - (int)hourPos; // 0.0 -> at idxA, 1.0 -> at idxB

      for (int i = 0; i < JEWEL_RING_COUNT; i++) {
        strip.setPixelColor(JEWEL_RING_START + i, (uint32_t)0);
      }
      strip.setPixelColor(JEWEL_RING_START + idxA,
          color_blend(hourColor, (uint32_t)0, (uint8_t)(frac * 255)));
      strip.setPixelColor(JEWEL_RING_START + idxB,
          color_blend((uint32_t)0, hourColor, (uint8_t)(frac * 255)));

      // ---------- Center pixel: hue sweeps through the hour ----------
      uint16_t hue16 = (uint16_t)(((m * 60 + s) / 3600.0f) * 65535.0f);
      byte rgb[3];
      colorHStoRGB(hue16, 255, rgb);
      uint32_t minuteColor = RGBW32(rgb[0], rgb[1], rgb[2], 0);

      uint8_t centerBrightness = minuteBright;
      if (breatheSeconds) {
        // gentle sine breathing, once per second, scaled into minuteBright's range
        float phase = (s * 1000 + (millis() % 1000)) / 1000.0f; // 0.0-1.0 over the second... approx
        uint8_t breath = (uint8_t)(127 + 127 * sinf(phase * 2.0f * PI));
        centerBrightness = scale8(minuteBright, breath);
      }
      strip.setPixelColor(JEWEL_CENTER_INDEX, color_blend((uint32_t)0, minuteColor, centerBrightness));
    }

    void addToConfig(JsonObject& root) override {
      JsonObject top = root.createNestedObject(F("Pocketwatch Clock"));
      top[F("Enabled")]              = enabled;
      top[F("Hour Color (RRGGBB)")]  = String(hourColor, HEX);
      top[F("Minute Glow Max Brightness (0-255)")] = minuteBright;
      top[F("Breathe on Seconds")]   = breatheSeconds;
    }

    bool readFromConfig(JsonObject& root) override {
      JsonObject top = root[F("Pocketwatch Clock")];
      bool configComplete = !top.isNull();

      String color;
      configComplete &= getJsonValue(top[F("Enabled")], enabled, true);
      configComplete &= getJsonValue(top[F("Hour Color (RRGGBB)")], color, F("FF3C00"));
      if (color.length()) hourColor = strtoul(color.c_str(), nullptr, 16);
      configComplete &= getJsonValue(top[F("Minute Glow Max Brightness (0-255)")], minuteBright, 160);
      configComplete &= getJsonValue(top[F("Breathe on Seconds")], breatheSeconds, true);

      return configComplete;
    }

    void addToJsonInfo(JsonObject& root) override {
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");
      JsonArray infoArr = user.createNestedArray("Pocketwatch Clock");
      infoArr.add(enabled ? "on" : "off");
    }

    uint16_t getId() override {
      return USERMOD_ID_UNSPECIFIED;
    }
};

static PocketwatchClockUsermod pocketwatch_clock;
REGISTER_USERMOD(pocketwatch_clock);
