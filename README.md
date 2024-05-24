si5351-nano-ctrl
==

This is a simple Arduino Nano project to control the commonly available SI5351 oscillator board (originally from Adafruit). 1.44" SPI TFT/OLED screen is used as the graphical interface. A rotary encoder and 4 buttons allow menu navigation and frequency adjustments.

Features include:
* Full range of frequencies from 2.5 kHz up to 300 MHz (with PLL overclock, performance guaranteed range limited to 225 MHz, see AN619).
* Automatic selection of PLL operation modes based on frequencies requested.
* Adjustable drive strength.
* Up to 6 presets can be saved to EEPROM. Loads `Preset 0` at startup.
* Xtal frequency correction, also stored on EEPROM.

Provided KiCad design is suitable for one-sided PCBs, e.g. made by toner transfer method at home. For two or more layers, PCB design can be optimized to reduce noise and unwanted coupling of SPI signals to the output.