#include "wmbus.h"
#include "version.h"

namespace esphome {
namespace wmbus {

static const char *TAG = "wmbus";

void WMBusComponent::setup() {
  this->high_freq_.start();
  if (this->led_pin_ != nullptr) {
    this->led_pin_->setup();
    this->led_pin_->digital_write(false);
    this->led_on_ = false;
  }
  if (!rf_mbus_.init(this->spi_conf_.mosi->get_pin(), this->spi_conf_.miso->get_pin(),
                     this->spi_conf_.clk->get_pin(), this->spi_conf_.cs->get_pin(),
                     this->spi_conf_.gdo0->get_pin(), this->spi_conf_.gdo2->get_pin())) {
    this->mark_failed();
    ESP_LOGE(TAG, "CC1101 initialization failed.");
    return;
  }

  this->add_driver(new Izar());
}

void WMBusComponent::loop() {
  this->led_handler();
  if (rf_mbus_.task()) {
    WMbusFrame mbus_data = rf_mbus_.get_frame();
    std::vector<unsigned char> frame = mbus_data.frame;
    std::string telegram = format_hex_pretty(frame);
    telegram.erase(std::remove(telegram.begin(), telegram.end(), '.'), telegram.end());

    // ToDo: add manufactures check
    uint32_t meter_id = ((uint32_t)frame[7] << 24) | ((uint32_t)frame[6] << 16) |
                        ((uint32_t)frame[5] << 8)  | ((uint32_t)frame[4]);

    if (this->wmbus_listeners_.count(meter_id) > 0) {
      auto *sensor = this->wmbus_listeners_[meter_id];
      auto selected_driver = this->drivers_[sensor->type];
      ESP_LOGI(TAG, "Using driver '%s' for ID [0x%08X] RSSI: %d dBm LQI: %d Mode: %s T: %s",
               selected_driver->get_name().c_str(),
               meter_id,
               mbus_data.rssi,
               mbus_data.lqi,
               mode_to_string(mbus_data.framemode).c_str(),
               telegram.c_str());
      if (sensor->key.size()) {
        if (this->decrypt_telegram(frame, sensor->key)) {
          std::string decrypted_telegram = format_hex_pretty(frame);
          decrypted_telegram.erase(std::remove(decrypted_telegram.begin(), decrypted_telegram.end(), '.'),
                                   decrypted_telegram.end());
          ESP_LOGD(TAG, "Decrypted T : %s", decrypted_telegram.c_str());
        }
        else {
          std::string decrypted_telegram = format_hex_pretty(frame);
          decrypted_telegram.erase(std::remove(decrypted_telegram.begin(), decrypted_telegram.end(), '.'),
                                   decrypted_telegram.end());
          std::string key = format_hex_pretty(sensor->key);
          key.erase(std::remove(key.begin(), key.end(), '.'), key.end());
          if (key.size()) {
            key.erase(key.size() - 5);
          }
          ESP_LOGE(TAG, "Something was not OK during decrypting telegram for ID [0x%08X] '%s' key: '%s'",
                   meter_id,
                   selected_driver->get_name().c_str(),
                   key.c_str());
          ESP_LOGE(TAG, "T : %s", telegram.c_str());
          ESP_LOGE(TAG, "T': %s", decrypted_telegram.c_str());
        }
      }
      auto mapValues = selected_driver->get_values(frame);
      if (mapValues.has_value()) {
        if (this->wmbus_listeners_[meter_id]->sensors_.count("lqi") > 0) {
          this->wmbus_listeners_[meter_id]->sensors_["lqi"]->publish_state(mbus_data.lqi);
        }
        if (this->wmbus_listeners_[meter_id]->sensors_.count("rssi") > 0) {
          this->wmbus_listeners_[meter_id]->sensors_["rssi"]->publish_state(mbus_data.rssi);
        }
        for (const auto &ele : mapValues.value()) {
          if (this->wmbus_listeners_[meter_id]->sensors_.count(ele.first) > 0) {
            this->wmbus_listeners_[meter_id]->sensors_[ele.first]->publish_state(ele.second);
          }
        }
        this->led_blink();
      }
      else {
        std::string not_ok_telegram = format_hex_pretty(frame);
        not_ok_telegram.erase(std::remove(not_ok_telegram.begin(), not_ok_telegram.end(), '.'),
                              not_ok_telegram.end());
        ESP_LOGE(TAG, "Can't get value from telegram for ID [0x%08X] '%s'",
                 meter_id, 
                 selected_driver->get_name().c_str());
        ESP_LOGE(TAG, "T : %s", not_ok_telegram.c_str());
      }
    }
    else {
      if (this->wmbus_listeners_.count(0) > 0) {
        if (this->wmbus_listeners_[0]->sensors_.count("lqi") > 0) {
          this->wmbus_listeners_[0]->sensors_["lqi"]->publish_state(mbus_data.lqi);
        }
        if (this->wmbus_listeners_[0]->sensors_.count("rssi") > 0) {
          this->wmbus_listeners_[0]->sensors_["rssi"]->publish_state(mbus_data.rssi);
        }
      }
      if (this->log_unknown_) {
        ESP_LOGD(TAG, "Meter ID [0x%08X] RSSI: %d dBm LQI: %d Mode: %s not found in configuration T: %s",
                meter_id,
                mbus_data.rssi,
                mbus_data.lqi,
                mode_to_string(mbus_data.framemode).c_str(),
                telegram.c_str());
      }
    }
  }
}

bool WMBusComponent::decrypt_telegram(std::vector<unsigned char> &telegram, std::vector<unsigned char> &key) {
  bool ret_val = false;
  std::vector<unsigned char>::iterator pos;
  pos = telegram.begin();
  unsigned char iv[16];
  int i=0;
  for (int j=0; j<8; ++j) {
    iv[i++] = telegram[2+j];
  }
  for (int j=0; j<8; ++j) {
    iv[i++] = telegram[11];
  }
  pos = telegram.begin() + 15;
  int num_encrypted_bytes = 0;
  int num_not_encrypted_at_end = 0;

  if (decrypt_TPL_AES_CBC_IV(telegram, pos, key, iv,
                            &num_encrypted_bytes, &num_not_encrypted_at_end)) {
    uint32_t decrypt_check = 0x2F2F;
    uint32_t dc = (((uint32_t)telegram[15] << 8) | ((uint32_t)telegram[16]));
    if ( dc == decrypt_check) {
      ret_val = true;
    }
  }
  return ret_val;
}

void WMBusComponent::register_wmbus_listener(WMBusListener *listener) {
  this->wmbus_listeners_[listener->id] = listener;
}

void WMBusComponent::add_driver(Driver *driver) {
  this->drivers_[driver->get_name()] = driver;
}

void WMBusComponent::led_blink() {
  if (this->led_pin_ != nullptr) {
    if (!this->led_on_) {
      this->led_on_millis_ = millis();
      this->led_pin_->digital_write(true);
      this->led_on_ = true;
    }
  }
}

void WMBusComponent::led_handler() {
  if (this->led_pin_ != nullptr) {
    if (this->led_on_) {
      if ((millis() - this->led_on_millis_) >= this->led_blink_time_) {
        this->led_pin_->digital_write(false);
        this->led_on_ = false;
      }
    }
  }
}

void WMBusComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "wM-Bus v2 [init]");
  if (this->led_pin_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  LED:");
    LOG_PIN("    Pin: ", this->led_pin_);
    ESP_LOGCONFIG(TAG, "    Duration: %d ms", this->led_blink_time_);
  }
  ESP_LOGCONFIG(TAG, "  CC1101 SPI bus:");
  LOG_PIN("    MOSI Pin: ", this->spi_conf_.mosi);
  LOG_PIN("    MISO Pin: ", this->spi_conf_.miso);
  LOG_PIN("    CLK Pin:  ", this->spi_conf_.clk);
  LOG_PIN("    CS Pin:   ", this->spi_conf_.cs);
  LOG_PIN("    GDO0 Pin: ", this->spi_conf_.gdo0);
  LOG_PIN("    GDO2 Pin: ", this->spi_conf_.gdo2);
  if (this->drivers_.size() > 0) {
    ESP_LOGCONFIG(TAG, "  Log output only.");

  }
  else {
    ESP_LOGE(TAG, "  Check connection to CC1101!");
  }
}

///////////////////////////////////////

void WMBusListener::dump_config() {
  std::string key = format_hex_pretty(this->key);
  key.erase(std::remove(key.begin(), key.end(), '.'), key.end());
  if (key.size()) {
    key.erase(key.size() - 5);
  }
  ESP_LOGCONFIG(TAG, "  Meter:");
  ESP_LOGCONFIG(TAG, "    ID: %zu [0x%08X]", this->id, this->id);
  ESP_LOGCONFIG(TAG, "    Type: %s", this->type.c_str());
  ESP_LOGCONFIG(TAG, "    Key: '%s'", key.c_str());
  for (const auto &ele : this->sensors_) {
    LOG_SENSOR("    ", "Sensor", ele.second);
  }
}

WMBusListener::WMBusListener(const uint32_t id, const std::string type, const std::string key) {
  this->id = id;
  this->type = type;
  hex_to_bin(key, &(this->key));
}

int WMBusListener::char_to_int(char input)
{
  if(input >= '0' && input <= '9') {
    return input - '0';
  }
  if(input >= 'A' && input <= 'F') {
    return input - 'A' + 10;
  }
  if(input >= 'a' && input <= 'f') {
    return input - 'a' + 10;
  }
  return -1;
}

bool WMBusListener::hex_to_bin(const char* src, std::vector<unsigned char> *target)
{
  if (!src) return false;
  while(*src && src[1]) {
    if (*src == ' ' || *src == '#' || *src == '|' || *src == '_') {
      // Ignore space and hashes and pipes and underlines.
      src++;
    } 
    else {
      int hi = char_to_int(*src);
      int lo = char_to_int(src[1]);
      if (hi<0 || lo<0) return false;
      target->push_back(hi*16 + lo);
      src += 2;
    }
  }
  return true;
}

}  // namespace wmbus
}  // namespace esphome
