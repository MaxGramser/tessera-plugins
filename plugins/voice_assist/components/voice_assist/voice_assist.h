#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/micro_wake_word/micro_wake_word.h"
#include "esphome/components/select/select.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/voice_assistant/voice_assistant.h"
#include "esphome/components/smart_display/plugin_api.h"

namespace esphome::voice_assist {

// Where a question is: Home Assistant's Assist pipeline tells it through voice_assistant's triggers (plugin.yaml).
enum class Phase : uint8_t { IDLE, LISTENING, THINKING, ANSWERING, FAILED };

// The plugin: a tile that says what the assistant does and asks it on a tap, an item for the top bar while it is busy,
// and on the screen's settings page the wake word, the phrase it listens for and a row that asks now.
class VoiceAssist : public Component, public tessera::Plugin {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::LATE; }
  void on_ready() override;
  bool settings(tessera::SettingsPage &page) override;
  void before_update() override { stop(); }

  void set_assistant(voice_assistant::VoiceAssistant *a) { assistant_ = a; }
  void set_wake_word(micro_wake_word::MicroWakeWord *w) { wake_word_ = w; }
  void set_wake_word_on(switch_::Switch *s) { wake_word_on_ = s; }
  void set_wake_phrase(select::Select *s) { wake_phrase_ = s; }
  void add_phrase(micro_wake_word::WakeWordModel *m) { phrases_.push_back(m); }

  // From plugin.yaml's triggers.
  void phase(Phase p);
  void heard(const std::string &text);
  void answer(const std::string &text);
  void failed(const std::string &code, const std::string &message);
  // Wake phrase chose its option `index`: that model listens, the others rest.
  void choose_phrase(size_t index);

  // A tap on the tile, or Ask on the settings page: ask now, or stop what runs.
  void ask();
  void stop();
  bool busy() const { return assistant_ != nullptr && assistant_->is_running(); }

  // What the tile and the top bar show.
  Phase current() const { return phase_; }
  const std::string &question() const { return heard_; }
  const std::string &reply() const { return answer_; }
  std::string idle_line() const;
  std::string phase_word() const;
  // A reply stays on the tile a while after it was spoken, and so does a failure.
  bool reply_fresh() const;
  bool showing_failure() const { return phase_ == Phase::FAILED && reply_fresh(); }

 protected:
  voice_assistant::VoiceAssistant *assistant_{};
  micro_wake_word::MicroWakeWord *wake_word_{};
  switch_::Switch *wake_word_on_{};
  select::Select *wake_phrase_{};
  std::vector<micro_wake_word::WakeWordModel *> phrases_;

  Phase phase_{Phase::IDLE};
  std::string heard_, answer_;
  uint32_t answered_at_{0};     // millis() of the last reply, or of the failure
};

}  // namespace esphome::voice_assist
