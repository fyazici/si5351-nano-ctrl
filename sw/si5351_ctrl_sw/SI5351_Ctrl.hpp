#pragma once

#include <Wire.h>

namespace {

constexpr uint8_t SI5351_ADDRESS = 0x60;

// register addresses
constexpr uint8_t CLK_OE_CONTROL_REG = 3;
constexpr uint8_t CLK_OE_CONTROL_CLK0_OEB = 0b001;
constexpr uint8_t CLK_OE_CONTROL_CLK1_OEB = 0b010;
constexpr uint8_t CLK_OE_CONTROL_CLK2_OEB = 0b100;

constexpr uint8_t CLK0_CONTROL_REG = 16;
constexpr uint8_t CLK1_CONTROL_REG = 17;
constexpr uint8_t CLK2_CONTROL_REG = 18;
constexpr uint8_t CLKx_CONTROL_IDRV_2ma = 0b00000000;
constexpr uint8_t CLKx_CONTROL_IDRV_4ma = 0b00000001;
constexpr uint8_t CLKx_CONTROL_IDRV_6ma = 0b00000010;
constexpr uint8_t CLKx_CONTROL_IDRV_8ma = 0b00000011;
constexpr uint8_t CLKx_CONTROL_CLKx_SRC_MS = 0b00001100;
constexpr uint8_t CLKx_CONTROL_MSx_SRC_PLLA = 0b00000000;
constexpr uint8_t CLKx_CONTROL_MSx_SRC_PLLB = 0b00100000;
constexpr uint8_t CLKx_CONTROL_MSx_INT_MODE = 0b01000000;
constexpr uint8_t CLKx_CONTROL_PDN = 0b10000000;

constexpr uint8_t SYNTH_PLL_A = 26;
constexpr uint8_t SYNTH_PLL_B = 34;

constexpr uint8_t SYNTH_MS0_BASE = 42;
constexpr uint8_t SYNTH_MS1_BASE = 50;
constexpr uint8_t SYNTH_MS2_BASE = 58;
constexpr uint8_t SYNTH_MSx_DIVBY4 = 0b00001100;

constexpr uint8_t PLL_RESET = 177;
constexpr uint8_t XTAL_LOAD_CAP = 183;

constexpr uint32_t XTAL_FREQ = 25000000;
constexpr uint32_t MIN_VCO_FREQ = 600000000;
constexpr uint32_t MAX_VCO_FREQ = 900000000;

class SI5351_Controller {
public:
  struct Config {
    uint8_t oeb;
    uint8_t control[3];
    int32_t freq[3];
    uint8_t checksum;

    void dump() const {
      Serial.println("Config {");
      Serial.print("  oeb="); Serial.println(oeb);
      Serial.print("  control="); Serial.print(control[0]); Serial.print(","); Serial.print(control[1]); Serial.print(","); Serial.println(control[2]);
      Serial.print("  freq="); Serial.print(freq[0]); Serial.print(","); Serial.print(freq[1]); Serial.print(","); Serial.println(freq[2]);
      Serial.print("  checksum="); Serial.println(checksum);
      Serial.println("}\n");
    }
  };

  void set_ch_en(uint8_t ch, bool en) {
    if (en) {
      cfg.oeb &= ~(0x1 << ch);
      set_ch_control(ch, get_ch_control(ch) & (~CLKx_CONTROL_PDN));
    } else {
      cfg.oeb |= (0x1 << ch);
      set_ch_control(ch, get_ch_control(ch) | CLKx_CONTROL_PDN);
    }
    si5351_set_oec(cfg.oeb);
  }

  bool get_ch_en(uint8_t ch) const {
    return !(cfg.oeb & (0x1 << ch));
  }

  void set_ch_drive_strength(uint8_t ch, uint8_t mode) {
    set_ch_control(ch, (get_ch_control(ch) & 0xFC) | (mode & 0x3));
  }

  uint8_t get_ch_drive_strength(uint8_t ch) const {
    return (get_ch_control(ch) & 0x3);
  }

  void set_ch_control(uint8_t ch, uint8_t value) {
    uint8_t cc = (ch == 0) ? CLK0_CONTROL_REG : ((ch == 1) ? CLK1_CONTROL_REG : CLK2_CONTROL_REG);
    cfg.control[ch] = value;
    si5351_write(cc, value);
  }

  uint8_t get_ch_control(uint8_t ch) const {
    return cfg.control[ch];
  }

  void set_ch_freq(uint8_t ch, int32_t freq, bool forced = false) {
    uint8_t fb = (ch == 0) ? SYNTH_PLL_A : SYNTH_PLL_B;
    uint8_t ms = (ch == 0) ? SYNTH_MS0_BASE : ((ch == 1) ? SYNTH_MS1_BASE : SYNTH_MS2_BASE);

    // if f > 150M, must use /4
    if (freq > 150000000) {
      if (freq > 225000000) {
        Serial.println("W: PLL overclock mode");
      }
      si5351_ms_set_freq(fb, freq * 4, dev_xtal_freq, 0);
      set_ch_control(ch, get_ch_control(ch) | CLKx_CONTROL_MSx_INT_MODE);
      si5351_ms_set_par(ms, 4, 0, 1, 0, SYNTH_MSx_DIVBY4);
    }
    // if f in [112.5M, 150M), must use /6
    else if (freq > 112500000) {
      si5351_ms_set_freq(fb, freq * 6, dev_xtal_freq, 0);
      set_ch_control(ch, get_ch_control(ch) & (~CLKx_CONTROL_MSx_INT_MODE));
      si5351_ms_set_par(ms, 6, 0, 1, 0, 0);
    }
    // if f in [500k, 112.5M), fix PLL to 900M
    else if (freq >= 500000) {
      si5351_ms_set_freq(fb, MAX_VCO_FREQ, dev_xtal_freq, 0);
      set_ch_control(ch, get_ch_control(ch) & (~CLKx_CONTROL_MSx_INT_MODE));
      si5351_ms_set_freq(ms, MAX_VCO_FREQ, freq, 0);
    }
    // if f < 500k, must use R dividers
    else {
      si5351_ms_set_freq(fb, MIN_VCO_FREQ, dev_xtal_freq, 0);
      set_ch_control(ch, get_ch_control(ch) & (~CLKx_CONTROL_MSx_INT_MODE));
      si5351_ms_set_freq(ms, MIN_VCO_FREQ, freq * 128, 7);
    }

    // if ch is set directly (not forced) and on pllb
    if ((!forced) && ((ch == 1) || (ch == 2))) {
      // if new freq > 112.5M or previous freq was < 112.5M
      if ((freq > 112500000) || (cfg.freq[ch] > 112500000)) {
        // reflect the update to forced channel (1->2, 2->1)
        set_ch_freq(3 - ch, freq, true);
      }
    }

    // reset PLL to ensure settings applied
    if (fb == SYNTH_PLL_A) {
      si5351_write(PLL_RESET, 0x20);
    } else {
      si5351_write(PLL_RESET, 0x80);
    }

    cfg.freq[ch] = freq;
  }

  int32_t get_ch_freq(uint8_t ch) const {
    return cfg.freq[ch];
  }

  char get_ch_pll_dsn(uint8_t ch) const {
    return (ch == 0) ? 'A' : 'B';
  }

  char get_ch_pll_mode(uint8_t ch) const {
    if (cfg.freq[ch] > 225000000) return '!';
    if (cfg.freq[ch] > 150000000) return '4';
    if (cfg.freq[ch] > 112500000) return '6';
    return '%';
  }

  char get_ch_drive_dsn(uint8_t ch) const {
    uint8_t mode = get_ch_drive_strength(ch);
    if (mode == CLKx_CONTROL_IDRV_2ma) return '2';
    if (mode == CLKx_CONTROL_IDRV_4ma) return '4';
    if (mode == CLKx_CONTROL_IDRV_6ma) return '6';
    if (mode == CLKx_CONTROL_IDRV_8ma) return '8';
    return "?";
  }

  uint32_t get_xtal_freq() const {
    return dev_xtal_freq;
  }

  uint32_t set_xtal_freq(uint32_t freq) {
    dev_xtal_freq = freq;
    Serial.println("I: recompute");
    Serial.println(freq);
    set_ch_freq(0, cfg.freq[0]);
    set_ch_freq(1, cfg.freq[1]);
    set_ch_freq(2, cfg.freq[2]);
  }

  const Config &get_config() {
    // update checksum
    const uint8_t *p = (uint8_t *)(&cfg);
    const uint8_t * const end = p + sizeof(cfg) - 1;
    uint8_t checksum = 0x5A;
    for (; p < end; ++p) {
      checksum ^= *p;
    }
    cfg.checksum = checksum;
    return cfg;
  }

  bool set_config(const Config &new_cfg) {
    // control checksum
    const uint8_t *p = (uint8_t *)(&new_cfg);
    const uint8_t * const end = p + sizeof(new_cfg);
    uint8_t checksum = 0x5A;
    for (; p < end; ++p) {
      checksum ^= *p;
    }
    if (checksum) {
      Serial.print("preset checksum error ");
      Serial.println(checksum);
      return false;
    }

    set_ch_en(0, !(new_cfg.oeb & 0x1));
    set_ch_en(1, !(new_cfg.oeb & 0x2));
    set_ch_en(2, !(new_cfg.oeb & 0x4));

    set_ch_control(0, new_cfg.control[0]);
    set_ch_control(1, new_cfg.control[1]);
    set_ch_control(2, new_cfg.control[2]);

    set_ch_freq(0, new_cfg.freq[0]);
    set_ch_freq(1, new_cfg.freq[1]);
    set_ch_freq(2, new_cfg.freq[2]);

    return true;
  }

  void set_default() {
    set_ch_control(0, CLKx_CONTROL_IDRV_8ma | CLKx_CONTROL_CLKx_SRC_MS | CLKx_CONTROL_MSx_SRC_PLLA);
    set_ch_control(1, CLKx_CONTROL_IDRV_8ma | CLKx_CONTROL_CLKx_SRC_MS | CLKx_CONTROL_MSx_SRC_PLLB);
    set_ch_control(2, CLKx_CONTROL_IDRV_8ma | CLKx_CONTROL_CLKx_SRC_MS | CLKx_CONTROL_MSx_SRC_PLLB);

    set_ch_freq(0, 1000000);
    set_ch_freq(1, 10000000);
    set_ch_freq(2, 100000000);

    set_ch_en(0, 0);
    set_ch_en(1, 0);
    set_ch_en(2, 0);
  }

  void begin(TwoWire *wire_ = &Wire, uint8_t addr = SI5351_ADDRESS, uint32_t xtal = XTAL_FREQ) {
    the_wire = wire_;
    dev_address = addr;
    dev_xtal_freq = xtal;

    si5351_start();
  }

  void begin(uint32_t xtal) {
    begin(&Wire, SI5351_ADDRESS, xtal);
  }

  void __raw_write_reg(uint8_t addr, uint8_t value) {
    si5351_write(addr, value);
  }

  uint8_t __raw_read_reg(uint8_t addr) {
    return si5351_read(addr);
  }

private:
  TwoWire *the_wire;
  uint8_t dev_address;
  uint32_t dev_xtal_freq;
  Config cfg;

  ////////////////////////////////
  //
  // Si5351A commands
  //
  ///////////////////////////////
  void si5351_write(uint8_t reg_addr, uint8_t reg_value) {
    the_wire->beginTransmission(dev_address);
    the_wire->write(reg_addr);
    the_wire->write(reg_value);
    the_wire->endTransmission();
  }

  uint8_t si5351_read(uint8_t reg_addr) {
    uint8_t r;
    the_wire->beginTransmission(dev_address);
    the_wire->write(reg_addr);
    the_wire->endTransmission();
    
    the_wire->requestFrom(dev_address, 1);
    return the_wire->read();
  }

  void si5351_start(void) {
    // Init clock chip
    si5351_write(XTAL_LOAD_CAP, 0xD2);     // Set crystal load capacitor to 6pF,
                                           // for bits 5:0 see also AN619 p. 60
    si5351_write(CLK_OE_CONTROL_REG, 0xFF);   // Disable all outputs
    si5351_write(CLK0_CONTROL_REG, 0x0C);  // Set PLLA to CLK0, 2 mA output
    si5351_write(CLK1_CONTROL_REG, 0x2C);  // Set PLLB to CLK1, 2 mA output
    si5351_write(CLK2_CONTROL_REG, 0x2C);  // Set PLLB to CLK2, 2 mA output
    si5351_write(PLL_RESET, 0xA0);         // Reset PLLA and PLLB
  }

  void si5351_set_oec(uint8_t oeb) {
    si5351_write(CLK_OE_CONTROL_REG, oeb);
  }

  void si5351_ms_set_par(int synth, uint32_t a, uint32_t b, uint32_t c, uint8_t log_rdiv, uint8_t divby4) {
    uint32_t p1, p2, p3;

    if (divby4) {
      p1 = 0;
      p2 = 0;
      p3 = 1;
    } else {
      p1 = 128 * a + (uint32_t)(128 * b / c) - 512;
      p2 = 128 * b - c * (uint32_t)(128 * b / c);
      p3 = c;
    }

    Serial.print("I: synth=");
    Serial.print(synth);
    Serial.print(", a=");
    Serial.print(a);
    Serial.print(", b=");
    Serial.print(b);
    Serial.print(", c=");
    Serial.print(c);
    Serial.print(", p1=");
    Serial.print(p1);
    Serial.print(", p2=");
    Serial.print(p2);
    Serial.print(", p3=");
    Serial.print(p3);
    Serial.print(", log(r)=");
    Serial.print(log_rdiv);
    Serial.print(", divby4=");
    Serial.println(divby4);

    //Write data to multisynth registers of synth n
    si5351_write(synth, (p3 & 0x0000FF00) >> 8);
    si5351_write(synth + 1, (p3 & 0x000000FF));
    si5351_write(synth + 2, (log_rdiv << 4) | divby4 | ((p1 & 0x00030000) >> 16));
    si5351_write(synth + 3, (p1 & 0x0000FF00) >> 8);
    si5351_write(synth + 4, (p1 & 0x000000FF));
    si5351_write(synth + 5, ((p3 & 0x000F0000) >> 12) | ((p2 & 0x000F0000) >> 16));
    si5351_write(synth + 6, (p2 & 0x0000FF00) >> 8);
    si5351_write(synth + 7, (p2 & 0x000000FF));
  }

  void si5351_ms_set_freq(int synth, uint32_t fpll, uint32_t freq, uint8_t log_rdiv) {
    uint32_t a, b, c = 1000000;
    double fdiv = (double)fpll / freq;  //division factor fvco/freq (will be integer part of a+b/c)
    double rm;                          //remainder

    a = (uint32_t)fdiv;
    rm = fdiv - a;  //(equiv. b/c)
    b = rm * c;

    si5351_ms_set_par(synth, a, b, c, log_rdiv, 0);
  }
};
}
