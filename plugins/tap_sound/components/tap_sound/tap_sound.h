#pragma once

#include "esphome/core/component.h"
#include "esphome/components/speaker/speaker.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/smart_display/plugin_api.h"
#include "esphome/components/smart_display/plugin_sound.h"

namespace esphome::tap_sound {

// The plugin: a tick for every tap the screen takes, and the switch that turns it off on the screen's settings page.
// A tick never waits for one before it: a quick second tap starts it again.
class TapSound : public Component, public tessera::Plugin {
 public:
  float get_setup_priority() const override { return setup_priority::LATE; }
  void loop() override { sounds_.loop(); }
  void on_touch() override;
  bool settings(tessera::SettingsPage &page) override;
  void before_update() override { sounds_.stop(); }

  void set_speaker(speaker::Speaker *s) { sounds_.set_speaker(s); }
  void set_sound_on(switch_::Switch *s) { sound_on_ = s; }
  void set_tick(tessera::Sound s) { tick_ = s; }

 protected:
  switch_::Switch *sound_on_{};
  tessera::Sound tick_;
  tessera::SoundPlayer sounds_;
};

}  // namespace esphome::tap_sound
