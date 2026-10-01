#ifndef ESP_HACK_SHIM_H
#define ESPHACK_SHIM_H

// ---------------------------------------------------------------------------
//  ESP-HACK -> T-Display-S3 GPIO shim
//
//  The original firmware addressed four physical buttons by GPIO number and
//  read them with plain pin reads plus GyverButton. The T-Display-S3 only has
//  TWO keys, so BUTTON_UP/DOWN/OK/BACK in CONFIG.h are software ids (>= 200)
//  that do not correspond to any package pin.
//
//  Every button read in src/ is routed through espHackDigitalRead(), which
//  serves those virtual ids from the gesture engine in tds3_buttons.cpp and
//  forwards everything else to the real Arduino call. espHackPinMode() and
//  espHackDigitalWrite() do the same so that GyverButton's internal pin setup
//  never touches hardware for a logical button.
// ---------------------------------------------------------------------------

#include <Arduino.h>

#define ESP_HACK_VIRTUAL_PIN_BASE 200

#ifdef __cplusplus
extern "C" {
#endif

int  espHackDigitalRead(uint8_t pin);
void espHackPinMode(uint8_t pin, uint8_t mode);
void espHackDigitalWrite(uint8_t pin, int value);
void espHackButtonsBegin(void);
void espHackButtonsUpdate(void);

#ifdef __cplusplus
}
#endif

#endif
