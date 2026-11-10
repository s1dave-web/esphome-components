/*
  Based on: https://github.com/wmbusmeters/wmbusmeters/blob/master/src/driver_izar.cc
  Copyright (C) 2019 Jacek Tomasiak (gpl-3.0-or-later)
  Copyright (C) 2020-2022 Fredrik Öhrström (gpl-3.0-or-later)
  Copyright (C) 2021 Vincent Privat (gpl-3.0-or-later)
*/

#pragma once

#include "driver.h"

#define GET_BIT(var, pos) ((var >> pos) & 0x01)

struct Izar: Driver
{
  Izar(std::string key = "") : Driver(std::string("izar"), key) {};

  // IZAR uses Diehl's LFSR scrambling, not AES: the meter key (if any) is used here (see decrypt()).
  bool decrypts_itself() override { return true; };
  virtual esphome::optional<std::map<std::string, double>> get_values(std::vector<unsigned char> &telegram) override {
    std::map<std::string, double> ret_val{};

    // Alarms, battery and transmit period are read from header bytes 11..13; ignore anything shorter
    if (telegram.size() < 15) {
      return {};
    }

    uint8_t decrypted[64] = {0};

    const uint8_t decoded_size = this->decrypt(reinterpret_cast<uint8_t*>(telegram.data()), telegram.size(), decrypted);
    if (decoded_size >= 5) {
      add_to_map(ret_val, "total_water_m3", this->get_total_water_m3(decrypted));
      // Last month is bytes 5..8: only there in a long enough payload (as wmbusmeters checks)
      if (decoded_size > 8) {
        add_to_map(ret_val, "last_month_total_water_m3", this->get_last_month_total_water_m3(decrypted));
        add_to_map(ret_val, "current_month_total_water_l", this->get_current_month_total_water_l(decrypted));
      }
    }

    // Date of the last-month reading, as a number YYMMDD (bytes 9..10 of the decoded payload).
    // Not YYYYMMDD: ESPHome sensor states are 32-bit floats, exact only up to 16777216, so an
    // 8-digit date gets rounded to an even number (20261031 became 20261032).
    if (decoded_size > 10) {
      add_to_map(ret_val, "last_month_measure_date", this->get_last_month_measure_date(decrypted));
    }

    // Serial number and manufacture year are coded in the address of SAP "PRIOS" frames only
    if (this->is_sap_prios(telegram)) {
      const uint32_t digits = ((uint32_t)(telegram[7] & 0x03) << 24) | ((uint32_t)telegram[6] << 16) |
                              ((uint32_t)telegram[5] << 8) | telegram[4];
      const uint32_t yy = (digits / 1000000) % 100;      // printed as 8 digits: YY + 6-digit serial
      add_to_map(ret_val, "serial_number", (double)(digits % 1000000));
      add_to_map(ret_val, "manufacture_year", (double)(yy > 70 ? 1900 + yy : 2000 + yy));
    }

    add_to_map(ret_val, "transmit_period_s", this->get_transmit_period_s(telegram));
    add_to_map(ret_val, "remaining_battery_life_y", this->get_remaining_battery_life_y(telegram));
    add_to_map(ret_val, "current_alarms", this->get_current_alarms(telegram));
    add_to_map(ret_val, "previous_alarms", this->get_previous_alarms(telegram));

    if (ret_val.size() > 0) {
      return ret_val;
    }
    else {
      return {};
    }
  };

private:
  // Same rule as wmbusmeters' detectDiehlFrameInterpretation() for SAP_PRIOS
  bool is_sap_prios(const std::vector<unsigned char> &telegram) {
    const uint8_t c_field = telegram[1];
    const uint16_t m_field = (telegram[3] << 8) | telegram[2];
    const uint8_t ci_field = telegram[10];
    return (c_field == 0x44 || c_field == 0x46) && m_field == 0x4C30 /* SAP */ &&
           ci_field >= 0xA0 && ci_field <= 0xA7;
  }

  esphome::optional<double> get_last_month_measure_date(uint8_t *decrypted) {
    // Two-digit year as the meter sends it (above 80 means 19xx, otherwise 20xx)
    const uint16_t year = ((decrypted[10] & 0xF0) >> 1) + ((decrypted[9] & 0xE0) >> 5);
    const uint8_t month = decrypted[10] & 0x0F;
    const uint8_t day = decrypted[9] & 0x1F;
    if (year > 99 || month < 1 || month > 12 || day < 1 || day > 31) {
      return {};
    }
    return (double)(year * 10000UL + month * 100UL + day);
  }

  esphome::optional<double> get_current_alarms(std::vector<unsigned char> &telegram) {
    esphome::optional<double> ret_val{};
    uint16_t alarms = 0;
    alarms |= (telegram[11] >> 7)       << 1; // general_alarm
    alarms |= (telegram[12] >> 7)       << 2; // leakage_currently
    alarms |= (telegram[12] >> 5 & 0x1) << 3; // meter_blocked
    alarms |= (telegram[13] >> 7)       << 4; // back_flow
    alarms |= (telegram[13] >> 6 & 0x1) << 5; // underflow
    alarms |= (telegram[13] >> 5 & 0x1) << 6; // overflow
    alarms |= (telegram[13] >> 4 & 0x1) << 7; // submarine
    alarms |= (telegram[13] >> 3 & 0x1) << 8; // sensor_fraud_currently
    alarms |= (telegram[13] >> 1 & 0x1) << 9; // mechanical_fraud_currently
    ret_val = (double)alarms;
    return ret_val;
  };

  esphome::optional<double> get_previous_alarms(std::vector<unsigned char> &telegram) {
    esphome::optional<double> ret_val{};
    uint16_t alarms = 0;
    alarms |= (telegram[12] >> 6 & 0x1) << 2; // leakage_previously
    alarms |= (telegram[13] >> 2 & 0x1) << 8; // sensor_fraud_previously
    alarms |= (telegram[13] & 0x1)      << 9; // mechanical_fraud_previously
    ret_val = (double)alarms;
    return ret_val;
  };

  esphome::optional<double> get_remaining_battery_life_y(std::vector<unsigned char> &telegram) {
    esphome::optional<double> ret_val{};
    ret_val = (telegram[12] & 0x1F) / 2.0;
    return ret_val;
  };

  esphome::optional<double> get_transmit_period_s(std::vector<unsigned char> &telegram) {
    esphome::optional<double> ret_val{};
    ret_val = 1 << ((telegram[11] & 0x0F) + 2);
    return ret_val;
  };

  esphome::optional<double> get_total_water_m3(uint8_t *decrypted) {
    esphome::optional<double> ret_val{};
    ret_val = (this->uintFromBytesLittleEndian(decrypted + 1)) / 1000.0;
    return ret_val;
  };

  esphome::optional<double> get_last_month_total_water_m3(uint8_t *decrypted) {
    esphome::optional<double> ret_val{};
    ret_val = (this->uintFromBytesLittleEndian(decrypted + 5)) / 1000.0;
    return ret_val;
  };

  esphome::optional<double> get_current_month_total_water_l(uint8_t *decrypted) {
    esphome::optional<double> ret_val{};
    ret_val = this->uintFromBytesLittleEndian(decrypted + 1) - this->uintFromBytesLittleEndian(decrypted + 5);
    return ret_val;
  };

  const uint8_t decoder[64] = {
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0x03,0xFF,0x01,0x02,0xFF,
    0xFF,0xFF,0xFF,0x07,0xFF,0xFF,0x00,0xFF,
    0xFF,0x05,0x06,0xFF,0x04,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0x0B,0xFF,0x09,0x0A,0xFF,
    0xFF,0x0F,0xFF,0xFF,0x08,0xFF,0xFF,0xFF,
    0xFF,0x0D,0x0E,0xFF,0x0C,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
  };

  uint32_t uintFromBytes(uint8_t* data) {
    uint32_t result = data[0] << 24;
    result += data[1] << 16;
    result += data[2] << 8;
    result += data[3];
    return result;
  }

  uint32_t uintFromBytesLittleEndian(uint8_t* data) {
    uint32_t result = data[3] << 24;
    result += data[2] << 16;
    result += data[1] << 8;
    result += data[0];
    return result;
  }

  uint32_t hashShiftKey(uint32_t key) {
    for (uint8_t i = 0; i < 8; i++) {
      uint8_t bit = GET_BIT(key, 1) ^ GET_BIT(key, 2) ^
                    GET_BIT(key, 11) ^ GET_BIT(key, 31);
      key <<= 1;
      key |= bit;
    }
    return key;
  }

  static bool telegram_too_long(size_t len) { return (len - 15) > 64; }

  // A configured key (8 or 16 bytes) becomes the LFSR seed as wmbusmeters' convertKey() does: first
  // 4 bytes XOR next 4. With no key, both Diehl default keys are tried (PRIOS_DEFAULT_KEY1 / KEY2).
  uint8_t decrypt(uint8_t* encoded, size_t len, uint8_t* decoded) {
    // Header is 15 bytes; the decoded payload must also fit the caller's 64-byte buffer
    if (len < 15 || telegram_too_long(len)) {
      return 0;
    }
    uint32_t seeds[2];
    uint8_t n_seeds = 0;
    if (this->meter_key_.size() >= 8) {
      seeds[n_seeds++] = uintFromBytes(this->meter_key_.data()) ^ uintFromBytes(this->meter_key_.data() + 4);
    } else {
      seeds[n_seeds++] = 0xdfd109e8;  // 39BC8A10 ^ E66D83F8
      seeds[n_seeds++] = 0xb71f0ae8;  // 51728910 ^ E66D83F8
    }
    for (uint8_t k = 0; k < n_seeds; k++) {
      const uint8_t size = this->decrypt_with_seed(encoded, len, decoded, seeds[k]);
      if (size > 0) {
        return size;
      }
    }
    return 0;
  }

  uint8_t decrypt_with_seed(uint8_t* encoded, size_t len, uint8_t* decoded, uint32_t seed) {
    uint32_t key = seed;
    key ^= uintFromBytes(encoded + 2);
    key ^= uintFromBytes(encoded + 6);
    key ^= uintFromBytes(encoded + 10);

    const uint8_t size = len - 15;
    for (uint8_t i = 0; i < size; i++) {
      key = hashShiftKey(key);
      decoded[i] = encoded[i + 15] ^ (key & 0xFF);
    }

    if (decoded[0] != 0x4B) {
      return 0;
    }

    return size;
  }
};