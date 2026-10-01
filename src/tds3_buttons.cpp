#include "esp_hack_shim.h"

#include "tds3_buttons.h"
#include "CONFIG.h"

namespace {

// ---- raw key sampling with debounce --------------------------------------
struct RawKey {
  bool stable = false;      // debounced level: true = pressed
  bool candidate = false;   // last sampled level
  uint32_t changedAt = 0;   // when the candidate level first appeared
  uint32_t downAt = 0;      // when the current press began
  bool wasDown = false;     // level seen on the previous tick
  bool longFired = false;   // hold action (click) already emitted
  bool holdFired = false;   // long-hold action (isHolded) already emitted

  void sample(bool level, uint32_t now) {
    if (level != candidate) {
      candidate = level;
      changedAt = now;
      return;
    }
    if (level != stable && (now - changedAt) >= TDS3_BTN_DEBOUNCE_MS) {
      stable = level;
      if (level) downAt = now;
    }
  }
};

RawKey key1;   // GPIO0  -> tap: UP,   hold: OK
RawKey key2;   // GPIO14 -> tap: DOWN, hold: BACK

// ---- logical button levels -------------------------------------------------
bool outUp = false;
bool outDown = false;
bool outOk = false;
bool outBack = false;
bool outOkHold = false;
bool outBackHold = false;

uint32_t upPulseUntil = 0;
uint32_t downPulseUntil = 0;
uint32_t okPulseUntil = 0;
uint32_t backPulseUntil = 0;
uint32_t okHoldPulseUntil = 0;
uint32_t backHoldPulseUntil = 0;
uint32_t lastUpdateMs = 0;

inline bool isVirtualPin(uint8_t pin) {
  return pin >= ESP_HACK_VIRTUAL_PIN_BASE;
}

// True once a pulse window has elapsed. Unsigned subtraction keeps this correct
// across millis() wrap-around.
inline bool expired(uint32_t until, uint32_t now) {
  return until != 0 && (int32_t)(now - until) >= 0;
}

// ---------------------------------------------------------------------------
//  One physical key, three meanings, split by how long it is held:
//
//    held <  TDS3_TAP_MAX_MS  -> short action  (UP / DOWN), on release
//    held >= TDS3_TAP_MAX_MS  -> long action   (OK / BACK), fires WHILE held
//    held >= TDS3_HOLD_MS     -> long variant  (OK / BACK) as isHolded()
//
//  The long action fires while the key is still down, not on release. That is
//  the difference between working and not working here: GyverButton clears its
//  pending click flag as soon as a level stays low past its own 500 ms hold
//  timeout, so anything that waits for release and then replays a long pulse
//  can never produce isClick() - and the firmware accepts menu items through
//  isClick() at 53 sites for OK and 60 for BACK.
//
//  TDS3_CLICK_PULSE_MS is therefore kept inside a hard window: above the 50 ms
//  debounce so the press registers, and well below the 500 ms hold timeout so
//  the click survives. The pulse length is fixed and independent of how long
//  the user keeps the key down, which makes the gesture feel instant.
//
//  A tap is still decided on release, so holding a key can never leak a page
//  step into the accept/back action.
// ---------------------------------------------------------------------------
void processKey(RawKey &key, bool &outShort, uint32_t &shortPulseUntil,
                bool &outLongClick, uint32_t &longClickPulseUntil,
                bool &outLongHold, uint32_t &longHoldPulseUntil, uint32_t now) {
  const bool down = key.stable;

  if (down && !key.wasDown) {
    key.longFired = false;
    key.holdFired = false;
  }

  if (down) {
    const uint32_t held = now - key.downAt;

    if (!key.longFired && held >= TDS3_TAP_MAX_MS) {
      key.longFired = true;
      outLongClick = true;
      longClickPulseUntil = now + TDS3_CLICK_PULSE_MS;
      // A pending tap must not survive into the long action.
      outShort = false;
      shortPulseUntil = 0;
    }

    if (!key.holdFired && held >= TDS3_HOLD_MS) {
      key.holdFired = true;
      outLongHold = true;
      longHoldPulseUntil = now + TDS3_HOLD_PULSE_MS;
    }
  } else if (key.wasDown && !key.longFired) {
    // Released before the hold threshold: this was a tap.
    outShort = true;
    shortPulseUntil = now + TDS3_CLICK_PULSE_MS;
  }

  if (outShort && expired(shortPulseUntil, now)) outShort = false;
  if (outLongClick && expired(longClickPulseUntil, now)) outLongClick = false;
  if (outLongHold && expired(longHoldPulseUntil, now)) outLongHold = false;

  key.wasDown = down;
}

}  // namespace

// ---------------------------------------------------------------------------
void tds3ButtonsBegin() {
  pinMode(TD_BTN1_PIN, INPUT_PULLUP);
  pinMode(TD_BTN2_PIN, INPUT_PULLUP);

  const uint32_t now = millis();
  key1 = RawKey();
  key2 = RawKey();
  key1.candidate = (digitalRead(TD_BTN1_PIN) == LOW);
  key2.candidate = (digitalRead(TD_BTN2_PIN) == LOW);
  key1.changedAt = key2.changedAt = now;

  outUp = outDown = outOk = outBack = false;
  outOkHold = outBackHold = false;
  upPulseUntil = downPulseUntil = 0;
  okPulseUntil = backPulseUntil = 0;
  okHoldPulseUntil = backHoldPulseUntil = 0;
  lastUpdateMs = now;
}

// ---------------------------------------------------------------------------
void tds3ButtonsUpdate() {
  const uint32_t now = millis();
  if (now == lastUpdateMs) return;   // idempotent within a millisecond
  lastUpdateMs = now;

  key1.sample(digitalRead(TD_BTN1_PIN) == LOW, now);
  key2.sample(digitalRead(TD_BTN2_PIN) == LOW, now);

  // BTN1: tap -> UP, hold -> OK click, long hold -> OK isHolded
  processKey(key1, outUp, upPulseUntil, outOk, okPulseUntil,
             outOkHold, okHoldPulseUntil, now);
  // BTN2: tap -> DOWN, hold -> BACK click, long hold -> BACK isHolded
  processKey(key2, outDown, downPulseUntil, outBack, backPulseUntil,
             outBackHold, backHoldPulseUntil, now);
}

// ---------------------------------------------------------------------------
int tds3ReadVirtual(uint8_t virtualPin) {
  tds3ButtonsUpdate();

  bool pressed = false;
  switch (virtualPin) {
    case BUTTON_UP:   pressed = outUp; break;
    case BUTTON_DOWN: pressed = outDown; break;
    case BUTTON_OK:   pressed = outOk || outOkHold; break;
    case BUTTON_BACK: pressed = outBack || outBackHold; break;
    default: break;
  }
  // Active-low, matching the original hardware.
  return pressed ? LOW : HIGH;
}

// ---------------------------------------------------------------------------
//  Shim entry points (used by every src/ translation unit, including the
//  vendored GyverButton, so logical buttons never touch real hardware).
// ---------------------------------------------------------------------------
extern "C" int espHackDigitalRead(uint8_t pin) {
  if (isVirtualPin(pin)) return tds3ReadVirtual(pin);
  return digitalRead(pin);
}

extern "C" void espHackPinMode(uint8_t pin, uint8_t mode) {
  if (isVirtualPin(pin)) return;   // logical buttons have no hardware pin
  pinMode(pin, mode);
}

extern "C" void espHackDigitalWrite(uint8_t pin, int value) {
  if (isVirtualPin(pin)) return;
  digitalWrite(pin, value);
}

extern "C" void espHackButtonsBegin(void) { tds3ButtonsBegin(); }
extern "C" void espHackButtonsUpdate(void) { tds3ButtonsUpdate(); }
