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
  ESP_LOGI(TAG, "Aqq %d", 123);
  if (true) {
    std::vector<unsigned char> frame = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::string telegram = format_hex_pretty(frame);
    telegram.erase(std::remove(telegram.begin(), telegram.end(), '.'), telegram.end());

    // ToDo: add check for manufactures
    uint32_t meter_id = ((uint32_t)frame[7] << 24) | ((uint32_t)frame[6] << 16) |
                        ((uint32_t)frame[5] << 8)  | ((uint32_t)frame[4]);

    ESP_LOGI(TAG, "Meter ID [0x%08X] T: %s", meter_id, telegram.c_str());
    this->led_blink();
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
