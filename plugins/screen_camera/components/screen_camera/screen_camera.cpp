#include "screen_camera.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::screen_camera {

// Tessera's camera icon.
static constexpr uint32_t ICON_CAMERA = 0xF07AE;   // cctv
// A picture keeps the item in the top bar this long: Home Assistant's dashboards ask for a new one every ten seconds.
static constexpr uint32_t PICTURE_SHOWN_MS = 12000;
// A stream is live while its pictures come; at five a second, a gap this long means it ended.
static constexpr uint32_t STREAM_QUIET_MS = 3000;
static const char *const TAG = "screen_camera";

void ScreenCamera::setup() {
  if (camera_ != nullptr) camera_->add_listener(this);
  add_bar_item("watching", [this]() {
    tessera::BarItem item;
    if (watching()) {
      item.shown = true;
      item.icon = ICON_CAMERA;
      // A stream lights the camera up in red, as a camera that records; a picture now and then keeps the bar's grey.
      item.tone = live() ? tessera::Tone::ALERT : tessera::Tone::NORMAL;
      item.text = text(live() ? "live" : "picture");
    }
    return item;
  });
}

bool ScreenCamera::watching() const {
  return last_picture_ != 0 && millis() - last_picture_ < PICTURE_SHOWN_MS;
}

bool ScreenCamera::live() const {
  return streaming_ && millis() - last_stream_ < STREAM_QUIET_MS;
}

void ScreenCamera::on_camera_image(const std::shared_ptr<camera::CameraImage> &image) {
  last_picture_ = millis();
  if (last_picture_ == 0) last_picture_ = 1;
  last_stream_ = last_picture_;
  if (!shown_) {
    shown_ = true;
    tessera::refresh();
  }
}

void ScreenCamera::on_stream_start() {
  if (!streaming_) ESP_LOGD(TAG, "Stream started");
  streaming_ = true;
  last_stream_ = millis();
}

void ScreenCamera::on_stream_stop() {
  if (streaming_) ESP_LOGD(TAG, "Stream stopped");
  streaming_ = false;
  tessera::refresh();
}

void ScreenCamera::on_interval(uint32_t now_ms) {
  // The item goes as soon as the last picture is old, not at the bar's next look; a stream that went quiet is no
  // longer live.
  if (shown_ && !watching()) {
    shown_ = false;
    tessera::refresh();
  }
  if (streaming_ && !live()) {
    ESP_LOGD(TAG, "Stream went quiet");
    streaming_ = false;
    tessera::refresh();
  }
}

}  // namespace esphome::screen_camera
