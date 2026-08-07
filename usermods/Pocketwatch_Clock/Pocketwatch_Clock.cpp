#include "wled.h"

/*
 * Pocketwatch Clock - a real WLED EFFECT (not an overlay)
 * ---------------------------------------------------------
 * After installing this, "Pocketwatch Clock" shows up in your normal
 * WLED effects list, right alongside Rainbow, Chase, etc. Select it like
 * any other effect, and it uses WLED's normal controls:
 *   - Color 1 (the regular color wheel) = the hour-hand color
 *   - Intensity slider                  = max brightness of the center glow
 *   - "Breathe" checkbox                = pulse the center pixel once/sec
 *
 * Assumes a 7-pixel NeoPixel Jewel wired center-out:
 *   pixel 0        = center
 *   pixels 1-6     = outer ring
 * If your jewel's pixel 0 isn't the physical center LED, change the two
 * constants below (see readme.md for how to test this).
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
  public:
    void setup() override {
      strip.addEffect(255, &mode_pocketwatch_clock, _data_FX_MODE_POCKETWATCH_CLOCK);
    }
    void loop() override {}
    uint16_t getId() override { return USERMOD_ID_UNSPECIFIED; }

};

static PocketwatchClockUsermod pocketwatch_clock;
REGISTER_USERMOD(pocketwatch_clock);
