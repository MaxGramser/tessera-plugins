# Changelog

## 1.1.0 - 2026-10-09
- Brings the panel's speaker and microphone for other plugins: a plugin that needs one brings this one along.
- Their ids are now `ts_speaker` and `ts_microphone`. If your screen's Override YAML names `p4_speaker` or
  `p4_microphone`, change it to the new ids.
- Listed under the topic Voice & sound, on the Plugins page's Hardware tab.
- Needs a Tessera with plugin API 0.7: update the app and the screen first.

## 1.0.3 - 2026-10-09
- The README says where the settings are now (the plugin's details on the screen's Plugins tab), and that echo
  cancellation is not in this plugin.

## 1.0.2 - 2026-10-08
- Shows as Beta in the app.

## 1.0.1 - 2026-10-07
- A test that runs is lit on the screen's settings page.

## 1.0.0 - 2026-10-07
- First version: the speaker and microphone of the Waveshare ESP32-P4 86 panel, a click on every tap, a speaker
  volume, a switch that turns the microphone off, and a test for each on the screen's settings page.
