#include "Arduino.h"
#pragma once

namespace {
class RotaryEncoder {
public:
  enum class Event {
    None,
    CW,
    CCW
  };

  RotaryEncoder(uint8_t pin_a, uint8_t pin_b)
    : pin_a{ pin_a }, pin_b{ pin_b } {
  }

  void begin() {
    last_event = Event::None;

    pinMode(pin_a, INPUT_PULLUP);
    pinMode(pin_b, INPUT_PULLUP);

    uint8_t port_a_num = digitalPinToPort(pin_a);
    
    mask_a = digitalPinToBitMask(pin_a);
    mask_b = digitalPinToBitMask(pin_b);
    port_a = portInputRegister(port_a_num);
    port_b = portInputRegister(digitalPinToPort(pin_b));

    // turn on PCINT
    switch (port_a_num) {
      case 2:
        PCICR |= (1 << PCIE0);
        PCMSK0 |= mask_a;
        break;
      case 3:
        PCICR |= (1 << PCIE1);
        PCMSK1 |= mask_a;
        break;
      default:
        Serial.print("E: cannot resolve int. regs for pins ");
        Serial.print(pin_a);
        Serial.println(pin_b);
        break;
    }
  }

  Event update() {
    Event r = last_event;
    last_event = Event::None;
    return r;
  }

  void callback() {
    // do not override events
    if (last_event == Event::None) {
      if (((*port_a) & mask_a) == 0) {
        if ((*port_b) & mask_b) last_event = Event::CW;
        else last_event = Event::CCW;
      }
    }
  }

private:
  const uint8_t pin_a, pin_b;
  // direct access while in interrupt
  volatile uint8_t *port_a, *port_b;
  uint8_t mask_a, mask_b;
  Event last_event;
};
}
