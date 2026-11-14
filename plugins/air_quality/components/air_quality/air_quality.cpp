#include "air_quality.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

namespace esphome::air_quality {

using tessera::Font;
using tessera::Tone;
namespace ui = tessera::ui;

// ---------------------------------------------------------------------------------------------------------------------
// The model: what is measured, what counts as good in a kind of room, and the verdict. No LVGL in this part.
// ---------------------------------------------------------------------------------------------------------------------

enum Measure : uint8_t { CO2, PM25, PM10, VOC, NOX, TEMP, HUM, SWING, MEASURES };
enum Level : uint8_t { GOOD, FAIR, POOR };

// The option of the tile that names each measure's sensor, and the text key of its name. SWING has no sensor of its
// own: it is how far the humidity moved in the last 24 hours, for rooms where an even climate counts.
static const char *const KEY[MEASURES] = {"co2", "pm25", "pm10", "voc", "nox", "temperature", "humidity", "swing"};
// molecule-co2, air-filter, air-filter, molecule, fire, thermometer, water-percent, gauge; the leaf of air that is fine.
static const uint32_t ICON[MEASURES] = {0xF07E4, 0xF0D43, 0xF0D43, 0xF0BAC, 0xF0238, 0xF050F, 0xF058E, 0xF029A};

// A measure's scale. One-sided (CO2, dust): fine from `fair_lo` (where the scale starts) up to `fair_hi`, fair up to
// `poor_hi`, and the scale ends at `end_hi`. Two-sided (temperature, humidity): fine between fair_lo and fair_hi, fair
// out to poor_lo and poor_hi, and the scale ends at end_lo and end_hi.
struct Band {
  bool two;
  float end_lo, poor_lo, fair_lo, fair_hi, poor_hi, end_hi;
};
static constexpr Band up(float floor, float fair, float poor, float end) { return {false, 0, 0, floor, fair, poor, end}; }
static constexpr Band mid(float end_lo, float poor_lo, float fair_lo, float fair_hi, float poor_hi, float end_hi) {
  return {true, end_lo, poor_lo, fair_lo, fair_hi, poor_hi, end_hi};
}
static constexpr Band NO_BAND = {false, 0, 0, 0, 0, 0, 0};

// What counts as good in a kind of room (README.md says where each number comes from). `swing` is how many points the
// humidity may move in 24 hours; only rooms that keep instruments or a collection judge it.
struct Profile {
  const char *id;
  Band co2, pm25, pm10, temp, hum, swing;
};
static const Profile PROFILES[] = {
    {"living", up(400, 1000, 1500, 2500), up(0, 10, 25, 75), up(0, 20, 45, 150), mid(13, 17, 19, 24, 27, 32), mid(15, 30, 40, 60, 70, 85), NO_BAND},
    {"bedroom", up(400, 800, 1000, 2000), up(0, 10, 25, 75), up(0, 20, 45, 150), mid(10, 14, 16, 20, 22, 27), mid(15, 30, 40, 60, 70, 85), NO_BAND},
    {"office", up(400, 800, 1000, 2000), up(0, 10, 25, 75), up(0, 20, 45, 150), mid(15, 18, 20, 24, 26, 31), mid(15, 30, 40, 60, 70, 85), NO_BAND},
    {"studio", up(400, 800, 1200, 2000), up(0, 10, 25, 75), up(0, 20, 45, 150), mid(13, 17, 20, 24, 26, 31), mid(25, 40, 45, 55, 60, 75), up(0, 15, 30, 50)},
    {"gallery", up(400, 1000, 1500, 2500), up(0, 5, 10, 35), up(0, 10, 30, 100), mid(8, 13, 16, 25, 27, 32), mid(25, 35, 40, 60, 65, 75), up(0, 10, 20, 40)},
    {"workshop", up(400, 1000, 1500, 2500), up(0, 10, 25, 75), up(0, 20, 45, 150), mid(6, 12, 15, 25, 29, 35), mid(10, 25, 35, 65, 75, 90), NO_BAND},
    {"nursery", up(400, 800, 1000, 2000), up(0, 10, 25, 75), up(0, 20, 45, 150), mid(10, 14, 16, 20, 22, 27), mid(15, 30, 40, 60, 70, 85), NO_BAND},
    {"kitchen", up(400, 1000, 1500, 2500), up(0, 15, 35, 100), up(0, 30, 60, 200), mid(11, 16, 18, 25, 28, 33), mid(15, 30, 40, 65, 75, 90), NO_BAND},
};
// A VOC sensor says an index (Sensirion: 100 is the room's own average), parts per billion or micrograms.
static const Band VOC_INDEX = up(100, 150, 250, 400), VOC_PPB = up(0, 220, 660, 2200), VOC_UG = up(0, 300, 1000, 3000);
static const Band NOX_INDEX = up(1, 20, 150, 300);

struct Reading {
  bool chosen = false, valid = false;
  float value = 0;     // as the sensor says it
  float p = 0;         // how far off: 0 ideal, 1 where fair starts, 2 where poor starts, 3 the end of the scale
  int8_t dir = 1;      // -1 too low, +1 too high
  Level level = GOOD;
  float q = 100;       // 100 ideal, 80 where fair starts, 60 where poor starts, 0 the end of the scale
  float pos = 0;       // where it stands on its scale, 0 to 1
  bool two = false;
  char text[12] = "";  // the value as words
  char unit[12] = "";
};

static float steps(float v, float a, float b, float c, float d) {
  if (v <= a) return 0;
  if (v < b) return (v - a) / (b - a);
  if (v < c) return 1 + (v - b) / (c - b);
  if (v < d) return 2 + (v - c) / (d - c);
  return 3;
}

static void judge(Reading &r, const Band &b, float v) {
  r.two = b.two;
  if (b.two) {
    const float ideal = (b.fair_lo + b.fair_hi) / 2;
    if (v >= ideal) {
      r.dir = 1;
      r.p = steps(v, ideal, b.fair_hi, b.poor_hi, b.end_hi);
    } else {
      r.dir = -1;
      r.p = steps(-v, -ideal, -b.fair_lo, -b.poor_lo, -b.end_lo);
    }
    r.pos = 0.5f + r.dir * r.p / 6;
  } else {
    r.dir = 1;
    r.p = steps(v, b.fair_lo, b.fair_hi, b.poor_hi, b.end_hi);
    r.pos = r.p / 3;
  }
  r.level = r.p < 1 ? GOOD : r.p < 2 ? FAIR : POOR;
  r.q = r.p <= 1 ? 100 - 20 * r.p : r.p <= 2 ? 80 - 20 * (r.p - 1) : 60 - 60 * (r.p - 2);
}

// A band with a person's own limits in place of the room's: `low` and `high` are where fine ends (NAN: the room's).
// The rest of the scale keeps its shape: a one-sided scale stretches with its limit, a two-sided one moves its ends
// along.
static Band tuned(Band b, float low, float high) {
  if (b.two) {
    if (std::isfinite(low) && std::isfinite(high) && low >= high) return b;
    if (std::isfinite(low)) {
      const float d = low - b.fair_lo;
      b.fair_lo = low, b.poor_lo += d, b.end_lo += d;
    }
    if (std::isfinite(high)) {
      const float d = high - b.fair_hi;
      b.fair_hi = high, b.poor_hi += d, b.end_hi += d;
    }
  } else if (std::isfinite(high) && high > b.fair_lo && b.fair_hi > b.fair_lo) {
    const float k = (high - b.fair_lo) / (b.fair_hi - b.fair_lo);
    b.poor_hi = b.fair_lo + (b.poor_hi - b.fair_lo) * k;
    b.end_hi = b.fair_lo + (b.end_hi - b.fair_lo) * k;
    b.fair_hi = high;
  }
  return b;
}

// The air of one room: its sensors, its limits, and the verdict.
struct Air {
  Reading r[MEASURES];
  const Profile *profile = &PROFILES[0];
  char entity[MEASURES][72] = {};       // the sensor of each measure, "" for none
  float low[MEASURES], high[MEASURES];  // a person's own limits, NAN for the room's
  Band band[MEASURES];                  // the scale each measure was last judged on,
  float mul[MEASURES], add[MEASURES];   // and how its sensor's unit comes to that scale's (ppm to ppb, °F to °C)
  float swing = NAN;                    // how far the humidity moved in 24 hours, NAN while not known
  int chosen = 0, valid = 0, off = 0, worst = -1, score = 0;
  Level level = GOOD;

  Air() {
    for (int i = 0; i < MEASURES; ++i) low[i] = high[i] = NAN, band[i] = NO_BAND, mul[i] = 1, add[i] = 0;
  }
  void room(const char *id) {
    for (const Profile &p : PROFILES)
      if (!strcmp(p.id, id)) profile = &p;
  }
  // The sensors chosen: a measure counts when it has one. The humidity's swing counts where the room judges it.
  void choose() {
    chosen = 0;
    for (int i = 0; i < MEASURES; ++i) {
      r[i].chosen = i == SWING ? entity[HUM][0] && profile->swing.fair_hi > 0 : entity[i][0] != 0;
      if (r[i].chosen) ++chosen;
    }
  }
  // A value of a measure on its scale, in the unit its sensor says it.
  Reading judged(int i, float v) const {
    Reading out;
    judge(out, band[i], v * mul[i] + add[i]);
    return out;
  }

  // The sensors' states, as the app sends them under `entities` and each option's id.
  void read(JsonObjectConst data) {
    JsonObjectConst entities = data["entities"];
    for (int i = 0; i < MEASURES; ++i) {
      Reading &m = r[i];
      m.valid = false;
      m.text[0] = m.unit[0] = 0;
      if (!m.chosen || i == SWING) continue;
      JsonObjectConst e = entities[KEY[i]];
      const char *state = e["state"] | "";
      char *end = nullptr;
      const float v = strtof(state, &end);
      if (!*state || end == state || !std::isfinite(v)) continue;
      const char *unit = e["attributes"]["unit_of_measurement"] | "";
      m.valid = true;
      m.value = v;
      mul[i] = 1, add[i] = 0;
      const Band *base = nullptr;
      switch (i) {
        case CO2: base = &profile->co2; break;
        case PM25: base = &profile->pm25; break;
        case PM10: base = &profile->pm10; break;
        case NOX: base = &NOX_INDEX; break;
        case HUM: base = &profile->hum; break;
        case TEMP:
          base = &profile->temp;
          if (strchr(unit, 'F')) mul[i] = 1 / 1.8f, add[i] = -32 / 1.8f;
          break;
        default:
          if (strstr(unit, "ppb")) base = &VOC_PPB;
          else if (strstr(unit, "ppm")) base = &VOC_PPB, mul[i] = 1000;
          else if (strstr(unit, "mg/m")) base = &VOC_UG, mul[i] = 1000;
          else if (strstr(unit, "g/m")) base = &VOC_UG;
          else base = &VOC_INDEX;
      }
      band[i] = tuned(*base, low[i], high[i]);
      judge(m, band[i], v * mul[i] + add[i]);
      snprintf(m.text, sizeof(m.text), i == TEMP ? "%.1f" : "%.0f", v);
      snprintf(m.unit, sizeof(m.unit), "%s", i == TEMP ? "°" : unit);
    }
    settle();
  }

  // The verdict: the worst measure's. Called again when the humidity's swing became known.
  void settle() {
    Reading &s = r[SWING];
    s.valid = s.chosen && std::isfinite(swing);
    if (s.valid) {
      band[SWING] = tuned(profile->swing, NAN, high[SWING]);
      s.value = swing;
      judge(s, band[SWING], swing);
      snprintf(s.text, sizeof(s.text), "%.0f", swing);
      snprintf(s.unit, sizeof(s.unit), "%%");
    }
    valid = off = 0;
    worst = -1;
    for (int i = 0; i < MEASURES; ++i) {
      if (!r[i].valid) continue;
      ++valid;
      if (r[i].level != GOOD) ++off;
      if (worst < 0 || r[i].q < r[worst].q) worst = i;
    }
    level = worst < 0 ? GOOD : r[worst].level;
    score = worst < 0 ? 0 : static_cast<int>(lroundf(r[worst].q));
  }

  // The chosen measures in the order to show them: as they are, or what is off first and then what people know best.
  int order(uint8_t *out, bool off_first) const {
    int n = 0;
    for (int i = 0; i < MEASURES; ++i)
      if (r[i].chosen) out[n++] = i;
    if (off_first) {
      static const uint8_t RANK[MEASURES] = {0, 1, 7, 5, 6, 3, 2, 4};   // CO2, PM2.5, humidity, temperature, swing, VOC, NOx, PM10
      std::stable_sort(out, out + n, [this](uint8_t a, uint8_t b) {
        const float qa = r[a].valid ? r[a].q : 101, qb = r[b].valid ? r[b].q : 101;
        if (qa < 80 || qb < 80) return qa < qb;
        return RANK[a] < RANK[b];
      });
    }
    return n;
  }
};

// ---------------------------------------------------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------------------------------------------------

static Tone tone_of(Level level) { return level == GOOD ? Tone::GOOD : level == FAIR ? Tone::BUSY : Tone::ALERT; }
static int lh(Font f) { return ui::line_height(f); }

static void show(lv_obj_t *o, bool on) {
  if (!o) return;
  if (on == !lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
  if (on) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}
static void fill(lv_obj_t *o, lv_color_t c) {
  if (o && !lv_color_eq(lv_obj_get_style_bg_color(o, LV_PART_MAIN), c)) lv_obj_set_style_bg_color(o, c, 0);
}
static void box(lv_obj_t *o, int x, int y, int w, int h) {
  if (!o) return;
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, std::max(1, w), std::max(1, h));
}
static lv_obj_t *round_block(lv_obj_t *parent, theme::Role role) {
  lv_obj_t *b = ui::block(parent, role);
  lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
  return b;
}
// How far a small label goes down beside a large one to stand on its baseline.
static int drop(Font large, Font small) {
  const lv_font_t *a = ui::font(large), *b = ui::font(small);
  return a && b ? (a->line_height - a->base_line) - (b->line_height - b->base_line) : 0;
}

// The colours of one face, worked out from the verdict's tone and the look's own roles, so light and dark both follow:
// the ground is the surface with a little of the tone in it, and everything on it is of that one colour family.
struct Palette {
  lv_color_t tone, ground, deep, mid, soft, ink, cell, cell_off, track;
};
static Palette palette_on(lv_color_t ground, Level level, bool any, theme::Role base);
// A ground of its own: the surface with a little of the tone in it (a row on the page, the popup).
static Palette palette_of(Level level, bool any, theme::Role base) {
  const lv_color_t tone = any ? ui::tone_color(tone_of(level), theme::MUTED) : ui::color(theme::MUTED);
  return palette_on(lv_color_mix(tone, ui::color(base), any ? 50 : 12), level, any, base);
}
// The card colour of a level (plugin API 0.26): the screen's own green, yellow and red for a card.
static tessera::CardColor card_of(Level level) {
  return level == GOOD ? tessera::CardColor::GREEN : level == FAIR ? tessera::CardColor::YELLOW : tessera::CardColor::RED;
}
static Palette palette_on(lv_color_t ground, Level level, bool any, theme::Role base) {
  Palette p;
  const lv_color_t surface = ui::color(base);
  p.ink = ui::color(theme::INK);
  p.tone = any ? ui::tone_color(tone_of(level), theme::MUTED) : ui::color(theme::MUTED);
  p.ground = ground;
  p.deep = p.tone;
  p.mid = lv_color_mix(p.tone, p.ground, 96);
  p.soft = lv_color_mix(p.ink, p.ground, 175);
  p.cell = lv_color_mix(surface, p.ground, 165);
  p.cell_off = lv_color_mix(p.tone, surface, 112);
  p.track = lv_color_mix(p.ink, p.ground, 30);
  return p;
}
static void ink(lv_obj_t *l, lv_color_t c) {
  if (l && !lv_color_eq(lv_obj_get_style_text_color(l, LV_PART_MAIN), c)) lv_obj_set_style_text_color(l, c, 0);
}

// Where a value stands: the zones of its scale as short bars, the one it is in lit, and a mark at the value. Three
// zones for a measure that is only ever too high, five for one that can be too low as well.
struct Scale {
  lv_obj_t *seg[5]{}, *mark{};
  int x = 0, y = 0, w = 0, h = 0;

  void make(lv_obj_t *parent) {
    for (auto &s : seg) s = round_block(parent, theme::TRACK);
    mark = round_block(parent, theme::INK);
  }
  void place(int x_, int y_, int w_, int h_) { x = x_, y = y_, w = w_, h = h_; }
  void paint(const Reading &r, lv_color_t quiet, lv_color_t lit, lv_color_t marker, lv_color_t ground) {
    static const int ONE[5] = {1, 1, 1, 0, 0}, TWO[5] = {1, 1, 2, 1, 1};
    const int *weight = r.two ? TWO : ONE, n = r.two ? 5 : 3, sum = r.two ? 6 : 3;
    const int gap = std::max(2, ui::px(3)), usable = w - (n - 1) * gap;
    const int active = !r.valid ? -1 : r.two ? (r.dir < 0 ? 2 - r.level : 2 + r.level) : r.level;
    int at = x;
    for (int i = 0; i < 5; ++i) {
      show(seg[i], i < n);
      if (i >= n) continue;
      const int sw = i == n - 1 ? x + w - at : usable * weight[i] / sum;
      box(seg[i], at, y, sw, h);
      fill(seg[i], i == active ? lit : quiet);
      at += sw + gap;
    }
    show(mark, r.valid);
    if (!r.valid) return;
    const int mw = std::max(3, ui::px(4)), mh = h + 2 * std::max(2, ui::px(3));
    const int mx = std::clamp(x + static_cast<int>(r.pos * w) - mw / 2, x, x + w - mw);
    box(mark, mx, y - (mh - h) / 2, mw, mh);
    fill(mark, marker);
    lv_obj_set_style_outline_width(mark, std::max(1, ui::px(2)), 0);
    lv_obj_set_style_outline_color(mark, ground, 0);
    lv_obj_set_style_outline_opa(mark, LV_OPA_COVER, 0);
    lv_obj_move_foreground(mark);
  }
};

// The ring: one arc for every sensor, clockwise from the top, each whole while its value is ideal and shorter as it
// goes off; the score of the worst one in the middle, and on a large ring the verdict under it.
struct Hero {
  lv_obj_t *arc[MEASURES]{}, *middle{}, *under{};
  uint8_t slot[MEASURES]{};
  int n = 0, x = 0, y = 0, size = 0, stroke = 0;
  float span = 0, gap = 0;
  Font face = Font::TITLE;
  bool words = false, worded = false;   // worded: the verdict stands inside the ring

  static int stroke_of(int size_) { return std::clamp(size_ * 12 / 100, std::max(4, ui::px(6)), ui::px(26)); }

  void make(lv_obj_t *parent, const Air &air) {
    n = air.order(slot, false);
    for (int i = 0; i < n; ++i) {
      lv_obj_t *a = arc[i] = lv_arc_create(parent);
      lv_obj_remove_style_all(a);
      lv_obj_remove_flag(a, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_arc_opa(a, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_arc_opa(a, LV_OPA_COVER, LV_PART_INDICATOR);
      lv_obj_set_style_arc_rounded(a, true, LV_PART_MAIN);
      lv_obj_set_style_arc_rounded(a, true, LV_PART_INDICATOR);
    }
    middle = ui::label(parent, Font::TITLE, theme::INK);
    under = ui::label(parent, Font::BODY_LARGE, theme::MUTED);
    lv_obj_set_style_text_align(middle, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_align(under, LV_TEXT_ALIGN_CENTER, 0);
  }

  // Whether a ring of this size has room for the verdict under its score.
  static bool takes_word(int size_) {
    const int inner = size_ - 2 * stroke_of(size_);
    return lh(Font::DIGITS) + lh(Font::BODY_LARGE) <= inner * 66 / 100 && ui::text_width("100", Font::DIGITS) <= inner * 74 / 100;
  }

  void place(int x_, int y_, int size_, bool with_word = false) {
    x = x_, y = y_, size = size_;
    stroke = stroke_of(size);
    // The round ends of two arcs need room between them: an end's own half width, twice, and a little air.
    gap = n > 1 ? stroke * 57.3f / ((size - stroke) / 2.0f) + 6 : 0;
    span = (360 - n * gap) / std::max(1, n);
    for (int i = 0; i < n; ++i) {
      box(arc[i], x, y, size, size);
      lv_obj_set_style_arc_width(arc[i], stroke, LV_PART_MAIN);
      lv_obj_set_style_arc_width(arc[i], stroke, LV_PART_INDICATOR);
    }
    const int inner = size - 2 * stroke;
    worded = with_word && takes_word(size);
    words = false;
    for (Font f : {Font::DIGITS, Font::VALUE, Font::HEADLINE, Font::TITLE, Font::BODY_LARGE, Font::BODY})
      if (ui::text_width("100", f) <= inner * 84 / 100 && lh(f) <= inner * 74 / 100) {
        face = f, words = true;
        break;
      }
    ui::set_font(middle, face);
    const int block = lh(face) + (worded ? lh(Font::BODY_LARGE) : 0);
    box(middle, x + stroke, y + (size - block) / 2, inner, lh(face));
    box(under, x + stroke, y + (size - block) / 2 + lh(face), inner, lh(Font::BODY_LARGE));
    show(middle, words);
    show(under, worded);
  }

  void paint(const Air &air, const std::string &verdict, const Palette &p) {
    for (int i = 0; i < n; ++i) {
      const Reading &r = air.r[slot[i]];
      const float from = 270 + gap / 2 + i * (span + gap);
      const float part = r.valid ? std::max(0.03f, r.q / 100) : 0;
      const int a = static_cast<int>(lroundf(from)) % 360, whole = static_cast<int>(lroundf(span));
      lv_arc_set_bg_angles(arc[i], a, (a + whole) % 360 == a ? a + 360 : (a + whole) % 360);
      const int lit = static_cast<int>(lroundf(span * part));
      lv_arc_set_angles(arc[i], a, n == 1 && lit >= 360 ? a + 360 : (a + std::max(1, lit)) % 360);
      lv_obj_set_style_arc_color(arc[i], p.track, LV_PART_MAIN);
      // What is fine is quiet, what is off is the darkest on the card; a card where all is fine wears its tone.
      lv_obj_set_style_arc_color(arc[i], r.level != GOOD || air.level == GOOD ? p.tone : p.mid, LV_PART_INDICATOR);
      lv_obj_set_style_arc_opa(arc[i], r.valid ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_INDICATOR);
    }
    char text[8];
    snprintf(text, sizeof(text), "%d", air.score);
    ui::set_text(middle, air.valid ? text : "");
    ink(middle, p.ink);
    ui::set_text(under, verdict);
    ink(under, p.soft);
  }
};

// One measure in its cell. What the cell holds follows its size (Face::variant).
struct Item {
  lv_obj_t *bg{}, *disc{}, *icon{}, *name{}, *value{}, *unit{}, *state{};
  Scale scale;
  bool scaled = false, detail = false;
  Font face = Font::TITLE, name_face = Font::BODY;
  int value_x = 0, value_y = 0, value_right = 0, value_room = 0;   // from value_x, or up to value_right when that is set
  int name_w = 0, x = 0, y = 0, w = 0, h = 0, measure = -1;

  void make(lv_obj_t *parent, bool with_scale, bool with_icon, bool with_disc, bool with_state = false) {
    // The cell holds its parts, and the disc its icon: each part's place is in its cell, from the cell's top left.
    bg = ui::block(parent, theme::TRACK);
    if (with_disc) disc = round_block(bg, theme::TRACK);
    if (with_icon) {
      icon = ui::label(disc ? disc : bg, Font::ICON_SMALL, theme::MUTED);
      lv_obj_set_style_text_align(icon, LV_TEXT_ALIGN_CENTER, 0);
    }
    if (with_state) state = ui::label(bg, Font::BODY, theme::MUTED);
    name = ui::label(bg, Font::BODY, theme::MUTED);
    value = ui::label(bg, Font::TITLE, theme::INK);
    unit = ui::label(bg, Font::BODY, theme::MUTED);
    scaled = with_scale;
    if (scaled) scale.make(bg);
  }
};

// The words of a tile or the card, and all its parts. The tile and the card are both one Face in another shape.
class Face {
 public:
  explicit Face(const AirQuality *plugin) : plugin_(plugin) {}
  Air air;

  void build(lv_obj_t *parent, int W, int H, bool detail, const char *title) {
    parent_ = parent, W_ = W, H_ = H, detail_ = detail;
    large_ = ui::large();
    g_ = ui::px(large_ ? 10 : 6);
    gc_ = ui::px(large_ ? 8 : 5);
    pad_ = ui::px(large_ ? 10 : 6);
    n_ = air.chosen;
    snprintf(title_, sizeof(title_), "%s", title ? title : "");
    if (!n_) {
      note_ = ui::text(parent, Font::BODY, theme::MUTED);
      lv_obj_set_style_text_align(note_, LV_TEXT_ALIGN_CENTER, 0);
      box(note_, 0, std::max(0, (H - 2 * lh(Font::BODY)) / 2), W, 2 * lh(Font::BODY));
      ui::set_text(note_, plugin_->text("choose"));
      return;
    }
    // On the card a panel in the verdict's colour holds the ring, the words and the key.
    if (detail) panel_ = ui::block(parent, theme::CARD);
    lv_obj_t *stage = detail ? panel_ : parent;
    hero_.make(stage, air);
    word_ = ui::label(stage, Font::TITLE, theme::INK);
    reason_ = ui::label(stage, Font::BODY, theme::MUTED);
    line_ = std::max(lh(Font::TITLE), lh(Font::ICON_SMALL));
    scale_h_ = std::max(3, ui::px(large_ ? 6 : 4));
    disc_ = lh(Font::ICON_SMALL) + ui::px(large_ ? 14 : 8);
    if (detail) return card();
    if (W >= ui::px(large_ ? 360 : 240) && H >= ui::px(large_ ? 150 : 100) && W * 10 >= H * 13 && split_view()) return;
    if (H >= ui::px(large_ ? 170 : 110) && band_view()) return;
    if (W >= ui::px(large_ ? 330 : 210) && wide_view()) return;
    stack_view();
  }

  // The data changed, or the look: every text and colour again. Positions that follow a text's width too.
  void paint() {
    if (!n_) {
      if (note_) ui::set_color(note_, theme::MUTED);
      return;
    }
    const bool any = air.valid > 0;
    // A tile's card wears the verdict's colour, and the card's panel the same one.
    const tessera::CardColor colour = any ? card_of(air.level) : tessera::CardColor::NONE;
    if (!detail_) ui::set_card_color(colour);
    const Palette p = palette_on(detail_ ? ui::card_color(colour) : ui::card_color(), air.level, any, theme::CARD);
    if (panel_) fill(panel_, p.ground);
    const std::string verdict = plugin_->text(!any ? "no_data" : air.level == GOOD ? "good" : air.level == FAIR ? "fair" : "poor");
    hero_.paint(air, verdict, p);
    // What is off, or that all is fine; the longest form that fits its room. Where the verdict stands inside the
    // ring, this is the headline.
    std::string why, advice;
    if (any && air.off) {
      why = fitted(off_words(air.worst, true), off_words(air.worst, false), reason_face_, reason_w_);
      const std::string key = std::string("advice_") + KEY[air.worst] +
                              (air.r[air.worst].two ? (air.r[air.worst].dir < 0 ? "_low" : "_high") : "");
      advice = plugin_->text(key.c_str());
      if (air.off > 1) {
        const std::string more = why + " " + tessera::format(plugin_->text("more"), air.off - 1);
        if (ui::text_width(more, reason_face_) <= reason_w_) why = more;
      }
    } else if (any) {
      why = fitted(tessera::format(plugin_->text("n_fine"), air.valid), plugin_->text("all_fine"), reason_face_, reason_w_);
      advice = plugin_->text("advice_fine");
    }
    if (ui::text_width(why, reason_face_) > reason_w_ && reason_face_ != Font::BODY && !headline_) {
      reason_face_ = Font::BODY;
      ui::set_font(reason_, Font::BODY);
    }
    ui::set_text(word_, verdict);
    ink(word_, p.ink);
    ui::set_text(reason_, why);
    ink(reason_, headline_ || (any && air.off) ? p.ink : p.soft);
    if (advice_) {
      ui::set_text(advice_, advice);
      ink(advice_, p.soft);
    }
    if (key_) {
      // The key only where something is off; without it the verdict stands in the middle of its panel.
      const bool on = any && air.off;
      show(key_, on);
      show(key_label_, on);
      fill(key_, p.ink);
      ink(key_label_, p.ground);
      if (words_y_ >= 0) {
        const int y = on ? words_y_ : words_mid_;
        lv_obj_set_y(word_, y);
        lv_obj_set_y(reason_, y + lh(Font::HEADLINE));
      }
    }
    ink(name_, p.soft);
    if (foot_) ink(foot_, detail_ ? p.soft : p.soft);
    // The measures: in their own order where all show, what is off first where some do not.
    uint8_t order[MEASURES];
    air.order(order, shown_ < n_);
    for (int i = 0; i < shown_; ++i) paint_item(items_[i], order[i], p);
  }

  bool has_card() const { return n_ > 0; }

  // What a tap on the card at (x, y) asks about: a measure, or -1 for nothing.
  int hit(int x, int y) const {
    if (key_ && !lv_obj_has_flag(key_, LV_OBJ_FLAG_HIDDEN) && x >= key_x_ - g_ && x < key_x_ + key_w_ + g_ && y >= key_y_ - g_ &&
        y < key_y_ + key_h_ + g_)
      return air.worst;
    for (int i = 0; i < shown_; ++i)
      if (x >= items_[i].x && x < items_[i].x + items_[i].w && y >= items_[i].y && y < items_[i].y + items_[i].h) return items_[i].measure;
    return -1;
  }

 private:
  const AirQuality *plugin_;
  lv_obj_t *parent_{}, *panel_{}, *note_{}, *word_{}, *reason_{}, *advice_{}, *name_{}, *foot_{}, *key_{}, *key_label_{};
  Hero hero_;
  Item items_[MEASURES];
  char title_[48] = "";   // the tile's name (a fixed field: the face lives in PSRAM with its tile)
  int W_ = 0, H_ = 0, g_ = 0, gc_ = 0, pad_ = 0, n_ = 0, shown_ = 0, reason_w_ = 0;
  int line_ = 0, scale_h_ = 0, disc_ = 0, key_x_ = 0, key_y_ = 0, key_w_ = 0, key_h_ = 0, foot_w_ = 0, words_y_ = -1, words_mid_ = 0;
  Font reason_face_ = Font::BODY;
  bool large_ = true, headline_ = false, detail_ = false;

  static std::string fitted(const std::string &longer, const std::string &shorter, Font f, int room) {
    return ui::text_width(longer, f) <= room ? longer : shorter;
  }
  std::string name_of(int measure, Font f, int room) const {
    if (measure == CO2 && !ui::covers(plugin_->text("co2"), f)) return plugin_->text("co2_plain");
    if (measure == TEMP && ui::text_width(plugin_->text("temperature"), f) > room) return plugin_->text("temperature_short");
    if (measure == HUM && ui::text_width(plugin_->text("humidity"), f) > room) return plugin_->text("humidity_short");
    if (measure == SWING && ui::text_width(plugin_->text("swing"), f) > room) return plugin_->text("swing_short");
    return plugin_->text(KEY[measure]);
  }
  // "CO2 high", "Too dry"; the short form is the measure's name alone.
  std::string off_words(int measure, bool full) const {
    if (!full) return name_of(measure, reason_face_, reason_w_);
    const Reading &r = air.r[measure];
    std::string key = std::string(KEY[measure]) + (r.dir < 0 ? "_low" : "_high");
    if (measure == CO2 && !ui::covers(plugin_->text("co2_high"), reason_face_)) key += "_plain";
    return plugin_->text(key.c_str());
  }
  std::string unit_of(const Reading &r, Font f) const {
    std::string u = r.unit;
    if (!ui::covers(u, f)) return u.find("g/m") != std::string::npos ? "ug/m3" : "";
    return u;
  }
  void named(int x, int y, int w, lv_text_align_t align) {
    name_ = ui::label(parent_, Font::BODY, theme::MUTED);
    lv_obj_set_style_text_align(name_, align, 0);
    box(name_, x, y, w, lh(Font::BODY));
    ui::set_text(name_, title_);
  }
  // The verdict and the line under it, from (x, y).
  void words(int x, int y, int w, Font word, Font reason, bool centred = false) {
    ui::set_font(word_, word);
    ui::set_font(reason_, reason);
    box(word_, x, y, w, lh(word));
    box(reason_, x, y + lh(word), w, lh(reason));
    const lv_text_align_t align = centred ? LV_TEXT_ALIGN_CENTER : LV_TEXT_ALIGN_LEFT;
    lv_obj_set_style_text_align(word_, align, 0);
    lv_obj_set_style_text_align(reason_, align, 0);
    reason_face_ = reason, reason_w_ = w;
  }
  // Where the verdict stands inside the ring: what is off as the headline, and what to do under it.
  void headline(int x, int y, int w, Font face, int advice_lines, bool centred = false) {
    headline_ = true;
    show(word_, false);
    ui::set_font(reason_, face);
    box(reason_, x, y, w, lh(face));
    reason_face_ = face, reason_w_ = w;
    advice_ = ui::text(parent_, Font::BODY, theme::MUTED);
    box(advice_, x, y + lh(face) + ui::px(2), w, advice_lines * lh(Font::BODY));
    if (centred) {
      lv_obj_set_style_text_align(reason_, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_style_text_align(advice_, LV_TEXT_ALIGN_CENTER, 0);
    }
  }
  int widest_word(Font f) const {
    int w = 0;
    for (const char *key : {"good", "fair", "poor"}) w = std::max(w, ui::text_width(plugin_->text(key), f));
    return w;
  }

  // What a cell of this size holds: 0 nothing fits, 1 one line (icon, name, value), 2 the name over the value,
  // 3 with its scale under them, 4 a roomy one with the icon in a disc and the value in the large face.
  int variant(int w, int h) const {
    const int lb = lh(Font::BODY), lt = lh(Font::TITLE);
    const int narrow = ui::px(large_ ? 70 : 52);
    if (w >= ui::px(large_ ? 150 : 104) && h >= 2 * pad_ + disc_ + lh(Font::VALUE) + scale_h_ + ui::px(large_ ? 22 : 12)) return 4;
    if (w >= narrow && h >= 2 * pad_ + lb + lt + scale_h_ + ui::px(large_ ? 3 : 2)) return 3;
    if (w >= narrow && h >= 2 * pad_ + lb + lt - ui::px(4)) return 2;
    if (w >= ui::px(large_ ? 128 : 92) && h >= line_ + ui::px(large_ ? 10 : 6)) return 1;
    return 0;
  }

  void cell(Item &it, int x, int y, int w, int h) {
    const int v = variant(w, h), lb = lh(Font::BODY), lt = lh(Font::TITLE), li = lh(Font::ICON_SMALL), p = pad_;
    const bool icon = v == 4 || v == 1 || w >= ui::px(large_ ? 104 : 76);
    it.make(parent_, v >= 3, icon, v == 4);
    it.detail = detail_;
    it.x = x, it.y = y, it.w = w, it.h = h;
    box(it.bg, x, y, w, h);
    x = y = 0;   // from here on, places in the cell
    if (v == 4) {
      const int in = p + ui::px(large_ ? 4 : 2);
      box(it.disc, x + in, y + in, disc_, disc_);
      box(it.icon, 0, (disc_ - li) / 2, disc_, li);
      it.name_w = w - 2 * in - disc_ - g_;
      box(it.name, x + in + disc_ + g_, y + in + (disc_ - lb) / 2, it.name_w, lb);
      it.face = Font::VALUE;
      it.value_x = x + in, it.value_room = w - 2 * in;
      it.value_y = y + h - in - scale_h_ - ui::px(large_ ? 14 : 8) - lh(Font::VALUE);
      it.scale.place(x + in, y + h - in - scale_h_ - ui::px(2), w - 2 * in, scale_h_);
    } else if (v == 1) {
      const int top = y + (h - line_) / 2;
      box(it.icon, x + p, top + (line_ - li) / 2, li, li);
      it.name_w = w / 2 - p - li;
      box(it.name, x + p + li + ui::px(large_ ? 8 : 5), top + (line_ - lb) / 2, it.name_w, lb);
      it.value_right = x + w - p, it.value_y = top + (line_ - lt) / 2, it.value_room = w / 2;
    } else {
      if (icon) box(it.icon, x + w - p - li, y + p + (lb - li) / 2, li, li);
      it.name_w = w - 2 * p - (icon ? li + ui::px(4) : 0);
      box(it.name, x + p, y + p, it.name_w, lb);
      const int under = v == 3 ? scale_h_ + ui::px(large_ ? 6 : 4) : 0;
      it.value_x = x + p, it.value_room = w - 2 * p;
      it.value_y = y + h - p - under - lt + (v == 3 ? 0 : ui::px(2));
      if (v == 3) it.scale.place(x + p, y + h - p - scale_h_ - ui::px(1), w - 2 * p, scale_h_);
    }
  }

  // The measures in the room (x0, y0, w, h): the grid whose cells hold the most and have the most pleasant shape, or
  // when not all fit, as many as do, what is off first.
  void cells(int x0, int y0, int w, int h) {
    const int cap = ui::px(detail_ ? (large_ ? 260 : 130) : (large_ ? 190 : 110));
    int cols = 0, rows = 0;
    float best = -1e9f;
    for (int c = 1; c <= std::min(n_, 4); ++c) {
      const int r = (n_ + c - 1) / c;
      const int cw = (w - (c - 1) * gc_) / c, ch = std::min(cap, (h - (r - 1) * gc_) / r);
      const int v = variant(cw, ch);
      if (!v) continue;
      const float score = v * 10 - 3 * std::fabs(std::log(static_cast<float>(cw) / ch / (v == 1 ? 5.0f : 1.7f))) - (n_ % c == 1 && c == 2 ? 6 : n_ % c ? 1.5f : 0);
      if (score > best) best = score, cols = c, rows = r;
    }
    shown_ = n_;
    if (!cols) {   // not all fit: the most that do
      int most = 0;
      for (int c = 1; c <= 4; ++c) {
        const int cw = (w - (c - 1) * gc_) / c;
        const int least = variant(cw, 2 * pad_ + lh(Font::BODY) + lh(Font::TITLE)) ? 2 * pad_ + lh(Font::BODY) + lh(Font::TITLE) - ui::px(4)
                          : variant(cw, line_ + ui::px(large_ ? 10 : 6)) ? line_ + ui::px(large_ ? 10 : 6) : 0;
        if (!least) continue;
        const int r = (h + gc_) / (least + gc_);
        if (std::min(n_, c * r) > most) most = std::min(n_, c * r), cols = c, rows = r;
      }
      if (!cols) return void(shown_ = 0);
      shown_ = most;
      rows = (shown_ + cols - 1) / cols;
    }
    const int ch = std::min(cap, (h - (rows - 1) * gc_) / rows);
    if (detail_) y0 += (h - rows * ch - (rows - 1) * gc_) / 2;   // on the card the cells stand in the middle of their room
    // How many stand in each row. A last row of one takes a neighbour from the row above it, so no cell is left
    // alone across the whole width.
    int in_row[MEASURES];
    for (int r = 0; r < rows; ++r) in_row[r] = std::min(cols, shown_ - r * cols);
    if (rows >= 2 && in_row[rows - 1] == 1 && in_row[rows - 2] >= 3) --in_row[rows - 2], ++in_row[rows - 1];
    for (int r = 0, i = 0; r < rows; ++r)
      for (int col = 0; col < in_row[r]; ++col, ++i) {
        const int cw = (w - (in_row[r] - 1) * gc_) / in_row[r];
        cell(items_[i], x0 + col * (cw + gc_), y0 + r * (ch + gc_), cw, ch);
      }
  }

  // One cell: the ring, with the verdict beside it or under it.
  void stack_view() {
    const int lb = lh(Font::BODY), lt = lh(Font::TITLE), lhd = lh(Font::HEADLINE);
    int size = std::min(H_, W_ * 46 / 100);
    const int beside = W_ - size - g_ - g_ / 2;
    if (beside >= widest_word(Font::TITLE) + ui::px(4)) {
      const Font word = beside >= widest_word(Font::HEADLINE) + ui::px(8) && lhd + lb <= H_ ? Font::HEADLINE : Font::TITLE;
      const bool with_name = *title_ && lb + lh(word) + lb <= H_;
      const int block = lh(word) + lb + (with_name ? lb : 0), top = (H_ - block) / 2, x = W_ - beside;
      hero_.place(0, (H_ - size) / 2, size);
      if (with_name) named(x, top, beside, LV_TEXT_ALIGN_LEFT);
      words(x, top + (with_name ? lb : 0), beside, word, Font::BODY);
    } else {
      size = std::min(W_, H_ - lt - lb - ui::px(4));
      hero_.place(0, 0, std::max(1, size));
      words(0, H_ - lb - lt, W_, Font::TITLE, Font::BODY);
    }
  }

  // One row of cells: the ring and the verdict at the left, then a cell for as many measures as fit.
  bool wide_view() {
    const int lb = lh(Font::BODY), lhd = lh(Font::HEADLINE), lt = lh(Font::TITLE);
    const int size = std::min(H_, ui::px(large_ ? 120 : 64));
    const bool big = lhd + 2 * lb <= H_;
    const Font word = big ? Font::HEADLINE : Font::TITLE;
    const int text_w = std::max(widest_word(word) + ui::px(6), ui::px(large_ ? 96 : 66));
    const int left = size + g_ + g_ / 2 + text_w, least = ui::px(large_ ? 72 : 54);
    const int room = W_ - left - g_;
    const int count = std::min(n_, (room + gc_) / (least + gc_));
    if (count < 1 || variant(least, H_) < 2) return false;
    hero_.place(0, (H_ - size) / 2, size);
    const bool with_name = *title_ && 2 * lb + lh(word) <= H_;
    const int top = (H_ - lh(word) - lb - (with_name ? lb : 0)) / 2, x = size + g_ + g_ / 2;
    if (with_name) named(x, top, text_w, LV_TEXT_ALIGN_LEFT);
    words(x, top + (with_name ? lb : 0), text_w, word, Font::BODY);
    (void) lt;
    const int cw = (room - (count - 1) * gc_) / count;
    shown_ = count;
    for (int i = 0; i < count; ++i) cell(items_[i], left + g_ + i * (cw + gc_), 0, cw, H_);
    return true;
  }

  // The ring and the verdict in a band on top, the measures under it.
  bool band_view() {
    const int lb = lh(Font::BODY), lt = lh(Font::TITLE), lhd = lh(Font::HEADLINE), lbl = lh(Font::BODY_LARGE);
    const int size = std::clamp(std::min(W_ * 42 / 100, H_ * 27 / 100), ui::px(large_ ? 58 : 38), ui::px(large_ ? 190 : 100));
    const int band_h = std::max(size, lt + lb);
    const int beside = W_ - size - g_ - g_ / 2, x = W_ - beside;
    const bool roomy = size >= lhd + lbl + (!*title_ ? 0 : lb) && beside >= widest_word(Font::HEADLINE) + ui::px(30);
    const Font word = roomy ? Font::HEADLINE : Font::TITLE, reason = roomy ? Font::BODY_LARGE : Font::BODY;
    if (H_ - band_h - g_ < line_ + ui::px(large_ ? 10 : 6)) return false;
    hero_.place(0, (band_h - size) / 2, size);
    const bool with_name = *title_ && lb + lh(word) + lh(reason) <= band_h;
    const int top = (band_h - lh(word) - lh(reason) - (with_name ? lb : 0)) / 2;
    if (with_name) named(x, top, beside, LV_TEXT_ALIGN_LEFT);
    words(x, top + (with_name ? lb : 0), beside, word, reason);
    cells(0, band_h + g_, W_, H_ - band_h - g_);
    return true;
  }

  // A tile lying down: the ring as large as the tile is high at the left, the verdict and the measures beside it. A
  // large ring holds the verdict itself, and the headline is then what is off, with what to do.
  bool split_view() {
    const int lb = lh(Font::BODY), lhd = lh(Font::HEADLINE), lbl = lh(Font::BODY_LARGE);
    const int size = std::min({H_, W_ * 38 / 100, ui::px(large_ ? 330 : 190)});
    const int x = size + 2 * g_, w = W_ - x;
    const bool worded = Hero::takes_word(size);
    const bool big = w >= ui::px(large_ ? 220 : 150) && H_ >= ui::px(large_ ? 250 : 150);
    hero_.place(0, (H_ - size) / 2, size, worded);
    int name_w = 0, head;
    if (*title_) {
      name_w = std::min(ui::text_width(title_, Font::BODY) + ui::px(4), w / 2);
      named(x + w - name_w, 0, name_w, LV_TEXT_ALIGN_RIGHT);
    }
    const int text_w = w - (name_w ? name_w + g_ : 0);
    if (worded) {
      const Font face = big ? Font::HEADLINE : Font::TITLE;
      headline(x, 0, text_w, face, 1);
      lv_obj_set_width(advice_, w);
      head = lh(face) + ui::px(2) + lb;
      if (name_) lv_obj_set_y(name_, drop(face, Font::BODY));
    } else {
      const Font word = big ? Font::HEADLINE : Font::TITLE, reason = big ? Font::BODY_LARGE : Font::BODY;
      words(x, 0, text_w, word, reason);
      lv_obj_set_width(reason_, w), reason_w_ = w;
      head = lh(word) + lh(reason);
      if (name_) lv_obj_set_y(name_, drop(word, Font::BODY));
    }
    (void) lhd, (void) lbl;
    const int top = head + g_;
    if (H_ - top < line_ + ui::px(large_ ? 10 : 6)) return false;
    cells(x, top, w, H_ - top);
    return true;
  }

  // The card: a panel in the verdict's colour with the ring, the verdict and a key that says what to do, and a cell
  // for every measure. A tap on the key or on a cell opens the popup that explains that measure.
  void card() {
    const int lb = lh(Font::BODY), lbl = lh(Font::BODY_LARGE);
    const int in = ui::px(large_ ? 18 : 10);
    foot_ = ui::label(parent_, Font::BODY, theme::MUTED);
    lv_obj_set_style_text_align(foot_, LV_TEXT_ALIGN_CENTER, 0);
    const std::string room = tessera::fill(plugin_->text("judged_as"), "room", plugin_->text((std::string("room_") + air.profile->id).c_str()));
    key_ = round_block(panel_, theme::INK);
    key_label_ = ui::label(key_, Font::BODY_LARGE, theme::CARD);
    lv_obj_set_style_text_align(key_label_, LV_TEXT_ALIGN_CENTER, 0);
    ui::set_text(key_label_, plugin_->text("what_to_do"));
    key_h_ = std::max(ui::mm(8), lbl + ui::px(large_ ? 16 : 10));
    key_w_ = ui::text_width(plugin_->text("what_to_do"), Font::BODY_LARGE) + 2 * ui::px(large_ ? 22 : 14);
    if (W_ * 10 >= H_ * 12) {   // lying down: the panel at the left
      const int left = W_ * 34 / 100, w = left - 2 * in;
      const bool small = H_ < ui::px(large_ ? 340 : 190);
      const Font word = small ? Font::TITLE : Font::HEADLINE, reason = small ? Font::BODY : Font::BODY_LARGE;
      box(panel_, 0, 0, left, H_);
      int size = std::min({w, H_ - 2 * in - g_ - lh(word) - g_ - key_h_, ui::px(large_ ? 300 : 170)});
      const bool worded = Hero::takes_word(size);
      const int text = lh(word) + (worded ? 0 : lh(reason));
      size = std::min(size, H_ - 2 * in - g_ - text - g_ - key_h_);
      int y = (H_ - size - g_ - text - g_ - key_h_) / 2;
      hero_.place((left - size) / 2, y, size, worded);
      y += size + g_;
      if (worded) {
        headline_ = true;
        show(word_, false);
        ui::set_font(reason_, word);
        box(reason_, in, y, w, lh(word));
        reason_face_ = word, reason_w_ = w;
        lv_obj_set_style_text_align(reason_, LV_TEXT_ALIGN_CENTER, 0);
      } else {
        words(in, y, w, word, reason, true);
      }
      key_x_ = (left - key_w_) / 2, key_y_ = y + text + g_;
      cells(left + g_, 0, W_ - left - g_, H_ - lb - g_ / 2);
      box(foot_, left + g_, H_ - lb, W_ - left - g_, lb);
      foot_w_ = W_ - left - g_;
    } else {                    // standing up: the panel on top
      const int size = std::clamp(std::min(W_ * 28 / 100, H_ * 22 / 100), ui::px(large_ ? 84 : 48), ui::px(large_ ? 200 : 110));
      const int x = in + size + in, beside = W_ - x - in, lhd = lh(Font::HEADLINE);
      const int block = lhd + lbl + g_ + key_h_;
      const int ph = std::max(size, block) + 2 * in;
      box(panel_, 0, 0, W_, ph);
      hero_.place(in, (ph - size) / 2, size);
      const int top = (ph - block) / 2;
      words(x, top, beside, Font::HEADLINE, Font::BODY_LARGE);
      key_x_ = x, key_y_ = top + lhd + lbl + g_;
      words_y_ = top, words_mid_ = (ph - lhd - lbl) / 2;
      cells(0, ph + g_, W_, H_ - ph - g_ - lb - g_ / 2);
      box(foot_, 0, H_ - lb, W_, lb);
      foot_w_ = W_;
    }
    box(key_, key_x_, key_y_, key_w_, key_h_);
    box(key_label_, 0, (key_h_ - lbl) / 2, key_w_, lbl);
    const std::string hint = room + " \u00B7 " + plugin_->text("tap_hint");
    ui::set_text(foot_, ui::text_width(hint, Font::BODY) <= foot_w_ ? hint : room);
  }

  void paint_item(Item &it, int measure, const Palette &face) {
    const Reading &r = air.r[measure];
    const bool is_off = r.valid && r.level != GOOD;
    it.measure = measure;
    // On a tile every cell is of the card's one colour family, and what is off is the strong one. On the card the
    // rows lie on the page, so each wears its own level.
    const Palette own = palette_of(r.level, r.valid, theme::CARD);
    const Palette &p = it.detail ? own : face;
    const lv_color_t ground = it.detail ? (is_off ? lv_color_mix(own.tone, ui::color(theme::CARD), 50) : ui::color(theme::CARD))
                                        : (is_off ? p.cell_off : p.cell);
    fill(it.bg, ground);
    ui::set_font(it.name, it.name_face);
    ui::set_text(it.name, name_of(measure, it.name_face, it.name_w));
    ink(it.name, it.detail ? p.ink : is_off ? p.ink : p.soft);
    if (it.disc) fill(it.disc, it.detail ? lv_color_mix(own.tone, ground, 64) : is_off ? p.ink : lv_color_mix(p.ink, ground, 26));
    if (it.icon) {
      ui::set_text(it.icon, ui::icon(ICON[measure]));
      // The icon wears its level's colour on the page's plain cells; on a tinted ground it is ink, which reads.
      // In a disc on the card it is the level's colour deepened with ink, which reads on the disc's tint.
      ink(it.icon, it.disc && it.detail ? lv_color_mix(p.ink, own.tone, 130)
                   : it.disc && is_off ? ground
                   : it.detail && !is_off ? own.tone : lv_color_mix(p.ink, ground, is_off ? 220 : 150));
    }
    if (it.state) {
      const char *key = !r.valid ? "no_data" : r.level == GOOD ? "good" : r.dir < 0 ? "word_low" : "word_high";
      ui::set_text(it.state, plugin_->text(key));
      ink(it.state, lv_color_mix(p.ink, ground, is_off ? 190 : 140));
    }
    // The value and its unit on one baseline, from the left or up to the right; the unit goes where both do not fit.
    const std::string value = r.valid ? r.text : "–";
    std::string unit = r.valid ? unit_of(r, Font::BODY) : "";
    ui::set_font(it.value, it.face);
    ui::set_text(it.value, value);
    ink(it.value, p.ink);
    const int vw = ui::text_width(value, it.face) + 2;
    const int space = unit.empty() || unit == "°" || unit == "%" ? 0 : ui::px(3);
    int uw = unit.empty() ? 0 : ui::text_width(unit, Font::BODY) + 2;
    if (it.value_room && vw + space + uw > it.value_room) unit.clear(), uw = 0;
    ui::set_text(it.unit, unit);
    ink(it.unit, lv_color_mix(p.ink, ground, is_off ? 190 : 160));
    const int x = it.value_right ? it.value_right - vw - (uw ? space + uw : 0) : it.value_x;
    box(it.value, x, it.value_y, vw, lh(it.face));
    box(it.unit, x + vw + space, it.value_y + drop(it.face, Font::BODY), std::max(1, uw), lh(Font::BODY));
    show(it.unit, uw > 0);
    if (it.scaled)
      it.scale.paint(r, lv_color_mix(p.ink, ground, 34), it.detail ? own.tone : is_off ? p.ink : p.tone, p.ink, ground);
  }
};

// ---------------------------------------------------------------------------------------------------------------------
// The popup that explains one measure: where it stands, where it comes from, a first step when it is off, and its
// last 24 hours
// ---------------------------------------------------------------------------------------------------------------------

// What the popup shows: the readings of the card or tile that opened it, and which measure. One popup shows at a time.
static Air popup_air;
static int popup_measure = 0;
static AirQuality *popup_plugin = nullptr;
// What the card shows: the air of the tile that opened it (a card gets the tile's data, not its options).
static Air card_air;

// A label of as many lines as its words take at this width; returns its height.
static int wrapped(lv_obj_t *label, int width) {
  lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(label, width);
  lv_obj_set_height(label, LV_SIZE_CONTENT);
  lv_obj_update_layout(label);
  return lv_obj_get_height(label);
}

class ExplainPopup : public tessera::Popup, public HistoryListener {
 public:
  static constexpr int HOURS = 24;

  ~ExplainPopup() override {
    if (plugin_) plugin_->forget(this);
  }

  void open(const tessera::PopupContext &c) override {
    plugin_ = popup_plugin;
    const int m = popup_measure;
    const Reading &r = popup_air.r[m];
    const bool large = ui::large(), is_off = r.valid && r.level != GOOD;
    const int g = ui::px(large ? 14 : 8), lb = lh(Font::BODY), lt = lh(Font::TITLE);
    const Font words = large ? Font::BODY_LARGE : Font::BODY;
    measure_ = m;
    source_ = m == SWING ? HUM : m;   // the swing is the humidity's own day
    width_ = c.width;
    // The first row: the measure's icon in a disc as large as the close key, its name, and how it stands.
    disc_ = round_block(c.parent, theme::TRACK);
    box(disc_, 0, 0, c.key, c.key);
    const Font glyph = lh(Font::ICON) <= c.key * 7 / 10 ? Font::ICON : Font::ICON_SMALL;
    icon_ = ui::label(disc_, glyph, theme::INK);
    lv_obj_set_style_text_align(icon_, LV_TEXT_ALIGN_CENTER, 0);
    box(icon_, 0, (c.key - lh(glyph)) / 2, c.key, lh(glyph));
    ui::set_text(icon_, ui::icon(ICON[m]));
    const int tx = c.key + g, tw = c.width - tx - c.corner;
    name_ = ui::label(c.parent, Font::TITLE, theme::INK);
    box(name_, tx, std::max(0, (c.key - lt - lb) / 2), tw, lt);
    std::string name = plugin_->text(KEY[m]);
    if (!ui::covers(name, Font::TITLE)) name = plugin_->text("co2_plain");
    ui::set_text(name_, name);
    state_ = ui::label(c.parent, Font::BODY, theme::MUTED);
    box(state_, tx, std::max(0, (c.key - lt - lb) / 2) + lt, tw, lb);
    std::string state = plugin_->text(!r.valid ? "no_data" : r.level == GOOD ? "good" : r.dir < 0 ? "word_low" : "word_high");
    if (r.valid) state += std::string(" · ") + worded(r.value);
    ui::set_text(state_, state);
    int y = std::max(c.key, lt + lb) + g;
    // Where it stands.
    scale_.make(c.parent);
    const int scale_h = std::max(4, ui::px(large ? 8 : 5));
    scale_.place(0, y + ui::px(3), c.width, scale_h);
    y += scale_h + ui::px(6) + g;
    // Where it comes from, and the first step when it is off.
    const std::string part = std::string(KEY[m]) + (r.two ? (r.dir < 0 ? "_low" : "_high") : "");
    lead_from_ = ui::label(c.parent, Font::BODY, theme::MUTED);
    box(lead_from_, 0, y, c.width, lb);
    ui::set_text(lead_from_, plugin_->text("lead_from"));
    y += lb + ui::px(2);
    from_ = ui::label(c.parent, words, theme::INK);
    ui::set_text(from_, plugin_->text(("cause_" + part).c_str()));
    lv_obj_set_pos(from_, 0, y);
    y += wrapped(from_, c.width) + g;
    lead_do_ = ui::label(c.parent, Font::BODY, theme::MUTED);
    box(lead_do_, 0, y, c.width, lb);
    ui::set_text(lead_do_, plugin_->text(is_off ? "lead_do" : "lead_now"));
    y += lb + ui::px(2);
    do_ = ui::label(c.parent, words, theme::INK);
    ui::set_text(do_, plugin_->text(is_off ? ("advice_" + part).c_str() : "in_range"));
    lv_obj_set_pos(do_, 0, y);
    y += wrapped(do_, c.width) + g;
    // Its last 24 hours, an hour a bar in the colour of how it stood then: asked for now, shown when the answer is
    // there (the popup grows with it), and left out where the glass has no room for it.
    bars_h_ = ui::px(large ? 54 : 32);
    if (y + lb + ui::px(4) + bars_h_ <= c.height) {
      lead_day_ = ui::label(c.parent, Font::BODY, theme::MUTED);
      box(lead_day_, 0, y, c.width / 2, lb);
      ui::set_text(lead_day_, plugin_->text("lead_day"));
      range_ = ui::label(c.parent, Font::BODY, theme::MUTED);
      lv_obj_set_style_text_align(range_, LV_TEXT_ALIGN_RIGHT, 0);
      box(range_, c.width / 2, y, c.width - c.width / 2, lb);
      bars_y_ = y + lb + ui::px(4);
      for (auto &b : bar_) {
        b = ui::block(c.parent, theme::TRACK);
        lv_obj_set_style_radius(b, std::max(1, ui::px(2)), 0);
        show(b, false);
      }
      show(lead_day_, false);
      show(range_, false);
      plugin_->listen(this);
      asked_ = plugin_->ask_history(popup_air.entity[source_]);
    }
    on_theme();
  }

  void on_history(const History &h) override {
    if (!asked_ || h.id != asked_ || !h.ok || !lead_day_) return;
    const int n = std::min<int>(h.count, HOURS), first = h.count - n;
    float low = h.mean[first], high = h.mean[first];
    for (int i = 0; i < n; ++i) low = std::min(low, h.mean[first + i]), high = std::max(high, h.mean[first + i]);
    const int gap = std::max(1, ui::px(2)), bw = std::max(2, (width_ - (HOURS - 1) * gap) / HOURS);
    const int left = width_ - (n * bw + (n - 1) * gap);   // the newest hour stands at the right edge
    for (int i = 0; i < HOURS; ++i) {
      show(bar_[i], i < n);
      if (i >= n) continue;
      const float v = h.mean[first + i];
      const float part = high - low > 1e-6f ? (v - low) / (high - low) : 0.5f;
      const int bh = std::max(ui::px(4), static_cast<int>(bars_h_ * (0.2f + 0.8f * part)));
      box(bar_[i], left + i * (bw + gap), bars_y_ + bars_h_ - bh, bw, bh);
      level_[i] = popup_air.judged(source_, v).level;
    }
    shown_ = n;
    ui::set_text(range_, worded(h.low, false) + " – " + worded(h.high));
    show(lead_day_, true);
    show(range_, true);
    on_theme();
  }

  void on_theme() override {
    const Reading &r = popup_air.r[measure_];
    const Palette p = palette_of(r.level, r.valid, theme::RAISED);
    const lv_color_t ground = ui::color(theme::RAISED);
    fill(disc_, lv_color_mix(p.tone, ground, 64));
    ink(icon_, p.tone);
    ui::set_color(name_, theme::INK);
    ui::set_color(state_, theme::MUTED);
    ui::set_color(lead_from_, theme::MUTED);
    ui::set_color(lead_do_, theme::MUTED);
    ui::set_color(from_, theme::INK);
    ui::set_color(do_, theme::INK);
    scale_.paint(r, lv_color_mix(p.ink, ground, 34), p.tone, p.ink, ground);
    if (!lead_day_) return;
    ui::set_color(lead_day_, theme::MUTED);
    ui::set_color(range_, theme::MUTED);
    for (int i = 0; i < shown_; ++i) fill(bar_[i], ui::tone_color(tone_of(static_cast<Level>(level_[i])), theme::MUTED));
  }

 private:
  AirQuality *plugin_{};
  lv_obj_t *disc_{}, *icon_{}, *name_{}, *state_{}, *lead_from_{}, *from_{}, *lead_do_{}, *do_{}, *lead_day_{}, *range_{};
  lv_obj_t *bar_[HOURS]{};
  uint8_t level_[HOURS]{};
  Scale scale_;
  uint32_t asked_ = 0;
  int measure_ = 0, source_ = 0, width_ = 0, bars_y_ = 0, bars_h_ = 0, shown_ = 0;

  // A value of the measure as words, with its unit.
  std::string worded(float value, bool with_unit = true) const {
    const Reading &r = popup_air.r[source_ == measure_ ? measure_ : source_];
    char text[16];
    snprintf(text, sizeof(text), source_ == TEMP || measure_ == TEMP ? "%.1f" : "%.0f", value);
    std::string unit = with_unit ? r.unit : "";
    if (!ui::covers(unit, Font::BODY)) unit = unit.find("g/m") != std::string::npos ? "ug/m3" : "";
    return std::string(text) + (unit.empty() || unit == "°" || unit == "%" ? "" : " ") + unit;
  }
};

static void explain(AirQuality *plugin, const Air &air, int measure) {
  if (measure < 0 || measure >= MEASURES) return;
  popup_air = air, popup_measure = measure, popup_plugin = plugin;
  tessera::show_popup(plugin, []() -> tessera::Popup * { return new ExplainPopup(); });
}

// ---------------------------------------------------------------------------------------------------------------------
// The tile and the card
// ---------------------------------------------------------------------------------------------------------------------

class RoomTile : public tessera::Tile, public HistoryListener {
 public:
  explicit RoomTile(AirQuality *plugin) : plugin_(plugin), face_(plugin) {}
  ~RoomTile() override { plugin_->forget(this); }

  void create(const tessera::TileContext &c) override {
    tile_ = c.tile;
    Air &air = face_.air;
    air.room(c.options["room"] | "living");
    for (int i = 0; i < MEASURES; ++i)
      if (i != SWING) snprintf(air.entity[i], sizeof(air.entity[i]), "%s", c.options[KEY[i]] | "");
    // A person's own limits: where fine ends, in the sensor's unit. An option left empty is not there.
    for (int i = 0; i < MEASURES; ++i) {
      const std::string key = KEY[i];
      air.high[i] = c.options[key + "_max"] | NAN;
      if (i == TEMP || i == HUM) air.low[i] = c.options[key + "_min"] | NAN;
    }
    air.choose();
    if (air.r[SWING].chosen) plugin_->listen(this);
    face_.build(c.parent, c.width, c.height, false, c.name);
  }
  void on_state(JsonObjectConst data) override {
    face_.air.read(data);
    face_.paint();
  }
  void on_tick(uint32_t epoch) override {
    if (settle_) {
      settle_ = false;
      face_.air.settle();
      face_.paint();
    }
    // Where the room judges how even its humidity is: the last 24 hours, asked again every ten minutes.
    if (face_.air.r[SWING].chosen && epoch && (!asked_at_ || epoch - asked_at_ >= 600)) {
      asked_at_ = epoch;
      asked_ = plugin_->ask_history(face_.air.entity[HUM]);
    }
  }
  void on_history(const History &h) override {
    if (!asked_ || h.id != asked_ || !h.ok) return;
    face_.air.swing = h.high - h.low;
    settle_ = true;   // painted in the tile's next tick: this is the plugin's call, not the tile's
  }
  void on_theme() override { face_.paint(); }
  void on_tap() override {
    if (!face_.has_card()) return;
    card_air = face_.air;
    tessera::open_card(plugin_->plugin_id(), "detail", "", tile_);
  }

 private:
  AirQuality *plugin_;
  Face face_;
  int tile_ = -1;
  uint32_t asked_ = 0, asked_at_ = 0;
  bool settle_ = false;
};

class DetailCard : public tessera::Card {
 public:
  explicit DetailCard(AirQuality *plugin) : plugin_(plugin), face_(plugin) {}

  void open(const tessera::CardContext &c) override {
    face_.air = card_air;
    face_.build(c.parent, c.width, c.height, true, "");
    face_.paint();
  }
  void on_state(JsonObjectConst data) override {
    face_.air.read(data);
    face_.paint();
  }
  void on_theme() override { face_.paint(); }
  void on_tap() override {
    const tessera::Point at = tessera::tap_point();
    explain(plugin_, face_.air, face_.hit(at.x, at.y));
  }

 private:
  AirQuality *plugin_;
  Face face_;
};

// ---------------------------------------------------------------------------------------------------------------------
// The plugin
// ---------------------------------------------------------------------------------------------------------------------

void AirQuality::setup() {
  add_tile("room", [this]() { return new RoomTile(this); });
  add_card("detail", [this]() { return new DetailCard(this); }, true);
}

uint32_t AirQuality::ask_history(const char *entity) {
  const uint32_t now = tessera::epoch();
  if (!now || !entity || !*entity) return 0;
  // From the start of the hour a day ago, in UTC: the recorder keeps a mean, a low and a high for every hour.
  const time_t from = static_cast<time_t>(now) - 24 * 3600;
  struct tm utc;
  gmtime_r(&from, &utc);
  char start[24];
  strftime(start, sizeof(start), "%Y-%m-%dT%H:00:00Z", &utc);
  JsonDocument request;
  request["ask"] = "recorder/statistics_during_period";
  JsonObject data = request["data"].to<JsonObject>();
  data["start_time"] = start;
  data["statistic_ids"].to<JsonArray>().add(entity);
  data["period"] = "hour";
  JsonArray types = data["types"].to<JsonArray>();
  types.add("mean");
  types.add("min");
  types.add("max");
  return tessera::send(this, request.as<JsonObjectConst>());
}

void AirQuality::on_message(JsonObjectConst message) {
  History h;
  h.id = message["re"] | 0u;
  if (!h.id) return;
  if (message["ok"] | false) {
    JsonObjectConst result = message["result"];
    for (JsonVariantConst v : result["mean"].as<JsonArrayConst>())
      if (h.count < sizeof(h.mean) / sizeof(h.mean[0]) && v.is<float>()) h.mean[h.count++] = v.as<float>();
    if (h.count) {
      h.low = h.high = h.mean[0];
      for (int i = 0; i < h.count; ++i) h.low = std::min(h.low, h.mean[i]), h.high = std::max(h.high, h.mean[i]);
      for (JsonVariantConst v : result["min"].as<JsonArrayConst>())
        if (v.is<float>()) h.low = std::min(h.low, v.as<float>());
      for (JsonVariantConst v : result["max"].as<JsonArrayConst>())
        if (v.is<float>()) h.high = std::max(h.high, v.as<float>());
    }
    h.ok = h.count >= 2;
  }
  for (HistoryListener *l : listeners_)
    if (l) l->on_history(h);
}

void AirQuality::listen(HistoryListener *listener) {
  for (auto *l : listeners_)
    if (l == listener) return;
  for (auto &l : listeners_)
    if (!l) {
      l = listener;
      return;
    }
}

void AirQuality::forget(HistoryListener *listener) {
  for (auto &l : listeners_)
    if (l == listener) l = nullptr;
}

}  // namespace esphome::air_quality
