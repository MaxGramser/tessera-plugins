# Screen camera

See the screen's own camera in Home Assistant. The screen becomes a camera of its device there: a picture on a
dashboard, a stream when you open it, and anything Home Assistant does with a camera (a snapshot in an automation, a
recording).

The picture is made on the screen in hardware, only while Home Assistant asks for one. While it does, a camera shows in
the screen's top bar, so whoever stands in front of it knows.

## What you need

- A screen whose board has a camera that Tessera knows how to power and reach: the reTerminal D1001.
- Nothing else in Home Assistant: the camera comes with the screen's device in the ESPHome integration.

## Set up

1. In Tessera, open **Plugins**, choose **Screen camera** and tick your screens. Each screen builds once with it.
2. In Home Assistant, open the screen's device (Settings, Devices): its camera is there as **Camera**. Put it on a
   dashboard with a Picture entity or a Picture glance card.
3. Optional: add **Camera in use** to a page's top bar (Top bar, Add, From plugins). It shows while pictures leave the
   screen: "Live" for a stream, "Camera" for a picture now and then.

## Privacy

Nothing leaves the screen until Home Assistant asks for a picture, and every picture goes only to your Home Assistant.
Everyone who can open the screen's device or a dashboard with its camera can see through it. To stop that for a while,
disable the camera entity in Home Assistant; to stop it for good, remove the plugin from the screen.

## For makers

The camera is ESPHome's camera with id `ts_camera` (the feature `camera`), so another plugin can build on it. The
camera driver is `esp_video_camera` (github.com/n-IA-hane/esphome-esp-video-camera, from ESPHome pull request
esphome/esphome#16944), copied unchanged into `components/esp_video_camera` under the ESPHome License, with its NOTICE.
