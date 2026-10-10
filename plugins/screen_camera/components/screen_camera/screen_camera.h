#pragma once
#include <cstdint>
#include <memory>

#include "esphome/core/component.h"
#include "esphome/components/camera/camera.h"
#include "esphome/components/smart_display/plugin_api.h"

namespace esphome::screen_camera {

// The plugin: an item for the top bar that shows while pictures leave the screen's camera, in red while it streams. It listens to the camera as
// ESPHome's API does (camera::CameraListener), on the main loop: a picture, or a stream that starts or stops.
class ScreenCamera : public Component, public tessera::Plugin, public camera::CameraListener {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::LATE; }
  void on_interval(uint32_t now_ms) override;

  void set_camera(camera::Camera *c) { camera_ = c; }

  void on_camera_image(const std::shared_ptr<camera::CameraImage> &image) override;
  void on_stream_start() override;
  void on_stream_stop() override;

  // Whether someone is watching: a picture left the screen a moment ago. Live: a stream runs and its pictures still
  // come, so a stream that ends without a word from the API (a viewer that went away) goes quiet by itself.
  bool watching() const;
  bool live() const;

 protected:
  camera::Camera *camera_{};
  bool streaming_{false};
  bool shown_{false};          // what the top bar shows now, so a change draws it at once
  uint32_t last_picture_{0};   // millis() of the last picture, 0 before the first
  uint32_t last_stream_{0};    // millis() of the last sign of a stream: a request for it or one of its pictures
};

}  // namespace esphome::screen_camera
