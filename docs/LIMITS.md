# What a plugin can and cannot do

## Works

| Idea | How |
|---|---|
| The next bus from your stop, with your line chosen in the editor | A tile, a `fetch`, an option with `options_from` (the `ov_departures` plugin). |
| Today's waste collection from your municipality's API | A tile and a `fetch`. |
| Hourly energy prices of your own supplier | A `fetch` (up to 48 items) and a tile that draws them. |
| A day of electricity prices per quarter of an hour from Home Assistant (Nord Pool, ENTSO-e, Tibber, ...) | A tile with `domains`, `has_attributes` and `fields` with `as: numbers` for a sensor's attributes, or `answers` for an action's answer. |
| A countdown to a date | A tile on the screen's clock alone (the template). |
| A sensor or a relay on the board's free pins | An ESPHome component with `inputs` of kind `gpio`; anything ESPHome can do. |
| Waking the screen when someone stands in front of it | A plugin with a radar sensor and `on_interval`. |
| Settings a person changes in the editor, a test with its result ("Heard: Okay Nabu") | ESPHome entities in `plugin.yaml` named in `settings`; a button with a `status`. |
| Talking to an AI model or a voice assistant | Home Assistant's Assist and its conversation agents (OpenAI, Anthropic); the plugin picks the assistant, the key stays in Home Assistant. |

## Works with a way around

| Idea | Way around |
|---|---|
| An API that needs OAuth | A Home Assistant integration that offers the data as an entity; then a normal tile. |
| An API that gives XML or GTFS-realtime | The Home Assistant integration for it. |
| Messages from outside (a webhook, a bot) | A webhook in Home Assistant that sets an entity. |
| A server, an account or a live connection of the plugin's own (a realtime voice session, a cloud service with OAuth) | A Home Assistant integration of your own; the plugin uses its entities and actions. |

## Cannot, on purpose

| Idea | Why |
|---|---|
| Change how all light tiles look | A plugin draws only its own tiles. A better card for everyone belongs in Tessera itself. |
| Block taps (a child lock) or scroll freely | Touch and navigation stay with the core. |
| Run code in the Tessera app or the editor | The app holds the key to all of Home Assistant; it runs only its own code. |
| Reach your home network through the app | A fetch goes only to public hosts the manifest names. |
| Fetch from the screen itself | The app does it once for every screen, without holding up drawing. |
| Set the screen's Wi-Fi, API, updates or log, or open a port on it | They belong to the core and the screen's own YAML (below, "plugin.yaml"). |
| Change without a build | A plugin is part of the firmware: every change builds the screen. |
| Live video | Pictures reach a screen as snapshots. |

## Cannot, yet

Not in the plugin API so far: a picture of the plugin's own on a tile or a card (pictures reach a screen through
Tessera's own picture route), a message to the app other than a Home Assistant command its manifest names
(`permissions.ha_commands`), and a Python part of a plugin in the app (the app runs only its own code, see above). A
settings page on the screen, a card, top bar items, tap actions, tiles of an entity and questions to the app are all
there ([FIRMWARE_API.md](FIRMWARE_API.md)).

## plugin.yaml

`plugin.yaml` and the file of every part are merged into the screen's own configuration. `tools/check.py` refuses a
file that sets one of these keys at its top level:

| Keys | Why |
|---|---|
| `esphome`, `esp32`, `esp8266`, `rp2040`, `psram` | The chip, the board and how it starts belong to the core. |
| `wifi`, `ethernet`, `network`, `improv_serial`, `esp32_improv` | How the screen joins the network is in the screen's own YAML. |
| `api`, `ota`, `logger` | Home Assistant's connection, updates and the log belong to the screen's own YAML and the core. |
| `external_components`, `packages`, `substitutions` | The app writes these for every plugin; a plugin's inputs reach `plugin.yaml` as substitutions already. |
| `web_server`, `captive_portal`, `mqtt`, `wireguard`, `socket`, `udp`, `packet_transport` | They open the screen to the network. |
| `http_request`, `online_image` | They fetch from the screen; data comes through the app's `fetch`. |

Two more rules for the ids it makes: one that starts with `ts_` only for a feature the plugin provides, and for each
feature it provides, that feature's component with exactly its id ([MANIFEST.md](MANIFEST.md), "Features").

## Physical limits

- **Flash**: a 4 MB board (the CYD) has little room left; the app shows how much a plugin takes there (`flash_kb`).
- **Memory**: every tile counts in the screen's layout memory (`memory`).
- **Time**: the screen draws, takes taps and runs plugins in one loop; `on_tick` and `on_state` must be quick.
