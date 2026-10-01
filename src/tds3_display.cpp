#include "tds3_display.h"

#include <Arduino_GFX_Library.h>

// ST7789 panel on the T-Display-S3, driven over an 8080-II 8-bit parallel bus.
static Arduino_ESP32PAR8Q *g_bus = nullptr;
static Arduino_ST7789 *g_panel = nullptr;

// ---------------------------------------------------------------------------
//  Non-uniform integer scaling that covers the whole 320x170 panel.
//
//  The logical canvas stays 128x64 because that is what every ESP-HACK screen
//  (menus, games, explorer, the doom renderer) is laid out for, and the panel
//  aspect ratio 320:170 is not 2:1, so no single integer scale can fill it.
//  128 -> 320 is 2.5 columns per logical pixel and 64 -> 170 is 2.65625 rows,
//  so both axes use a mix of 2x and 3x repetitions distributed evenly.
//
//  The mapping tables are exact prefix sums (x * PANEL / LOGICAL), which
//  guarantees the spans tile the panel with no gap, no overlap and no leftover
//  black border.
// ---------------------------------------------------------------------------
static const int16_t kPanelW = TD_PANEL_WIDTH;    // 320
static const int16_t kPanelH = TD_PANEL_HEIGHT;   // 170

static int16_t g_colStart[SCREEN_WIDTH + 1];      // logical x -> panel column
static int16_t g_rowStart[SCREEN_HEIGHT + 1];     // logical y -> panel row

// Rows are pushed in bands so the framebuffer stays small while avoiding one
// address-window setup per panel row. A logical row expands to at most 3 panel
// rows, so a band of kBandLogicalRows needs at most 3x that many panel rows.
static const int16_t kBandLogicalRows = 4;
static const int16_t kBandMaxPanelRows = kBandLogicalRows * 3;
static uint16_t g_bandBuffer[kBandMaxPanelRows * TD_PANEL_WIDTH];

#define RGB565_WHITE 0xFFFF
#define RGB565_BLACK 0x0000

// ---------------------------------------------------------------------------
void tdPanelPowerOn() {
  // The board latches its own power; without this the panel never comes up.
  pinMode(LCD_POWER_ON, OUTPUT);
  digitalWrite(LCD_POWER_ON, HIGH);
}

void tdPanelBacklight(bool on) {
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, on ? HIGH : LOW);
}

// ---------------------------------------------------------------------------
TDS3_Display::TDS3_Display() : Adafruit_GFX(SCREEN_WIDTH, SCREEN_HEIGHT) {
  memset(_buffer, 0, sizeof(_buffer));
}

bool TDS3_Display::begin() {
  tdPanelPowerOn();
  // Backlight must come on BEFORE the panel is initialised: if the ST7789 init
  // ever fails the screen would otherwise stay dark and look dead, which makes
  // a working UI indistinguishable from a broken one.
  tdPanelBacklight(true);
  delay(10);

  if (g_bus == nullptr) {
    g_bus = new Arduino_ESP32PAR8Q(LCD_DC, LCD_CS, LCD_WR, LCD_RD,
                                   LCD_D0, LCD_D1, LCD_D2, LCD_D3,
                                   LCD_D4, LCD_D5, LCD_D6, LCD_D7);
  }
  if (g_panel == nullptr) {
    // 170x320 ST7789 with the 35 column offset used by the T-Display-S3.
    g_panel = new Arduino_ST7789(g_bus, LCD_RST, 0 /* rotation */, true /* IPS */,
                                 170 /* w */, 320 /* h */,
                                 35 /* col off 1 */, 0 /* row off 1 */,
                                 35 /* col off 2 */, 0 /* row off 2 */);
  }

  if (!g_panel->begin()) {
    return false;
  }

  g_panel->setRotation(1);   // -> 320x170
  g_panel->fillScreen(RGB565_BLACK);
  g_panel->invertDisplay(false);

  // Exact prefix-sum mapping so the scaled image tiles the whole panel.
  for (int16_t x = 0; x <= BUF_W; x++) g_colStart[x] = (int16_t)((int32_t)x * kPanelW / BUF_W);
  for (int16_t y = 0; y <= BUF_H; y++) g_rowStart[y] = (int16_t)((int32_t)y * kPanelH / BUF_H);

  _width = SCREEN_WIDTH;
  _height = SCREEN_HEIGHT;
  _ready = true;

  return true;
}

void TDS3_Display::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if (x < 0 || x >= BUF_W || y < 0 || y >= BUF_H) return;

  // Adafruit_GFX applies the current rotation/scroll to drawPixel calls; the
  // firmware only ever uses rotation 0, but honour the transforms anyway.
  switch (getRotation()) {
    case 1: { int16_t t = x; x = y; y = BUF_H - 1 - t; break; }
    case 2: { x = BUF_W - 1 - x; y = BUF_H - 1 - y; break; }
    case 3: { int16_t t = x; x = BUF_W - 1 - y; y = t; break; }
    default: break;
  }
  if (x < 0 || x >= BUF_W || y < 0 || y >= BUF_H) return;

  const uint16_t byteIndex = (uint16_t)(y * (BUF_W / 8) + (x / 8));
  const uint8_t bitMask = (uint8_t)(0x80 >> (x & 7));

  if (color) _buffer[byteIndex] |= bitMask;
  else       _buffer[byteIndex] &= (uint8_t)~bitMask;
}

void TDS3_Display::invertDisplay(bool i) {
  _invert = i;
}

void TDS3_Display::clearDisplay() {
  // The framebuffer stores foreground bits; clearing means "no foreground".
  memset(_buffer, 0, sizeof(_buffer));
}

void TDS3_Display::display() {
  if (!_ready || g_panel == nullptr) return;

  const uint16_t fg = _invert ? RGB565_BLACK : RGB565_WHITE;
  const uint16_t bg = _invert ? RGB565_WHITE : RGB565_BLACK;
  const int16_t bytesPerRow = BUF_W / 8;

  for (int16_t y0 = 0; y0 < BUF_H; y0 += kBandLogicalRows) {
    const int16_t y1 = (int16_t)min((int16_t)(y0 + kBandLogicalRows), BUF_H);

    // Panel rows covered by this band; the tables are exact prefix sums, so
    // consecutive bands abut perfectly and together cover 0..kPanelH.
    const int16_t panelRowTop = g_rowStart[y0];
    const int16_t panelRowEnd = g_rowStart[y1];
    const int16_t panelRows = panelRowEnd - panelRowTop;
    if (panelRows <= 0) continue;

    int32_t out = 0;
    for (int16_t y = y0; y < y1; y++) {
      const uint8_t *row = &_buffer[y * bytesPerRow];
      const int16_t rowStart = out;

      // Expand one logical row (128 px) across the full 320 panel columns.
      for (int16_t x = 0; x < BUF_W; x++) {
        const bool lit = (row[x >> 3] & (uint8_t)(0x80 >> (x & 7))) != 0;
        const uint16_t color = lit ? fg : bg;
        const int16_t span = g_colStart[x + 1] - g_colStart[x];
        for (int16_t i = 0; i < span; i++) g_bandBuffer[out++] = color;
      }

      // Reuse the row just built for the remaining vertical repetitions.
      const int16_t vSpan = g_rowStart[y + 1] - g_rowStart[y];
      const int16_t rowPixels = out - rowStart;
      for (int16_t v = 1; v < vSpan; v++) {
        memcpy(&g_bandBuffer[out], &g_bandBuffer[rowStart], rowPixels * sizeof(uint16_t));
        out += rowPixels;
      }
    }

    g_panel->draw16bitRGBBitmap(0, panelRowTop, g_bandBuffer, kPanelW, panelRows);
  }
}
