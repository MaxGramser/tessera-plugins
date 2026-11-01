#include "ov5647.h"

#include "esphome/core/log.h"
#include "esphome/components/esp_video_camera/camera_sensor.h"

namespace esphome::ov5647 {

static const char *const TAG = "ov5647";

// The registers of esp_cam_sensor's ov5647_regs.h and its format tables.
static constexpr uint16_t REG_ID_HIGH = 0x300a;
static constexpr uint16_t REG_ID_LOW = 0x300b;
static constexpr uint16_t OV5647_ID = 0x5647;
static constexpr uint16_t REG_FLIP = 0x3820;
static constexpr uint16_t REG_MIRROR = 0x3821;
static constexpr uint16_t REG_WINDOW_X = 0x3810;  // high byte; the low byte follows, then the rows' two
static constexpr uint16_t REG_WINDOW_Y = 0x3812;

static OV5647 *found_sensor = nullptr;
static void camera_controls_set() {
  if (found_sensor != nullptr) found_sensor->controls_set();
}
// Its registers after the camera's controls only: its frame rate and field of view stay its format's.
static const tessera_camera_sensor_t CAMERA_SENSOR = {"OV5647", nullptr, nullptr, nullptr, nullptr, camera_controls_set};

bool OV5647::read_(uint16_t reg, uint8_t &value) { return this->read_register16(reg, &value, 1) == i2c::ERROR_OK; }
bool OV5647::write_(uint16_t reg, uint8_t value) { return this->write_register16(reg, &value, 1) == i2c::ERROR_OK; }

void OV5647::setup() {
  uint8_t high = 0, low = 0;
  if (!this->read_(REG_ID_HIGH, high) || !this->read_(REG_ID_LOW, low)) {
    ESP_LOGD(TAG, "No sensor answers at 0x%02X", this->address_);
    return;
  }
  if ((uint16_t(high) << 8 | low) != OV5647_ID) {
    ESP_LOGD(TAG, "Sensor id 0x%02X%02X is not the OV5647: its registers left alone", high, low);
    return;
  }
  this->found_ = true;
  found_sensor = this;
  tessera_camera_add_sensor(&CAMERA_SENSOR);
}

void OV5647::controls_set() {
  if (!this->found_) return;
  uint8_t flip = 0, mirror = 0, x_high = 0, x_low = 0, y_high = 0, y_low = 0;
  if (!this->read_(REG_FLIP, flip) || !this->read_(REG_MIRROR, mirror) || !this->read_(REG_WINDOW_X, x_high) ||
      !this->read_(REG_WINDOW_X + 1, x_low) || !this->read_(REG_WINDOW_Y, y_high) ||
      !this->read_(REG_WINDOW_Y + 1, y_low)) {
    if (!this->failed_) ESP_LOGW(TAG, "The sensor did not answer: its colours may come out in the wrong order");
    this->failed_ = true;
    return;
  }
  this->failed_ = false;
  const bool upside_down = flip & FLIP_BIT, mirrored = mirror & FLIP_BIT;
  const Window now{uint16_t(x_high << 8 | x_low), uint16_t(y_high << 8 | y_low)};
  this->own_ = own_window(now, this->wrote_, this->written_, this->own_);
  const Window want = window_for(this->own_, upside_down, mirrored);
  bool ok = true;
  if (mirror_register(mirror) != mirror) ok = this->write_(REG_MIRROR, mirror_register(mirror));
  if (want.x != now.x)
    ok = this->write_(REG_WINDOW_X, want.x >> 8) && this->write_(REG_WINDOW_X + 1, want.x & 0xff) && ok;
  if (want.y != now.y)
    ok = this->write_(REG_WINDOW_Y, want.y >> 8) && this->write_(REG_WINDOW_Y + 1, want.y & 0xff) && ok;
  this->written_ = want;
  this->wrote_ = true;
  const int flips = (upside_down ? 1 : 0) | (mirrored ? 2 : 0);
  if (!ok) {
    ESP_LOGW(TAG, "Its registers did not all take: its colours may come out in the wrong order");
  } else if (flips != this->flips_) {
    ESP_LOGI(TAG, "%s%s, its window at %u, %u for the colours", upside_down ? "Upside down" : "Upright",
             mirrored ? " and mirrored" : "", (unsigned) want.x, (unsigned) want.y);
  }
  this->flips_ = ok ? flips : -1;
}

void OV5647::dump_config() {
  ESP_LOGCONFIG(TAG, "OV5647 colour order:");
  if (!this->found_) {
    ESP_LOGCONFIG(TAG, "  No OV5647 on this screen");
    return;
  }
  ESP_LOGCONFIG(TAG, "  Found: the ISP's mirror bit and the window follow the camera's flips");
}

}  // namespace esphome::ov5647
