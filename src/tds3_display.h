#ifndef TDS3_DISPLAY_H
#define TDS3_DISPLAY_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "CONFIG.h"

// ---------------------------------------------------------------------------
//  TDS3_Display
//
//  ESP-HACK draws everything through the Adafruit_GFX API against a 128x64
//  monochrome canvas (clearDisplay / drawPixel / drawBitmap / display).
//  This class keeps that exact contract but renders the result on the
//  T-Display-S3 colour LCD:
//
//    * a 1024 byte 1bpp framebuffer holds the logical 128x64 image
//    * display() stretches it across the FULL 320x170 panel using non-uniform
//      integer scaling (a mix of 2x and 3x repetitions per axis), so there is
//      no black border and no dead zone
//    * the logical canvas stays 128x64, so every ESP-HACK screen keeps its
//      layout and the firmware needs no changes
//    * invertDisplay() swaps foreground/background, reproducing the
//      White/Black colour scheme the original firmware offers
//
//  The panel is 320:170 while the canvas is 2:1, so no single integer scale
//  can fill it; hence the per-axis mapping tables built in begin().
// ---------------------------------------------------------------------------
class TDS3_Display : public Adafruit_GFX {
public:
  TDS3_Display();

  bool begin();

  // Adafruit_GFX primitives
  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
  void invertDisplay(bool i) override;

  // OLED-compatible surface API used across the firmware
  void clearDisplay();
  void display();
  uint8_t *getBuffer() { return _buffer; }

private:
  static const int16_t BUF_W = SCREEN_WIDTH;
  static const int16_t BUF_H = SCREEN_HEIGHT;

  uint8_t _buffer[(SCREEN_WIDTH * SCREEN_HEIGHT) / 8];
  bool _invert = false;
  bool _ready = false;
};

#endif
