#pragma once

namespace {
class ButtonController {
public:
  enum class Event {
    None,
    Click,
    LongPress
  };
  
  ButtonController(uint8_t pin, uint32_t debounce_ms=50, uint32_t long_press_ms=500) 
    : pin{pin}, debounce_ms{debounce_ms}, long_press_ms{long_press_ms} {
  }

  void begin() {
    pinMode(pin, INPUT_PULLUP);
    last_change = 0;
  }

  Event update() {
    int state = digitalRead(pin);
    // pressed
    if ((last_change == 0) && (state == 0)) {
      last_change = millis();
    }
    // released or glitch
    else if ((last_change != 0) && (state == 1)) {
      uint32_t elapsed = millis() - last_change;
      last_change = 0;
      // passed enough time for long press
      if (elapsed > long_press_ms) {
        return Event::LongPress;
      }
      else if (elapsed > debounce_ms) {
        return Event::Click;
      }
      else {
        // glitched
      }
    }

    return Event::None;
  }
private:
  const uint8_t pin;
  const uint32_t debounce_ms;
  const uint32_t long_press_ms;
  uint32_t last_change;
};
}
