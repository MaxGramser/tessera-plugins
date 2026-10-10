#include "voice_assist.h"

#include "esphome/components/api/api_server.h"
#include "esphome/core/hal.h"

namespace esphome::voice_assist {

using tessera::Font;
namespace ui = tessera::ui;

// Icons of Tessera's set: the assistant, the assistant thinking, the assistant that could not help.
static constexpr uint32_t ICON_ASSISTANT = 0xF06A9;   // robot
static constexpr uint32_t ICON_THINKING = 0xF01D8;    // dots-horizontal
static constexpr uint32_t ICON_FAILED = 0xF169F;      // robot-confused
// How long a reply or a failure stays on the tile after it was spoken.
static constexpr uint32_t REPLY_SHOWN_MS = 20000;

// The tile: the assistant's icon, what it does now, and under it what it heard or answered. A tap asks, or stops.
class AssistTile : public tessera::Tile {
 public:
  explicit AssistTile(VoiceAssist *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    width_ = c.width;
    height_ = c.height;
    wide_ = c.columns >= 2 && c.width > c.height * 3 / 2;
    icon_ = ui::label(c.parent, Font::ICON, theme::MUTED);
    lv_obj_set_style_text_align(icon_, LV_TEXT_ALIGN_CENTER, 0);
    status_ = ui::label(c.parent, Font::TITLE, theme::INK);
    detail_ = ui::label(c.parent, Font::BODY, theme::INK_SOFT);
    lv_label_set_long_mode(detail_, LV_LABEL_LONG_DOT);
    if (!wide_) {
      lv_obj_set_style_text_align(status_, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_style_text_align(detail_, LV_TEXT_ALIGN_CENTER, 0);
    }
  }

  void on_tick(uint32_t) override {
    const Phase phase = plugin_->current();
    const bool busy = phase == Phase::LISTENING || phase == Phase::THINKING || phase == Phase::ANSWERING;
    const bool failed = plugin_->showing_failure();
    uint32_t glyph = phase == Phase::THINKING ? ICON_THINKING : failed ? ICON_FAILED : ICON_ASSISTANT;
    ui::set_text(icon_, ui::icon(glyph));
    ui::set_color(icon_, busy ? theme::ACCENT : theme::MUTED);
    std::string status = busy || failed ? plugin_->phase_word() : plugin_->idle_line();
    // Under it: what was heard while it thinks, the reply while and after it answers.
    std::string detail;
    if (phase == Phase::THINKING) detail = plugin_->question();
    else if (phase == Phase::ANSWERING || plugin_->reply_fresh()) detail = plugin_->reply();
    layout(status, detail);
  }

  void on_theme() override {
    ui::set_color(status_, theme::INK);
    ui::set_color(detail_, theme::INK_SOFT);
    on_tick(0);
  }

  void on_tap() override { plugin_->ask(); }

 private:
  // The icon, the status line and as many lines of the detail as fit: beside each other on a wide card, under each
  // other on any other. The status takes the largest of the screen's fonts that fits.
  void layout(const std::string &status, const std::string &detail) {
    const int icon_h = ui::line_height(Font::ICON);
    const int text_w = wide_ ? width_ - icon_h - ui::px(12) : width_;
    Font face = Font::BODY;
    for (Font f : {Font::HEADLINE, Font::TITLE, Font::BODY_LARGE, Font::BODY}) {
      face = f;
      if (ui::text_width(status, f) <= text_w) break;
    }
    ui::set_font(status_, face);
    ui::set_text(status_, status);
    ui::set_text(detail_, detail);
    const int status_h = ui::line_height(face), line_h = ui::line_height(Font::BODY);
    if (wide_) {
      lv_obj_set_pos(icon_, 0, (height_ - icon_h) / 2);
      lv_obj_set_size(icon_, icon_h, icon_h);
      const int lines = detail.empty() ? 0 : std::max(1, std::min(3, (height_ - status_h) / line_h));
      const int top = (height_ - status_h - lines * line_h) / 2;
      lv_obj_set_pos(status_, icon_h + ui::px(12), top);
      lv_obj_set_size(status_, text_w, status_h);
      lv_obj_set_pos(detail_, icon_h + ui::px(12), top + status_h);
      lv_obj_set_size(detail_, text_w, lines ? lines * line_h : 1);
      return;
    }
    const int room = height_ - icon_h - status_h;
    const int lines = detail.empty() || room < line_h ? 0 : std::min(3, room / line_h);
    const int top = std::max(0, (height_ - icon_h - status_h - lines * line_h) / 2);
    lv_obj_set_pos(icon_, 0, top);
    lv_obj_set_size(icon_, width_, icon_h);
    lv_obj_set_pos(status_, 0, top + icon_h);
    lv_obj_set_size(status_, width_, status_h);
    lv_obj_set_pos(detail_, 0, top + icon_h + status_h);
    lv_obj_set_size(detail_, width_, lines ? lines * line_h : 1);
  }

  VoiceAssist *plugin_;
  int width_ = 0, height_ = 0;
  bool wide_ = false;
  lv_obj_t *icon_{}, *status_{}, *detail_{};
};

void VoiceAssist::setup() {
  add_tile("assist", [this]() { return new AssistTile(this); });
  add_bar_item("busy", [this]() {
    tessera::BarItem item;
    const bool fresh_failure = showing_failure();
    if (busy() || phase_ == Phase::LISTENING || phase_ == Phase::THINKING || phase_ == Phase::ANSWERING || fresh_failure) {
      item.shown = true;
      item.icon = phase_ == Phase::THINKING ? ICON_THINKING : fresh_failure ? ICON_FAILED : ICON_ASSISTANT;
      item.text = phase_word();
    }
    return item;
  });
}

void VoiceAssist::on_ready() {
  // The phrase Wake phrase kept over the restart listens; the others rest.
  auto index = wake_phrase_ != nullptr ? wake_phrase_->active_index() : optional<size_t>{};
  choose_phrase(index.has_value() ? *index : 0);
}

bool VoiceAssist::settings(tessera::SettingsPage &page) {
  page.icon = "\U000F06A9";
  page.toggle(text("wake_word"), [this] { return wake_word_on_ != nullptr && wake_word_on_->state; },
              [this](bool on) { on ? wake_word_on_->turn_on() : wake_word_on_->turn_off(); });
  if (wake_phrase_ != nullptr) {
    std::vector<std::string> options;
    for (size_t i = 0; i < wake_phrase_->size(); ++i) options.emplace_back(wake_phrase_->option_at(i));
    page.choice(text("wake_phrase"), options,
                [this] { auto index = wake_phrase_->active_index(); return index.has_value() ? (int) *index : 0; },
                [this](int index) { wake_phrase_->make_call().set_index((size_t) index).perform(); });
  }
  page.action(text("ask"), "\U000F06A9", [this] { ask(); }, nullptr,
              [this]() -> std::string { return busy() ? phase_word() : std::string(); })
      .active([this] { return busy(); });
  return true;
}

void VoiceAssist::phase(Phase p) {
  if (p == Phase::IDLE && phase_ == Phase::FAILED) {
    // The pipeline ends after a failure too: the failure stays on the tile while it is fresh.
  } else {
    phase_ = p;
  }
  if (p == Phase::LISTENING) {
    heard_.clear();
    answer_.clear();
  }
  tessera::refresh();
}

void VoiceAssist::heard(const std::string &text) {
  heard_ = text;
  tessera::refresh();
}

void VoiceAssist::answer(const std::string &text) {
  answer_ = text;
  answered_at_ = millis();
  phase_ = Phase::ANSWERING;
  tessera::refresh();
}

void VoiceAssist::failed(const std::string &code, const std::string &message) {
  ESP_LOGW("voice_assist", "Assist failed: %s (%s)", code.c_str(), message.c_str());
  answer_ = message.empty() ? code : message;
  answered_at_ = millis();
  phase_ = Phase::FAILED;
  tessera::refresh();
}

void VoiceAssist::choose_phrase(size_t index) {
  for (size_t i = 0; i < phrases_.size(); ++i) {
    if (i == index) phrases_[i]->enable();
    else phrases_[i]->disable();
  }
  tessera::refresh();
}

void VoiceAssist::ask() {
  if (assistant_ == nullptr) return;
  if (busy()) {
    assistant_->request_stop();
    return;
  }
  phase_ = Phase::LISTENING;
  heard_.clear();
  answer_.clear();
  assistant_->request_start(false, true);
  tessera::refresh();
}

void VoiceAssist::stop() {
  if (busy()) assistant_->request_stop();
}

bool VoiceAssist::reply_fresh() const {
  return answered_at_ != 0 && millis() - answered_at_ < REPLY_SHOWN_MS;
}

std::string VoiceAssist::idle_line() const {
  if (api::global_api_server == nullptr || !api::global_api_server->is_connected()) return text("away");
  if (wake_word_on_ != nullptr && wake_word_on_->state && wake_phrase_ != nullptr && wake_phrase_->has_state())
    return tessera::fill(text("say_phrase"), "phrase", std::string(wake_phrase_->current_option()));
  return text("tap_to_ask");
}

std::string VoiceAssist::phase_word() const {
  // A failure that is no longer fresh is quiet again.
  if (phase_ == Phase::FAILED && !reply_fresh()) return idle_line();
  switch (phase_) {
    case Phase::LISTENING: return text("listening");
    case Phase::THINKING: return text("thinking");
    case Phase::ANSWERING: return text("answering");
    case Phase::FAILED: return text("failed");
    default: return idle_line();
  }
}

}  // namespace esphome::voice_assist
