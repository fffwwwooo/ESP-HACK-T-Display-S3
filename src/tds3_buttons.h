#ifndef TDS3_BUTTONS_H
#define TDS3_BUTTONS_H

#include <Arduino.h>

// ---------------------------------------------------------------------------
//  Two physical keys -> four logical ESP-HACK buttons
//
//  The T-Display-S3 carries only BTN1 (GPIO0) and BTN2 (GPIO14). Each key keeps
//  its own meaning for a short press and gains a second meaning while held:
//
//    BTN1 (GPIO0)   tap        -> UP      (page up)
//                   hold       -> OK      (accept / select)
//                   very long  -> OK as isHolded()
//    BTN2 (GPIO14)  tap        -> DOWN    (page down)
//                   hold       -> BACK    (go back)
//                   very long  -> BACK as isHolded()
//
//  espHackDigitalRead() reports a LEVEL rather than an event, so GyverButton and
//  every existing isClick()/isHolded()/isPress() call site keeps working.
//
//  WHEN each gesture is decided matters a great deal here:
//
//    * a tap is decided on RELEASE, so holding a key can never leak a page step
//      into the accept/back action;
//    * a hold is decided WHILE the key is still down, at TDS3_TAP_MAX_MS.
//
//  The hold action cannot wait for release. GyverButton clears its pending click
//  flag as soon as a level stays low past its own hold timeout (setTimeout(500),
//  see tick() in GyverButton.cpp), so anything replayed after release as a long
//  pulse yields isHolded() and NO isClick(). The firmware accepts menu items
//  through isClick() at 53 sites for OK and 60 for BACK, and only checks
//  isHolded() at 2 and 5 sites, so isClick() is the event that has to work.
//
//  The pulse emitted for the hold action is therefore pinned inside a hard
//  window: above the 50 ms debounce so it registers at all, and well below the
//  500 ms hold timeout so the click survives. Its length does not depend on how
//  long the user keeps the key down, so accepting feels immediate.
//
//  Holding past TDS3_HOLD_MS additionally produces one isHolded() event for the
//  few sites that need it (SubGHz "save key"/"brute force", the file explorer,
//  the settings menu). The two events are mutually exclusive in time, so a hold
//  never fires both a back action and a hold action at once.
//
#define TDS3_BTN_DEBOUNCE_MS   12   // raw level must stay stable this long
#define TDS3_TAP_MAX_MS       400   // released before this -> page up / down
#define TDS3_HOLD_MS         2000   // still held past this -> extra isHolded()
#define TDS3_CLICK_PULSE_MS   120   // long action replayed as a pulse this long
#define TDS3_HOLD_PULSE_MS    620   // long-press variant, must exceed 50+500 ms

// TDS3_CLICK_PULSE_MS is bounded on both sides and that is not a free choice:
//
//   * it must stay above GyverButton's 50 ms debounce (setDebounce(50) in
//     main.cpp), otherwise the press is filtered out entirely;
//   * it must stay well below GyverButton's 500 ms hold timeout
//     (setTimeout(500)), because once that timeout elapses tick() clears
//     oneClick_f and isClick() can never fire for that press again.
//
// The firmware accepts menu items through isClick() at 53 sites for OK and 60
// for BACK, so the pulse has to land inside that window.
// Initialise the two physical keys and reset the gesture engine.
void tds3ButtonsBegin();

// Advance the gesture engine. Called automatically by espHackDigitalRead().
void tds3ButtonsUpdate();

// Level of a logical button: LOW = pressed, HIGH = released.
int tds3ReadVirtual(uint8_t virtualPin);

#endif
