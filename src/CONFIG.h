#ifndef PIN_H
#define PIN_H

// ===========================================================================
//  ESP-HACK -> LilyGO T-Display-S3 (LCD version, two buttons, ESP32-S3R8)
//
//  GPIO budget on this board:
//    GPIO26..32   quad SPI flash                  -> NOT usable
//    GPIO33..37   octal PSRAM (S3R8 package)      -> NOT usable
//    GPIO19/20    native USB D-/D+                -> NOT usable
//    GPIO5..9,15,38,39..42,45..48  LCD/panel      -> NOT usable
//    GPIO0,14     the two onboard buttons
//    GPIO4        battery voltage divider (left alone)
//  Free pins: 1, 2, 3, 10, 11, 12, 13, 16, 17, 18, 21
// ===========================================================================

// ---- Display --------------------------------------------------------------
// The ESP-HACK UI is laid out for a 128x64 canvas. That logical resolution is
// kept and scaled x2 (256x128), centred on the 320x170 ST7789 panel, so every
// screen in the firmware renders unchanged.
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define TD_PANEL_WIDTH 320
#define TD_PANEL_HEIGHT 170

#define LCD_POWER_ON 15   // board power latch - must go HIGH before init
#define LCD_RST 5
#define LCD_CS 6
#define LCD_DC 7
#define LCD_WR 8
#define LCD_RD 9
#define LCD_BL 38
#define LCD_D0 39
#define LCD_D1 40
#define LCD_D2 41
#define LCD_D3 42
#define LCD_D4 45
#define LCD_D5 46
#define LCD_D6 47
#define LCD_D7 48

// Unused on this port (no I2C OLED), kept so existing references still build.
#define OLED_ADR 0x3C
#define OLED_RESET -1
#define OLED_SCL -1
#define OLED_SDA -1

// ---- Buttons --------------------------------------------------------------
// Only TWO physical keys exist (GPIO0 and GPIO14). Each one carries two
// meanings, split by how long it is held (see src/tds3_buttons.cpp):
//
//   BTN1 (GPIO0)  short tap .... UP      (page up)
//                 long hold .... OK      (accept / select)
//   BTN2 (GPIO14) short tap .... DOWN    (page down)
//                 long hold .... BACK    (go back)
//
// The ids are deliberately outside the valid ESP32-S3 GPIO range so the shim in
// esp_hack_shim.h can recognise them and no stray pinMode() touches hardware.
#define BUTTON_UP 200
#define BUTTON_DOWN 201
#define BUTTON_OK 202
#define BUTTON_BACK 203

#define TD_BTN1_PIN 0
#define TD_BTN2_PIN 14

// ---- Shared SPI bus -------------------------------------------------------
// The T-Display-S3R8 leaves only a handful of usable GPIOs, and GPIO3 is a
// strapping pin (it selects the JTAG source, which would claim the LCD data
// lines 39..42 if it ever read high at reset). It is therefore left unused.
// To fit SD + CC1101 + IR into the remaining safe pins, the SD card and the
// CC1101 share ONE SPI bus with separate chip selects, which is the normal
// way to wire two SPI devices anyway. sdSPI in main.cpp is an alias of the
// global SPI object so both use the same host controller.
// The three bus lines match the official T-Display-S3 TF-shield wiring.
#define SPI_SCK 11
#define SPI_MOSI 13
#define SPI_MISO 12

// ---- SD Card (shares SPI_SCK/MOSI/MISO) -----------------------------------
#define SD_CS 10
#define SD_MOSI SPI_MOSI
#define SD_CLK SPI_SCK
#define SD_MISO SPI_MISO

// ---- CC1101 (shares SPI_SCK/MOSI/MISO) ------------------------------------
#define CC1101_GDO0 1
#define CC1101_CS 2
#define CC1101_SCK SPI_SCK
#define CC1101_MOSI SPI_MOSI
#define CC1101_MISO SPI_MISO

// ---- Infrared -------------------------------------------------------------
// GPIO17/18 are the documented I2C header pins on the LCD (non-touch) variant.
#define IR_TRANSMITTER 18
#define IR_RECIVER 17

// ---- GPIO module ----------------------------------------------------------
// Mirrors the original overlap scheme: GPIO_A is the IR transmitter and
// GPIO_C/D/E are the shared SPI bus lines, so an NRF24 or iButton can reuse
// the CC1101 socket exactly like on the ESP32-WROOM build.
#define GPIO_A 18
#define GPIO_B 17
#define GPIO_C 11
#define GPIO_D 13
#define GPIO_E 12
#define GPIO_F 2

// Firmware version
static const char* FIRMWARE = "v1.3-s3";

#endif
