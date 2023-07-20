#pragma once

#include "esphome/core/log.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

#include <map>
#include <string>

//wMBus lib
#include "rf_mbus.hpp"
#include "crc.hpp"
#include "mbus_packet.hpp"
#include "utils.hpp"

#include "drivers.h"

namespace esphome {
namespace wmbus {

class WMBusListener {
  public:
    WMBusListener(const uint32_t id, const std::string type, const std::string key);
    uint32_t id;
    std::string type;
    std::vector<unsigned char> key{};
    std::map<std::string, sensor::Sensor *> sensors_{};
    void add_sensor(std::string type, sensor::Sensor *sensor) {
      this->sensors_[type] = sensor;
    };

    void dump_config();
    int char_to_int(char input);
    bool hex_to_bin(const std::string &src, std::vector<unsigned char> *target) { return hex_to_bin(src.c_str(), target); };
    bool hex_to_bin(const char* src, std::vector<unsigned char> *target);
};

struct Cc1101 {
  InternalGPIOPin *mosi{nullptr};
  InternalGPIOPin *miso{nullptr};
  InternalGPIOPin *clk{nullptr};
  InternalGPIOPin *cs{nullptr};
  InternalGPIOPin *gdo0{nullptr};
  InternalGPIOPin *gdo2{nullptr};
};

class WMBusComponent : public PollingComponent {
  public:
    void setup() override;
    void update() override;
    void dump_config() override;
    float get_setup_priority() const override { return setup_priority::LATE; }
    void set_led_pin(GPIOPin *led) { this->led_pin_ = led; }
    void set_led_blink_time(uint32_t led_blink_time) { this->led_blink_time_ = led_blink_time; }
    void register_wmbus_listener(WMBusListener *listener);
    void add_cc1101(InternalGPIOPin *mosi, InternalGPIOPin *miso,
                    InternalGPIOPin *clk, InternalGPIOPin *cs,
                    InternalGPIOPin *gdo0, InternalGPIOPin *gdo2) {
      this->spi_conf_.mosi = mosi;
      this->spi_conf_.miso = miso;
      this->spi_conf_.clk  = clk;
      this->spi_conf_.cs   = cs;
      this->spi_conf_.gdo0 = gdo0;
      this->spi_conf_.gdo2 = gdo2;
    }
    void set_log_unknown() { this->log_unknown_ = true; }

  private:

  protected:
    void add_driver(Driver *driver);
    bool decrypt_telegram(std::vector<unsigned char> &telegram, std::vector<unsigned char> &key);
    void led_blink();
    void led_handler();
    HighFrequencyLoopRequester high_freq_;
    GPIOPin *led_pin_{nullptr};
    Cc1101 spi_conf_{};
    std::map<uint32_t, WMBusListener *> wmbus_listeners_{};
    std::map<std::string, Driver *> drivers_{};
    uint32_t led_blink_time_{0};
    uint32_t led_on_millis_{0};
    bool led_on_{false};
    bool log_unknown_{true};
    rf_mbus rf_mbus_;
};

}  // namespace wmbus
}  // namespace esphome
