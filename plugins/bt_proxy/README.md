# Bluetooth proxy

> **Warning: the proxy costs the screen's responsiveness.** Wi-Fi and Bluetooth share one radio, and the screen already
> asks a lot of its Wi-Fi. With the proxy on, taps and pages can answer more slowly, the Wi-Fi gets weaker, and live
> links such as camera streams and video can stutter or drop. Add it only where you can live with that, and turn it off
> with the setting **Bluetooth proxy** when the screen suffers. A separate ESP32 as a Bluetooth proxy costs a few euros
> and takes nothing from the screen.

Turns the screen into a Bluetooth proxy for Home Assistant, the same one a ready-made ESPHome Bluetooth proxy runs.
Home Assistant then hears Bluetooth devices near the screen (thermometers, plant sensors, tags for presence with
[Bermuda](https://github.com/agittins/bermuda)) and can connect to some of them through it (a lock, a light), up to
three at a time.

## Before you add it: Wi-Fi

The screen does Wi-Fi and Bluetooth with one radio, so every moment it listens for Bluetooth is a moment without
Wi-Fi. The plugin listens the way ESPHome's own Wi-Fi proxies do, a short slice of time at a steady pace, but the
screen still has less Wi-Fi than before:

- Pictures, covers and camera streams can load more slowly. One tester saw a camera stream run clearly smoother with
  the proxy off.
- On weak Wi-Fi the screen can react more slowly to Home Assistant, or drop its connection more often.

Turn the proxy off at any time with the setting **Bluetooth proxy** (below), without a build. When the screen shows
camera streams, or hangs on the edge of your Wi-Fi, a separate ESP32 as a Bluetooth proxy is the better choice: it costs
a few euros and you can put it where the reception is best.

## Which screens

Every screen with an ESP32-P4 (the Guition P4 screens, the Waveshare P4 86, the M5Stack Tab5, the reTerminal D1001,
the CrowPanel 10.1). The P4 runs Bluetooth through the ESP32-C6 beside it, which also does its Wi-Fi, and has the
memory to spare. The app only offers the plugin on those screens.

Not on an ESP32-S3 with an RGB panel (the 4-inch Guition, the Waveshare 4.3, 5 and 7-inch, and their kind): Bluetooth
needs memory inside the chip that the panel and Wi-Fi already use, and such a screen stops connecting. Not on the CYD
and other 4 MB screens either: there is no room for it.

## What you need

- A screen with an ESP32-P4.
- A recent firmware on its ESP32-C6, the chip beside the P4 that does Wi-Fi and Bluetooth. Many screens come with one
  too old for Bluetooth (a reTerminal D1001 came with 2.3.2). The plugin brings the update with it, see "Updating the
  C6" below. With the old firmware the screen works as before and Bluetooth stays off.

## Set up

1. In Tessera, open **Plugins**, choose **Bluetooth proxy** and tick your screen. Leave **Wi-Fi chip update** ticked.
2. Add it. The screen builds once.
3. In Home Assistant, open the screen's device. **Bluetooth status** says what the proxy does. **Listening**: done.
   **Update the Wi-Fi chip**: the C6 firmware is too old, follow "Updating the C6" below.
4. In Home Assistant, **Settings**, **Devices & services**, **Bluetooth** shows the screen as a proxy, named after the
   screen and the Bluetooth address of its C6 (not the Wi-Fi address of the screen), and the devices it hears as Home
   Assistant finds them.

## On the screen and in the app

**Bluetooth proxy** turns Bluetooth on and off, and **Bluetooth status** says what it does: listening, off, starting, or
that the Wi-Fi chip needs its update. Both are in Home Assistant on the screen's device, and on the screen itself: hold
the top bar, then **Settings**, **Plugins**, **Bluetooth**. The switch is also in the plugin's details on the screen's
Plugins tab in Tessera. Off gives the radio to Wi-Fi alone; the screen keeps it off across a restart until you turn it
on.

## Updating the C6

The ESP32-C6 has a firmware of its own, and ESPHome publishes newer ones. With **Wi-Fi chip update** ticked, the screen's
device in Home Assistant has **Wi-Fi co-processor firmware**, with the version on the C6 and the newest one. It never
installs by itself: an update that goes wrong leaves the C6 starting over and over, and only its download mode brings it
back, which on some screens means opening the case. So it waits for you:

1. In Home Assistant, open the screen's device and **Wi-Fi co-processor firmware**.
2. Press **Install**, and leave the screen powered until it is done (a few minutes). The screen restarts, and
   **Bluetooth status** says **Listening**.

It checks for a newer version every six hours and shows it there; installing stays your choice. Do not want it? Untick
**Wi-Fi chip update** in the plugin's details on the screen's Plugins tab and choose **Save and build**.

## Good to know

- It adds about 370 KB to the firmware, and the Wi-Fi chip update about 200 KB more. Every P4 screen has room for that.
- The proxy follows ESPHome's own: what Home Assistant does with it is in the
  [ESPHome Bluetooth proxy docs](https://esphome.io/components/bluetooth_proxy/).
- Added a Bluetooth proxy or the C6 update through the screen's **Override YAML** before? Remove those blocks
  (`esp32_ble_tracker:`, `bluetooth_proxy:`, `http_request:` and the `esp32_hosted` update) first, or the build finds
  them twice. The update keeps its name, so Home Assistant keeps its entity. If that entity says unavailable after the
  build, reload the screen in Home Assistant (**Settings**, **Devices & services**, **ESPHome**, the screen's three dots,
  **Reload**).
- Tried on a reTerminal D1001: its C6 went from 2.3.2 to 2.12.12 with Install, and the proxy heard 51 devices in its
  first minute. With 0.2.0, built by the Tessera app from the plugin with **Wi-Fi chip update**, the screen shows
  **Bluetooth status** Listening and the update as up to date.
