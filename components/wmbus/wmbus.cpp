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
      frame = {0x6e ,0x44 ,0x01 ,0x06 ,0x54 ,0x90 ,0x28 ,0x14 ,0x05 ,0x07 ,0x7a ,0x79 ,0x00 ,0x60 ,0x85 ,0x6e ,0x8B ,0x13 ,0xf2 ,0xcB ,0x68 ,0x37 ,0xe8 ,0xd9 ,0xf8 ,0xa0 ,0xB0 ,0xdc ,0x8B ,0xfc ,0xf7 ,0x36 ,0x18 ,0x2B ,0x15 ,0xB2 ,0x7f ,0xdB ,0xa3 ,0xfd ,0x9f ,0xde ,0x56 ,0x37 ,0x4a ,0xdf ,0x94 ,0x23 ,0x5c ,0x01 ,0xc8 ,0x54 ,0x01 ,0x04 ,0xf7 ,0x3c ,0x5e ,0x61 ,0x7a ,0x1e ,0x3f ,0xBf ,0xe5 ,0x66 ,0x69 ,0xc7 ,0x0e ,0x94 ,0x1B ,0xec ,0xc1 ,0x78 ,0x58 ,0x29 ,0x9a ,0xeB ,0x41 ,0xd3 ,0xd0 ,0x93 ,0xd9 ,0xcB ,0x78 ,0xe7 ,0x8d ,0x79 ,0x16 ,0xc3 ,0x50 ,0xfe ,0x61 ,0x47 ,0x05 ,0x1B ,0xec ,0xe0 ,0x27 ,0x47 ,0x4f ,0x2a ,0x23 ,0xaa ,0x38 ,0x1e ,0x05 ,0xc7 ,0x5c ,0x6e ,0x84 ,0x45 ,0x75};
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
          //ESP_LOGE(TAG, "T : %s", telegram.c_str());
          //ESP_LOGE(TAG, "T': %s", decrypted_telegram.c_str());
        }
      }
      if (selected_driver->get_value(frame, value)) {
        ESP_LOGI(TAG, "Value from telegram [0x%08X]: %l", meter_id, value);
      }
      else {
        ESP_LOGE(TAG, "Can't get value from telegram for ID [0x%08X] '%s'", meter_id, selected_driver->get_name().c_str());
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
