#pragma once
// What the OV5647's registers hold for the picture Espressif's esp_cam_sensor promises: its colours in the order its
// formats declare (GBRG), however the camera flips the sensor. Pure, so a PC tests it (tests/test_ov5647_window.cpp).
//
// - 0x3821 has two mirror bits: bit 1 mirrors the sensor's readout, bit 2 the ISP's. esp_cam_sensor's formats set bit 1
//   alone (0x03), and the colours come out of the P4's image processor in the wrong order (a purple picture, orange as
//   green); with bit 2 as well (0x07, as Linux's ov5647.c starts it) they are right. The picture does not mirror.
// - A flip (bit 1 of 0x3820 for upside down, bit 1 of 0x3821 for mirrored) moves the readout by a row or a column, and
//   the colour order with it, while the image processor keeps the format's: with both flips red and blue swap. The ISP's
//   window, one pixel on for each flip (0x3811 for columns, 0x3813 for rows), brings the format's order back.
// Measured on a Waveshare ESP32-P4-WIFI6-Touch-LCD-7B with its OV5647 (800x800 RAW8, 2026-11-01): unturned with 0x3821
// 0x07 and the window at 8, 0; turned half way round (both flips) with the window at 9, 1 (7, 1 as well).
#include <cstdint>

namespace esphome::ov5647 {

static constexpr uint8_t FLIP_BIT = 0x02;        // 0x3820 and 0x3821: the sensor's own flip and mirror
static constexpr uint8_t ISP_MIRROR_BIT = 0x04;  // 0x3821: the ISP's mirror, which the formats leave out

struct Window {
  uint16_t x;  // 0x3810 and 0x3811
  uint16_t y;  // 0x3812 and 0x3813
  bool operator==(const Window &other) const { return x == other.x && y == other.y; }
  bool operator!=(const Window &other) const { return !(*this == other); }
};

// 0x3821 with the ISP's mirror bit as well as whatever else it holds.
inline uint8_t mirror_register(uint8_t now) { return now | ISP_MIRROR_BIT; }

// The window for the sensor's flips, from its format's own.
inline Window window_for(Window own, bool upside_down, bool mirrored) {
  return {uint16_t(own.x + (mirrored ? 1 : 0)), uint16_t(own.y + (upside_down ? 1 : 0))};
}

// The format's own window: what the sensor holds now, unless that is the window written last, while no format came
// since (a turn, not a start), and the own one is the one known before.
inline Window own_window(Window now, bool wrote, Window written, Window own_before) {
  return wrote && now == written ? own_before : now;
}

}  // namespace esphome::ov5647
