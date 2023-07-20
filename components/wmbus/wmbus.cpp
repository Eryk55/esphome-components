#include "wmbus.h"
#include "version.h"

namespace esphome {
namespace wmbus {

static const char *TAG = "wmbus";

WMBusComponent::WMBusComponent() {}
WMBusComponent::~WMBusComponent() {}

void WMBusComponent::setup() {
  this->high_freq_.start();
  if (this->led_pin_ != nullptr) {
    this->led_pin_->setup();
    this->led_pin_->digital_write(false);
    led_on_ = false;
  }
  memset(this->mb_packet_, 0, sizeof(this->mb_packet_));

  this->add_driver(new Elf());
  this->add_driver(new Izar());
  this->add_driver(new Qheat());
  this->add_driver(new Itron());
  this->add_driver(new Evo868());
  this->add_driver(new Qwater());
  this->add_driver(new Amiplus());
  this->add_driver(new Bmeters());
  this->add_driver(new Vario451());
  this->add_driver(new Unismart());
  this->add_driver(new Ultrimis());
  this->add_driver(new Apator08());
  this->add_driver(new Mkradio3());
  this->add_driver(new Mkradio4());
  this->add_driver(new Apator162());
  this->add_driver(new ApatorEITN());
  this->add_driver(new Hydrocalm3());
  this->add_driver(new FhkvdataIII());
  ESP_LOGI(TAG, "CC1101 initialization OK. Waiting for telegrams...");
}

void WMBusComponent::update() {
  this->led_handler();
  if (true) {
    std::vector<unsigned char> frame;
    if (this->meter_telegram_) {
      frame = {0x66 ,0x44 ,0x49 ,0x6A ,0x40 ,0x88 ,0x22 ,0x55 ,0x18 ,0x37 ,0x72 ,0x53 ,0x25 ,0x14 ,0x25 ,0x9C ,0x80 ,0x0A ,0xD3 ,0x4E ,0x86 ,0x5F ,0x1D ,0x3B ,0x27 ,0xDA ,0xB2 ,0x51 ,0x80 ,0x20 ,0x9B ,0x5E ,0x5F ,0xF8 ,0xC3 ,0x3D ,0x88 ,0xAC ,0x6F ,0x2E ,0x96 ,0xB4 ,0x60 ,0x85 ,0x43 ,0xCC ,0x85 ,0x21 ,0x99 ,0x2F ,0x77 ,0x67 ,0x26 ,0x1B ,0xA8 ,0xB0 ,0x70 ,0xE0 ,0xE7 ,0x6A ,0x36 ,0x6F ,0x97 ,0x60 ,0xCD ,0xBB ,0xCB ,0xBF ,0x23 ,0xDD ,0x01 ,0xC8 ,0x81 ,0x25 ,0xAD ,0xD3 ,0xDA ,0xCE ,0x62 ,0x1A ,0xA5 ,0x56 ,0x4E ,0x83 ,0x84 ,0x5C ,0x6E ,0xFD ,0x92 ,0xCC ,0x20 ,0x48 ,0xA7 ,0xEB ,0x83 ,0x3D ,0xB1 ,0x8F ,0x82 ,0x0D ,0x4D ,0x02 ,0xF5};
    }
    else {
      frame = {0x66 ,0x44 ,0x49 ,0x6A ,0x40 ,0x88 ,0x22 ,0x55 ,0x18 ,0x37 ,0x72 ,0x53 ,0x25 ,0x14 ,0x25 ,0x49 ,0x6A ,0x01 ,0x06 ,0xC8 ,0x00 ,0x50 ,0x05 ,0x0C ,0x35 ,0xDD ,0x5B ,0xDA ,0xBA ,0xD4 ,0x1B ,0xE7 ,0x60 ,0xD0 ,0x33 ,0xEE ,0xB4 ,0xB8 ,0x67 ,0xEB ,0x55 ,0x44 ,0x30 ,0x5D ,0x97 ,0x15 ,0x3E ,0x03 ,0x14 ,0x30 ,0xAB ,0x2A ,0x7D ,0x7B ,0xC7 ,0x77 ,0x12 ,0xC4 ,0x37 ,0xEE ,0x6C ,0x53 ,0x0C ,0x60 ,0xCD ,0xBB ,0xCB ,0xBF ,0x23 ,0xDD ,0x01 ,0xC8 ,0x81 ,0x25 ,0xAD ,0xD3 ,0xDA ,0xCE ,0x62 ,0x1A ,0xA5 ,0x56 ,0x4E ,0x83 ,0x84 ,0x5C ,0x6E ,0xFD ,0x92 ,0xCC ,0x20 ,0x48 ,0xA7 ,0xEB ,0x83 ,0x3D ,0xB1 ,0x8F ,0x82 ,0x0D ,0x4D ,0x02 ,0xF5};
    }
    this->meter_telegram_ = !(this->meter_telegram_);

    std::string telegram = format_hex_pretty(frame);
    telegram.erase(std::remove(telegram.begin(), telegram.end(), '.'), telegram.end());

    // ToDo: add check for manufactures
    uint32_t meter_id = ((uint32_t)frame[7] << 24) | ((uint32_t)frame[6] << 16) |
                        ((uint32_t)frame[5] << 8)  | ((uint32_t)frame[4]);

    ESP_LOGI(TAG, "Meter ID [0x%08X] T: %s", meter_id, telegram.c_str());
    this->led_blink();
    
    if (this->wmbus_listeners_.count(meter_id) > 0) {
      auto *sensor = this->wmbus_listeners_[meter_id];
      auto selected_driver = this->drivers_[sensor->type];
      ESP_LOGI(TAG, "Using driver '%s' for ID [0x%08X] T: %s", selected_driver->get_name().c_str(), meter_id, telegram.c_str());
      float value{0};
      if (sensor->key.size()) {
        if (this->decrypt_telegram(frame, sensor->key)) {
          std::string decrypted_telegram = format_hex_pretty(frame);
          decrypted_telegram.erase(std::remove(decrypted_telegram.begin(), decrypted_telegram.end(), '.'), decrypted_telegram.end());
          ESP_LOGD(TAG, "Decrypted T : %s", decrypted_telegram.c_str());
        }
        else {
          std::string decrypted_telegram = format_hex_pretty(frame);
          decrypted_telegram.erase(std::remove(decrypted_telegram.begin(), decrypted_telegram.end(), '.'), decrypted_telegram.end());
          std::string key = format_hex_pretty(sensor->key);
          key.erase(std::remove(key.begin(), key.end(), '.'), key.end());
          if (key.size()) {
            key.erase(key.size() - 5);
          }
          ESP_LOGE(TAG, "Something was not OK during decrypting telegram for ID [0x%08X] '%s' key: '%s'", meter_id, selected_driver->get_name().c_str(), key.c_str());
          ESP_LOGE(TAG, "T : %s", telegram.c_str());
          ESP_LOGE(TAG, "T': %s", decrypted_telegram.c_str());
        }
      }
      if (selected_driver->get_value(frame, value)) {
        ESP_LOGI(TAG, "Value from telegram [0x%08X]: %f", meter_id, value);
      }
      else {
        ESP_LOGE(TAG, "Can't get value from telegram for ID [0x%08X] '%s'", meter_id, selected_driver->get_name().c_str());
      }
    }
  }
}

bool WMBusComponent::decrypt_telegram(std::vector<unsigned char> &telegram, std::vector<unsigned char> &key, int offset) {
  bool ret_val = false;
  std::vector<unsigned char>::iterator pos;
  // CI
  pos = telegram.begin() + 10;
  ESP_LOGD(TAG, "  CI 0x%02X", *pos);
  // data offset
  if ((*pos == 0x67) || (*pos == 0x6E) || (*pos == 0x74) || (*pos == 0x7A) || (*pos == 0x7D) || (*pos == 0x7F) || (*pos == 0x9E)) {
    ESP_LOGD(TAG, "  CI short");
    offset = 15;
  }
  else if ((*pos == 0x68) || (*pos == 0x6F) || (*pos == 0x72) || (*pos == 0x75) || (*pos == 0x7C) || (*pos == 0x7E) || (*pos == 0x9F)) {
    ESP_LOGD(TAG, "  CI long");
    uint32_t meter_id = ((uint32_t)telegram[7] << 24) | ((uint32_t)telegram[6] << 16) |
                        ((uint32_t)telegram[5] << 8)  | ((uint32_t)telegram[4]);
    uint32_t tpl_id = ((uint32_t)telegram[14] << 24) | ((uint32_t)telegram[13] << 16) |
                        ((uint32_t)telegram[12] << 8)  | ((uint32_t)telegram[11]);
    ESP_LOGI(TAG, "Meter ID [0x%08X] [0x%08X]", meter_id, tpl_id);
    offset = 23;
  }
  else {
    ESP_LOGD(TAG, "  CI unknown");
    offset = 15;
  }
  
  unsigned char iv[16];
  int i=0;
  for (int j=0; j<8; ++j) {
    iv[i++] = telegram[2+j];
  }
  for (int j=0; j<8; ++j) {
    iv[i++] = telegram[11];
  }
  pos = telegram.begin() + offset;
  int num_encrypted_bytes = 0;
  int num_not_encrypted_at_end = 0;

for (int iii=0; iii <16, iii++) {
  ESP_LOGD(TAG, "      0x%02X", iv[iii);
}

  if (decrypt_TPL_AES_CBC_IV(telegram, pos, key, iv,
                            &num_encrypted_bytes, &num_not_encrypted_at_end)) {
    uint32_t decrypt_check = 0x2F2F;
    uint32_t dc = (((uint32_t)telegram[offset] << 8) | ((uint32_t)telegram[offset+1]));
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
    if (!led_on_) {
      this->led_on_millis_ = millis();
      this->led_pin_->digital_write(true);
      led_on_ = true;
    }
  }
}

void WMBusComponent::led_handler() {
  if (this->led_pin_ != nullptr) {
    if (led_on_) {
      if ((millis() - this->led_on_millis_) >= this->led_blink_time_) {
        this->led_pin_->digital_write(false);
        led_on_ = false;
      }
    }
  }
}


void WMBusComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "wM-Bus v%s: [init]", MY_VERSION);
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
    ESP_LOGCONFIG(TAG, "  Serial output only.");
  }
  else {
    ESP_LOGCONFIG(TAG, "  Check connection to CC1101!");
  }
}

}  // namespace wmbus
}  // namespace esphome
