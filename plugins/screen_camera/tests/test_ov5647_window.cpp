// clang++ -std=c++17 -Wall -Wextra -Werror -I components tests/test_ov5647_window.cpp -o /tmp/test_ov5647_window && /tmp/test_ov5647_window
// (from plugins/screen_camera). The OV5647's registers for its colour order (components/ov5647/ov5647_window.h): the
// ISP's mirror bit, the window one pixel on for each flip, and the format's own window across a start and a turn.
#include <cstdio>

#include "ov5647/ov5647_window.h"

using namespace esphome::ov5647;

static int failures = 0;
static void expect(bool ok, const char *what) {
  if (!ok) {
    std::printf("FAIL %s\n", what);
    ++failures;
  }
}

int main() {
  // The formats' 0x03 gets the ISP's mirror; the sensor's own bits stay as they are.
  expect(mirror_register(0x03) == 0x07, "0x03 becomes 0x07");
  expect(mirror_register(0x01) == 0x05, "0x01 becomes 0x05");
  expect(mirror_register(0x07) == 0x07, "0x07 stays");

  // As measured: upright at the format's 8, 0; turned half way round (both flips) at 9, 1.
  const Window own{8, 0};
  expect(window_for(own, false, false) == Window{8, 0}, "upright: the format's own");
  expect(window_for(own, true, true) == Window{9, 1}, "turned half way round: a pixel on each way");
  expect(window_for(own, true, false) == Window{8, 1}, "upside down only: a row on");
  expect(window_for(own, false, true) == Window{9, 0}, "mirrored only: a column on");

  // A start hands the sensor its format: what it holds is the format's own window.
  expect(own_window(Window{8, 0}, false, Window{0, 0}, Window{0, 0}) == own, "the first start");
  // A turn without a start: it still holds the window written last, so the own one is the one known.
  expect(own_window(Window{9, 1}, true, Window{9, 1}, own) == own, "a turn after a turn");
  // A start after a turn: the format wrote its own again, whatever was written before.
  expect(own_window(Window{8, 0}, true, Window{9, 1}, own) == own, "a start after a turn");
  // A format with another window (another mode): that one.
  expect(own_window(Window{16, 4}, true, Window{9, 1}, own) == (Window{16, 4}), "another format's own");

  if (failures == 0) std::printf("ok\n");
  return failures == 0 ? 0 : 1;
}
