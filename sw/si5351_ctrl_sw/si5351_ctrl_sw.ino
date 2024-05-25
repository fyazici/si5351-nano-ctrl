#include <Wire.h>
#include <SPI.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>     // Core graphics library
#include <Adafruit_ST7735.h>  // Hardware-specific library for ST7735
#include <Fonts/FreeMono9pt7b.h>
#include "SI5351_Ctrl.hpp"
#include "ButtonController.hpp"
#include "RotaryEncoder.hpp"

// cs, dc, rst
Adafruit_ST7735 tft = Adafruit_ST7735(6, 4, 5);

SI5351_Controller clock_ctrl;
ButtonController btn_s(A2), btn_w(A3), btn_n(8), btn_e(7), btn_enc(12);
RotaryEncoder renc(A0, A1);

constexpr int EEPROM_PRESET_BASE = 0;
constexpr int EEPROM_XTAL_CORR_BASE = 124;

bool disp_redraw = true;

enum class app_state : int {
  SEL_CH = 1,
  ADJ_FREQ = 2,
  PRESET = 3,
  XTAL_CORR = 4,
  APP_INFO = 5
};

const char * APP_INFO_STR =
  "SI5351 Controller\n"
  "w/ Arduino Nano\n\n"
  "Build:\n" 
  __DATE__ " " __TIME__ "\n"
  "github.com/fyazici\n/si5351-nano-ctrl"
;

app_state state;
uint8_t ch_sel, digit_sel, preset_sel;
GFXcanvas1 canvas(128, 32);

void setup() {
  Wire.begin();
  Serial.begin(115200);

  Serial.println(APP_INFO_STR);

  {
    int32_t xtal_freq;
    EEPROM.get(EEPROM_XTAL_CORR_BASE, xtal_freq);
    clock_ctrl.begin(xtal_freq);

    SI5351_Controller::Config cfg;
    int addr = EEPROM_PRESET_BASE;
    EEPROM.get(addr, cfg);
    if (!clock_ctrl.set_config(cfg)) {
      clock_ctrl.set_default();
    }
  }

  // display backlight
  pinMode(3, OUTPUT);
  digitalWrite(3, HIGH);

  tft.initR(INITR_144GREENTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);

  canvas.setFont(&FreeMono9pt7b);
  canvas.setTextSize(1);

  btn_s.begin();
  btn_w.begin();
  btn_n.begin();
  btn_e.begin();
  btn_enc.begin();
  renc.begin();

  state = app_state::SEL_CH;
  ch_sel = 0;
  digit_sel = 0;
  preset_sel = 0;
}

void print_freq(uint8_t x, uint8_t y, uint32_t freq, int8_t digit_sel) {
  uint8_t xn;
  for (int8_t i = 0; i < 11; i++) {
    xn = x + 11 * (10 - i);
    canvas.setCursor(xn, y + 12);
    if ((freq > 0) && ((i == 3) || (i == 7))) canvas.print("'");
    else {
      if ((freq > 0) || (i == 0)) canvas.print((char)((freq % 10) + 0x30));
      else canvas.print(" ");  // for bg color to show up
      freq /= 10;
    }
  }
  if (digit_sel != -1) {
    if (digit_sel < 3) {
      canvas.fillRect(x + 11 * (10 - digit_sel), y + 14, 11, 2, 1);
    } else if (digit_sel < 6) {
      canvas.fillRect(x + 11 * (9 - digit_sel), y + 14, 11, 2, 1);
    } else {
      canvas.fillRect(x + 11 * (8 - digit_sel), y + 14, 11, 2, 1);
    }
  }
}

void print_freq(uint8_t x, uint8_t y, uint32_t freq) {
  print_freq(x, y, freq, -1);
}

int32_t ipow(int32_t b, int32_t p) {
  int32_t r = 1;
  while (p--) r *= b;
  return r;
}

void fastDraw1(const uint8_t x, const uint8_t y, uint8_t *bitmap, const uint8_t w, const uint8_t h, const uint16_t color, const uint16_t bg) {
  const uint16_t n = w * h / 8;
  uint8_t b;

  tft.startWrite();
  tft.setAddrWindow(x, y, w, h);
  for (uint16_t i = 0; i < n; ++i) {
    b = bitmap[i];
    // unroll for performance
    tft.SPI_WRITE16((b & 0x80) ? color : bg);
    tft.SPI_WRITE16((b & 0x40) ? color : bg);
    tft.SPI_WRITE16((b & 0x20) ? color : bg);
    tft.SPI_WRITE16((b & 0x10) ? color : bg);
    tft.SPI_WRITE16((b & 0x08) ? color : bg);
    tft.SPI_WRITE16((b & 0x04) ? color : bg);
    tft.SPI_WRITE16((b & 0x02) ? color : bg);
    tft.SPI_WRITE16((b & 0x01) ? color : bg);
  }
  tft.endWrite();
}

void ui_draw_ch(uint8_t x, uint8_t y, uint8_t ch, bool selected, uint8_t digit_sel = -1) {
  canvas.fillScreen(0);
  canvas.setFont(&FreeMono9pt7b);

  canvas.setCursor(0, 12);
  canvas.print("C");
  canvas.print(ch);

  canvas.setCursor(36, 12);
  canvas.print(clock_ctrl.get_ch_pll_dsn(ch));
  canvas.print(clock_ctrl.get_ch_pll_mode(ch));

  canvas.setCursor(66, 12);
  canvas.print(clock_ctrl.get_ch_drive_dsn(ch));
  canvas.print("mA ");

  if (clock_ctrl.get_ch_en(ch)) {
    canvas.drawFastHLine(107, 12, 5, 1);
    canvas.drawFastVLine(112, 2, 11, 1);
    canvas.drawFastHLine(112, 2, 5, 1);
    canvas.drawFastVLine(117, 2, 11, 1);
    canvas.drawFastHLine(117, 12, 5, 1);
    canvas.drawFastVLine(122, 2, 11, 1);
    canvas.drawFastHLine(122, 2, 5, 1);
  }
  else {
    canvas.drawFastHLine(107, 7, 20, 1);
  }

  print_freq(6, 14, clock_ctrl.get_ch_freq(ch), digit_sel);

  if (selected) {
    fastDraw1(x, y, canvas.getBuffer(), canvas.width(), canvas.height(), ST77XX_BLACK, ST7735_WHITE);
  } else {
    fastDraw1(x, y, canvas.getBuffer(), canvas.width(), canvas.height(), ST7735_WHITE, ST7735_BLACK);
  }
}

void ui_draw_freq_corr(uint8_t x, uint8_t y, uint8_t digit_sel) {
  canvas.fillScreen(0);
  canvas.setFont(&FreeMono9pt7b);

  canvas.setCursor(0, 12);
  canvas.print("XTAL CORR:");

  print_freq(0, 13, clock_ctrl.get_xtal_freq(), digit_sel);

  fastDraw1(x, y, canvas.getBuffer(), canvas.width(), canvas.height(), ST77XX_WHITE, ST7735_BLUE);
}

void ui_draw_presets(uint8_t x, uint8_t y, uint8_t preset_sel) {
  for (uint8_t i = 0; i < 6; i++) {
    canvas.fillScreen(0);
    canvas.setFont(&FreeMono9pt7b);
    canvas.setCursor(0, 12);
    canvas.print("Preset ");
    canvas.print(i);
    if (i == preset_sel) {
      fastDraw1(x, y + i * 16, canvas.getBuffer(), 128, 16, ST77XX_BLACK, ST7735_WHITE);
    } else {
      fastDraw1(x, y + i * 16, canvas.getBuffer(), 128, 16, ST7735_WHITE, ST7735_BLACK);
    }
  }
}

void ui_draw_info_panel(uint8_t x, uint8_t y) {
  canvas.fillScreen(0);
  // using smaller 8x5 fixed font for bottom info panel
  canvas.setFont();
  canvas.drawFastHLine(0, 0, 128, 1);

  switch (state) {
    case app_state::SEL_CH:
      canvas.setCursor(0, 4);
      canvas.print(" SEL ");
      canvas.setCursor(0, 20);
      canvas.print("(I/O)");
      canvas.drawFastVLine(35, 0, 32, 1);
      canvas.setCursor(65, 2);
      canvas.print("PRESET");
      canvas.setCursor(48, 13);
      canvas.print("-");
      canvas.setCursor(103, 13);
      canvas.print("INFO");
      canvas.setCursor(68, 24);
      canvas.print("ENTER");
      break;
    case app_state::ADJ_FREQ:
      canvas.setCursor(0, 4);
      canvas.print(" ADJ ");
      canvas.setCursor(0, 20);
      canvas.print("(->0)");
      canvas.drawFastVLine(35, 0, 32, 1);
      canvas.setCursor(68, 2);
      canvas.print("DRIVE");
      canvas.setCursor(48, 13);
      canvas.print("<");
      canvas.setCursor(112, 13);
      canvas.print(">");
      canvas.setCursor(71, 24);
      canvas.print("BACK");
      break;
    case app_state::PRESET:
      canvas.setCursor(3, 4);
      canvas.print(" SEL ");
      canvas.setCursor(0, 20);
      canvas.print("(SAVE)");
      canvas.drawFastVLine(35, 0, 32, 1);
      canvas.setCursor(71, 2);
      canvas.print("LOAD");
      canvas.setCursor(48, 13);
      canvas.print("-");
      canvas.setCursor(91, 13);
      canvas.print("DELETE");
      canvas.setCursor(71, 24);
      canvas.print("BACK");
      break;
    case app_state::XTAL_CORR:
      canvas.setCursor(0, 4);
      canvas.print(" ADJ ");
      canvas.setCursor(0, 20);
      canvas.print("(->0)");
      canvas.drawFastVLine(35, 0, 32, 1);
      canvas.setCursor(71, 2);
      canvas.print("SAVE");
      canvas.setCursor(48, 13);
      canvas.setCursor(48, 13);
      canvas.print("<");
      canvas.setCursor(112, 13);
      canvas.print(">");
      canvas.setCursor(71, 24);
      canvas.print("BACK");
      break;
    case app_state::APP_INFO:
      canvas.setCursor(0, 4);
      canvas.print("  -  ");
      canvas.setCursor(0, 20);
      canvas.print("  -  ");
      canvas.drawFastVLine(35, 0, 32, 1);
      canvas.setCursor(80, 2);
      canvas.print("-");
      canvas.setCursor(48, 13);
      canvas.setCursor(48, 13);
      canvas.print("-");
      canvas.setCursor(112, 13);
      canvas.print("-");
      canvas.setCursor(71, 24);
      canvas.print("BACK");
    default:
      break;
  }

  fastDraw1(x, y, canvas.getBuffer(), 128, 32, ST77XX_RED, ST77XX_BLACK);
}

void ui_draw_app_info(uint8_t x, uint8_t y) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setFont();
  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(x, y);
  tft.print(APP_INFO_STR);
}

void process_sel_ch() {
  switch (renc.update()) {
    case RotaryEncoder::Event::CW:
      if (ch_sel == 0) ch_sel = 2;
      else ch_sel--;
      disp_redraw = true;
      break;
    case RotaryEncoder::Event::CCW:
      ch_sel++;
      if (ch_sel > 2) ch_sel = 0;
      disp_redraw = true;
      break;
    default:
      break;
  }
  switch (btn_s.update()) {
    case ButtonController::Event::Click:
      state = app_state::ADJ_FREQ;
      disp_redraw = true;
      break;
    case ButtonController::Event::LongPress:
      state = app_state::PRESET;
      disp_redraw = true;
      break;
    default:
      break;
  }
  switch (btn_n.update()) {
    case ButtonController::Event::Click:
      state = app_state::PRESET;
      disp_redraw = true;
      break;
    default:
      break;
  }
  switch (btn_e.update()) {
    case ButtonController::Event::Click:
      state = app_state::APP_INFO;
      disp_redraw = true;
      break;
    default:
      break;
  }
  switch (btn_enc.update()) {
    case ButtonController::Event::Click:
      clock_ctrl.set_ch_en(ch_sel, !clock_ctrl.get_ch_en(ch_sel));
      disp_redraw = true;
      break;
    case ButtonController::Event::LongPress:
      state = app_state::XTAL_CORR;
      disp_redraw = true;
    default:
      break;
  }
}

void process_adj_freq() {
  int32_t freq = clock_ctrl.get_ch_freq(ch_sel);

  switch (renc.update()) {
    case RotaryEncoder::Event::CW:
      freq += ipow(10, digit_sel);
      freq = min(300000000, freq);
      clock_ctrl.set_ch_freq(ch_sel, freq);
      disp_redraw = true;
      break;
    case RotaryEncoder::Event::CCW:
      freq -= ipow(10, digit_sel);
      freq = max(2500, freq);
      clock_ctrl.set_ch_freq(ch_sel, freq);
      disp_redraw = true;
      break;
    default:
      break;
  }
  switch (btn_enc.update()) {
    case ButtonController::Event::Click:
      freq = (freq / ipow(10, digit_sel + 1)) * ipow(10, digit_sel + 1);
      freq = min(300000000, max(2500, freq));
      clock_ctrl.set_ch_freq(ch_sel, freq);
      disp_redraw = true;
      break;
    default:
      break;
  }
  switch (btn_s.update()) {
    case ButtonController::Event::Click:
      state = app_state::SEL_CH;
      disp_redraw = true;
      break;
    default:
      break;
  }
  switch (btn_e.update()) {
    case ButtonController::Event::Click:
      {
        if (digit_sel == 0) digit_sel = 8;
        else digit_sel--;
        disp_redraw = true;
        break;
      }
    default:
      break;
  }
  switch (btn_w.update()) {
    case ButtonController::Event::Click:
      {
        digit_sel++;
        if (digit_sel > 8) digit_sel = 0;
        disp_redraw = true;
        break;
      }
    default:
      break;
  }
  switch (btn_n.update()) {
    case ButtonController::Event::Click:
      {
        auto mode = clock_ctrl.get_ch_drive_strength(ch_sel);
        mode = (mode + 1) % 4;
        clock_ctrl.set_ch_drive_strength(ch_sel, mode);
        disp_redraw = true;
      }
      break;
    default:
      break;
  }
}

void process_preset() {
  switch (renc.update()) {
    case RotaryEncoder::Event::CW:
      if (preset_sel == 0) preset_sel = 5;
      else preset_sel--;
      disp_redraw = true;
      break;
    case RotaryEncoder::Event::CCW:
      preset_sel++;
      if (preset_sel > 5) preset_sel = 0;
      disp_redraw = true;
      break;
    default:
      break;
  }
  switch (btn_s.update()) {
    case ButtonController::Event::Click:
      state = app_state::SEL_CH;
      disp_redraw = true;
      break;
    default:
      break;
  }
  switch (btn_e.update()) {
    case ButtonController::Event::Click:
      {
        Serial.println("I: delete preset");
        SI5351_Controller::Config cfg;
        int addr = EEPROM_PRESET_BASE + preset_sel * sizeof(cfg);
        EEPROM.get(addr, cfg);
        cfg.checksum ^= 0xFF; // inverting checksum invalidates entry
        EEPROM.put(addr, cfg);
      }
      state = app_state::SEL_CH;
      disp_redraw = true;
      break;
    default:
      break;
  }
  switch (btn_enc.update()) {
    case ButtonController::Event::Click:
      {
        Serial.println("I: save preset");
        auto cfg = clock_ctrl.get_config();
        int addr = EEPROM_PRESET_BASE + preset_sel * sizeof(cfg);
        EEPROM.put(addr, cfg);
        state = app_state::SEL_CH;
        disp_redraw = true;
      }
      break;
    default:
      break;
  }
  switch (btn_n.update()) {
    case ButtonController::Event::Click:
      {
        Serial.println("I: load preset");
        SI5351_Controller::Config cfg;
        int addr = EEPROM_PRESET_BASE + preset_sel * sizeof(cfg);
        EEPROM.get(addr, cfg);
        clock_ctrl.set_config(cfg);
        state = app_state::SEL_CH;
        disp_redraw = true;
      }
      break;
    default:
      break;
  }
}

void process_xtal_corr() {
  int32_t freq = clock_ctrl.get_xtal_freq();

  switch (renc.update()) {
    case RotaryEncoder::Event::CW:
      freq += ipow(10, digit_sel);
      freq = min(30000000, freq);
      clock_ctrl.set_xtal_freq(freq);
      disp_redraw = true;
      break;
    case RotaryEncoder::Event::CCW:
      freq -= ipow(10, digit_sel);
      freq = max(20000000, freq);
      clock_ctrl.set_xtal_freq(freq);
      disp_redraw = true;
      break;
    default:
      break;
  }
  switch (btn_enc.update()) {
    case ButtonController::Event::Click:
      freq = (freq / ipow(10, digit_sel + 1)) * ipow(10, digit_sel + 1);
      clock_ctrl.set_xtal_freq(freq);
      disp_redraw = true;
      break;
    default:
      break;
  }
  switch (btn_s.update()) {
    case ButtonController::Event::Click:
      state = app_state::SEL_CH;
      disp_redraw = true;
      break;
    default:
      break;
  }
  switch (btn_e.update()) {
    case ButtonController::Event::Click:
      if (digit_sel == 0) digit_sel = 8;
      else digit_sel--;
      disp_redraw = true;
      break;
    default:
      break;
  }
  switch (btn_w.update()) {
    case ButtonController::Event::Click:
      digit_sel++;
      if (digit_sel > 8) digit_sel = 0;
      disp_redraw = true;
      break;
    default:
      break;
  }
  switch (btn_n.update()) {
    case ButtonController::Event::Click:
      Serial.println("I: save xtal correction");
      EEPROM.put(EEPROM_XTAL_CORR_BASE, freq);
      state = app_state::SEL_CH;
      disp_redraw = true;
      break;
    default:
      break;
  }
}

void process_app_info() {
  switch (btn_s.update()) {
    case ButtonController::Event::Click:
      state = app_state::SEL_CH;
      disp_redraw = true;
      break;
    default:
      break;
  }
}

void loop() {
  switch (state) {
    case app_state::SEL_CH:
      process_sel_ch();
      break;
    case app_state::ADJ_FREQ:
      process_adj_freq();
      break;
    case app_state::PRESET:
      process_preset();
      break;
    case app_state::XTAL_CORR:
      process_xtal_corr();
      break;
    case app_state::APP_INFO:
      process_app_info();
      break;
    default:
      break;
  }

  if (disp_redraw) {
    switch (state) {
      case app_state::SEL_CH:
        ui_draw_ch(0, 0, 0, ch_sel == 0);
        ui_draw_ch(0, 32, 1, ch_sel == 1);
        ui_draw_ch(0, 64, 2, ch_sel == 2);
        break;
      case app_state::ADJ_FREQ:
        if (ch_sel == 0) ui_draw_ch(0, 0, 0, true, digit_sel);
        else tft.fillRect(0, 0, 128, 32, ST77XX_BLACK);
        if (ch_sel == 1) ui_draw_ch(0, 32, 1, true, digit_sel);
        else tft.fillRect(0, 32, 128, 32, ST77XX_BLACK);
        if (ch_sel == 2) ui_draw_ch(0, 64, 2, true, digit_sel);
        else tft.fillRect(0, 64, 128, 32, ST77XX_BLACK);
        break;
      case app_state::PRESET:
        ui_draw_presets(0, 0, preset_sel);
        break;
      case app_state::XTAL_CORR:
        ui_draw_freq_corr(0, 0, digit_sel);
        tft.fillRect(0, 32, 128, 96, ST77XX_BLACK);
        break;
      case app_state::APP_INFO:
        ui_draw_app_info(0, 0);
        break;
      default:
        break;
    }
    ui_draw_info_panel(0, 96);
    disp_redraw = false;
  }

  delay(10);
}

// NOTE: change PCINT0-2 based on pins used for encoder output A
ISR(PCINT1_vect) {
  renc.callback();
}
