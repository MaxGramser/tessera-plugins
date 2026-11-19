#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

#include "esphome/components/smart_display/plugin_api.h"

namespace esphome::voice_assist {

// What the popup of a timer that ran out shows now, as the plugin knows it (VoiceAssist::timer_view).
struct TimerView {
  bool ringing = false;
  std::string title;  // the timer's name, or "Time is up"; "2 timers are up" for several
  std::string line;   // what ran out: "Time is up · 5 min", or the names of several
  std::string hint;   // "Tap to turn it off" while it rings, "" once it stopped by itself
};

// The popup of a timer that ran out (plugin API 0.18): a disc in the alert's colour with a ringing bell, on one line
// with the popup's close key and as big, the timer's name beside it, and under them what ran out and how to stop it.
// While it rings, rings ripple out of the disc, as the voice popup's do while it listens. A tap on it stops the ring and
// closes it; going by itself (its close key, a tap beside it, standby, the wake word) stops the ring too.
class TimerPopup : public tessera::Popup {
 public:
  TimerPopup(uint32_t icon, std::function<TimerView()> read, std::function<void()> tapped,
             std::function<void()> dismissed)
      : icon_code_(icon), read_(std::move(read)), tapped_(std::move(tapped)), dismissed_(std::move(dismissed)) {}

  void open(const tessera::PopupContext &c) override {
    namespace ui = tessera::ui;
    using tessera::Font;
    width_ = c.width;
    tallest_ = c.height;
    const bool large = ui::large();
    disc_ = c.key > 0 ? c.key : ui::px(large ? 60 : 40);
    const lv_color_t alert = ui::tone_color(tessera::Tone::ALERT, theme::ACCENT);
    for (auto &ripple : ripples_) {
      ripple = circle(c.parent, disc_);
      lv_obj_set_style_bg_opa(ripple, LV_OPA_TRANSP, 0);
      lv_obj_set_style_border_width(ripple, ui::px(large ? 3 : 2), 0);
      lv_obj_set_style_border_color(ripple, alert, 0);
    }
    fill_ = circle(c.parent, disc_);
    ui::set_fill(fill_, tessera::Tone::ALERT, theme::ACCENT);
    icon_ = ui::label(fill_, Font::ICON_SMALL, theme::ON_ACCENT);
    lv_obj_set_width(icon_, disc_);
    lv_obj_set_style_text_align(icon_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(icon_);
    ui::set_text(icon_, ui::icon(icon_code_));
    title_ = ui::label(c.parent, Font::HEADLINE, theme::INK);
    const int title_x = disc_ + ui::px(large ? 16 : 10);
    lv_obj_set_x(title_, title_x);
    title_w_ = std::max(1, width_ - title_x - c.corner);  // short of the popup's close key
    lv_obj_set_width(title_, title_w_);
    top_ = disc_ + ui::px(large ? 14 : 8);
    gap_ = ui::px(6);
    line_ = ui::text(c.parent, Font::BODY_LARGE, theme::INK);
    lv_obj_set_pos(line_, 0, top_);
    hint_ = ui::label(c.parent, Font::BODY, theme::INK_SOFT);
    lv_obj_set_width(hint_, width_);
    show(true);
  }

  void on_interval(uint32_t) override { show(false); }
  void on_theme() override {
    tessera::ui::set_color(title_, theme::INK);
    tessera::ui::set_color(line_, theme::INK);
    tessera::ui::set_color(hint_, theme::INK_SOFT);
    tessera::ui::set_fill(fill_, tessera::Tone::ALERT, theme::ACCENT);
    show(true);
  }
  void on_tap() override {
    if (tapped_) tapped_();
  }
  void on_dismiss() override {
    if (dismissed_) dismissed_();
  }

  // Draws what the plugin says now, and only what changed.
  void show(bool force) {
    namespace ui = tessera::ui;
    using tessera::Font;
    const TimerView v = read_();
    if (force || v.ringing != ringing_) {
      ringing_ = v.ringing;
      motion();
    }
    // The title in the largest of the screen's fonts that fits beside the disc.
    if (force || std::string_view(title_text_) != v.title) {
      title_text_.assign(v.title.data(), v.title.size());
      Font face = Font::BODY;
      for (Font f : {Font::HEADLINE, Font::TITLE, Font::BODY_LARGE, Font::BODY}) {
        face = f;
        if (ui::text_width(v.title, f) <= title_w_) break;
      }
      ui::set_font(title_, face);
      lv_obj_set_y(title_, (disc_ - ui::line_height(face)) / 2);
      ui::set_text(title_, v.title);
    }
    // Under the disc: what ran out, up to three lines, then the hint. The card is as high as what shows.
    if (force || std::string_view(line_text_) != v.line || std::string_view(hint_text_) != v.hint) {
      line_text_.assign(v.line.data(), v.line.size());
      hint_text_.assign(v.hint.data(), v.hint.size());
      const int big = ui::line_height(Font::BODY_LARGE), small = ui::line_height(Font::BODY);
      const int room = std::max(0, tallest_ - top_ - (v.hint.empty() ? 0 : small + gap_));
      const int line_lines = std::min({3, room / std::max(1, big), lines(v.line, Font::BODY_LARGE)});
      ui::set_text(line_, v.line);
      lv_obj_set_size(line_, width_, std::max(1, line_lines * big));
      hide(line_, line_lines == 0);
      const int hint_top = top_ + (line_lines ? line_lines * big + gap_ : 0);
      lv_obj_set_y(hint_, hint_top);
      ui::set_text(hint_, v.hint);
      hide(hint_, v.hint.empty());
    }
  }

 protected:
  lv_obj_t *circle(lv_obj_t *parent, int size) {
    lv_obj_t *c = tessera::ui::block(parent, theme::ACCENT);
    lv_obj_set_size(c, size, size);
    lv_obj_set_pos(c, 0, 0);
    lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
    return c;
  }
  static void hide(lv_obj_t *part, bool hidden) {
    if (hidden) lv_obj_add_flag(part, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(part, LV_OBJ_FLAG_HIDDEN);
  }
  // How many lines a text takes at the room's width.
  int lines(const std::string &text, tessera::Font font) const {
    if (text.empty()) return 0;
    const lv_font_t *face = tessera::ui::font(font);
    if (!face) return 1;
    lv_point_t size;
    lv_text_get_size(&size, text.c_str(), face, 0, 0, width_, LV_TEXT_FLAG_NONE);
    return std::max(1, (int) (size.y / std::max(1, (int) lv_font_get_line_height(face))));
  }
  // Ripples out of the disc while it rings; still once it stopped.
  void motion() {
    namespace ui = tessera::ui;
    for (int i = 0; i < 2; ++i) {
      hide(ripples_[i], !ringing_);
      if (ringing_) ui::animate(ripples_[i], tessera::Motion::RIPPLE, 1600, i * 800);
      else ui::still(ripples_[i]);
    }
  }

  uint32_t icon_code_;
  std::function<TimerView()> read_;
  std::function<void()> tapped_, dismissed_;
  int width_ = 0, tallest_ = 0, disc_ = 0, title_w_ = 1, top_ = 0, gap_ = 0;
  bool ringing_ = false;
  tessera::Text title_text_, line_text_, hint_text_;  // what shows, in PSRAM
  lv_obj_t *ripples_[2]{}, *fill_{}, *icon_{}, *title_{}, *line_{}, *hint_{};
};

}  // namespace esphome::voice_assist
