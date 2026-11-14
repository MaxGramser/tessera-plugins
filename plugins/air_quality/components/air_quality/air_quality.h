#pragma once
#include "esphome/core/component.h"
#include "esphome/components/smart_display/plugin_api.h"

namespace esphome::air_quality {

// A day of a sensor as Home Assistant's recorder keeps it: the mean of each hour, oldest first, and the lowest and
// the highest value of the day.
struct History {
  uint32_t id = 0;   // the question it answers (AirQuality::ask_history)
  bool ok = false;
  uint8_t count = 0;
  float mean[26] = {};
  float low = 0, high = 0;
};

// Who hears a day when it is there: a tile that judges how even its humidity is, the popup that draws it.
class HistoryListener {
 public:
  virtual void on_history(const History &history) = 0;
};

// The plugin: an ESPHome component and a tessera::Plugin. It registers the tile and its card, and asks Home Assistant
// for a sensor's last 24 hours on behalf of whoever needs them.
class AirQuality : public Component, public tessera::Plugin {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void on_message(JsonObjectConst message) override;
  // Asks for the last 24 hours of a sensor; 0 when it could not be asked (no clock yet, no sensor). The answer
  // reaches every listener with this number.
  uint32_t ask_history(const char *entity);
  void listen(HistoryListener *listener);
  void forget(HistoryListener *listener);

 private:
  HistoryListener *listeners_[16] = {};
};

}  // namespace esphome::air_quality
