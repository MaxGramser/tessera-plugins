# Instructions for AI assistants (and a summary for people)

You are helping someone make or change a plugin for Tessera, the ESP32 touch screens for Home Assistant. Read this
file first, then the docs in the order below. Everything here is checked by `tools/check.py`; run it before you say a
plugin is done.

## What a plugin is, in five sentences

1. A plugin is a folder: a manifest (`tessera-plugin.yaml`), an ESPHome package (`plugin.yaml`), an ESPHome component
   in C++ (`components/<id>/`), texts (`translations/<language>.json`), a `README.md` and a `CHANGELOG.md`.
2. The screen builds the component into its firmware; the component registers tile types with the core through
   `tessera::Plugin` (`esphome/components/smart_display/plugin_api.h` in the Tessera repository).
3. The Tessera app (in Home Assistant) reads only the manifest: it shows the plugin, its options and its README, and
   carries out the plugin's `fetch` (JSON from a web service) with its own code.
4. A tile of a plugin is `plugin:<plugin id>.<tile id>` in a screen's layout. The core gives it a card's drawing area
   while its page is on the glass, hands it the data the app sent (`on_state`), and ticks it once a second (`on_tick`).
5. The plugin API is 0.7, and a manifest says `api: "0.7"`. A plugin builds on every core with the same major and at
   least its minor. While the API is 0.x a minor may still change a name (`docs/FIRMWARE_API.md`, "Versions").

## Where each kind of thing lives

| The plugin needs | Use | Read |
|---|---|---|
| Data from a web service | A `fetch` the app carries out, mapped to fields | docs/FETCH.md |
| Data Home Assistant already has (a sensor, a calendar, prices in attributes) | A tile of an entity: `domains`, `attributes`, `fields`, `has_attributes` | docs/MANIFEST.md, "Tiles" |
| The answer of a Home Assistant action (`calendar.get_events`, `nordpool.get_prices_for_date`) | `permissions.ha_commands`, `tessera::send`, and `answers` to map it | docs/MANIFEST.md, "Answers"; docs/FIRMWARE_API.md |
| To do something in Home Assistant | `tessera::action`, named in `permissions.home_assistant_actions` | docs/FIRMWARE_API.md |
| A setting a person changes | An ESPHome entity in `plugin.yaml`, named in `settings` | docs/MANIFEST.md, "Settings a person changes" |
| A value or key filled in once | `inputs` (`scope: all` for every screen, `screen` per screen) | docs/MANIFEST.md, "What a person fills in" |
| Hardware on the board | ESPHome components in `plugin.yaml`, `boards`, optional `parts` | docs/MAKING_A_PLUGIN.md |
| A speaker, microphone or media player for other plugins | `provides`, and the component with the feature's id (`ts_speaker`) in `plugin.yaml` | docs/MAKING_A_PLUGIN.md, "Features" |
| A speaker, microphone or media player another plugin or the board brings | `requires.features`, or a part with `features` when the plugin also works without it | docs/MANIFEST.md, "Features" |
| Another plugin on the same screen | `requires.plugins`: the app adds it with yours | docs/MANIFEST.md, "What it needs and brings" |
| A server, an account, a live session (OAuth, a websocket, a voice model) | A Home Assistant integration of its own, which the plugin reaches through its entities and actions | docs/MAKING_A_PLUGIN.md, "Where the backend lives" |

## Read in this order

1. [docs/MAKING_A_PLUGIN.md](docs/MAKING_A_PLUGIN.md): the files, the steps, trying it on a screen.
2. [docs/MANIFEST.md](docs/MANIFEST.md): every field of `tessera-plugin.yaml` and its limits.
3. [docs/FIRMWARE_API.md](docs/FIRMWARE_API.md): the C++ API, the life of a tile, the drawing rules.
4. [docs/FETCH.md](docs/FETCH.md): data from a web service, the map language, the limits.
5. [docs/TRANSLATIONS.md](docs/TRANSLATIONS.md), [docs/TESTING.md](docs/TESTING.md),
   [docs/PUBLISHING.md](docs/PUBLISHING.md), [docs/LIMITS.md](docs/LIMITS.md) when you need them.

The two plugins to copy from: [`template/`](template) (a tile without data, the smallest complete plugin) and
[`plugins/ov_departures/`](plugins/ov_departures) (a tile with data from a web service, a list of choices and a
countdown). For the other parts: [`plugins/waste_collection/`](plugins/waste_collection) (a tile of an entity, a card,
a tap action, a top bar item, settings) and [`plugins/p4_audio/`](plugins/p4_audio) (one board's hardware, a click
on every tap, settings with tests).

## Rules that are never optional

- **Start from the template**: `python3 tools/new_plugin.py <id>`. Do not write the folder from memory.
- **Topics say what it is about**: one or two of the list in `docs/MANIFEST.md`, "What it is about", always. Never a topic of your own; ask for a new one in an issue. What the plugin adds (tiles, a function, hardware)
  is no topic: the app reads it from the manifest.
- **A feature is a promise.** A plugin that `provides` a speaker makes `speaker:` with `id: ts_speaker`; one that needs
  it uses `ts_speaker` and never makes it. Ids that start with `ts_` are Tessera's, for nothing else.
- **plugin.yaml adds, it never takes over**: no `wifi`, `api`, `ota`, `logger`, `esphome`, `http_request`,
  `web_server`, `packages`, `external_components` or the other keys of `docs/LIMITS.md`, "plugin.yaml".
- **The id is the same everywhere, and for ever**: the folder in `plugins/`, `id:` in the manifest, the component
  folder `components/<id>/`, the YAML key in `plugin.yaml` and the C++ namespace `esphome::<id>`. Lowercase letters,
  digits and `_`, starting with a letter, at most 32. Screens, layouts and secrets know the plugin by it: never rename
  it. A fork that keeps the id is another plugin of that id (another origin) and never gets the original's secrets.
- **No words in code or manifest.** Every name, label and hint in the manifest is a key into
  `translations/en.json`, part `app`. Every word on the screen is a key of part `screen`, read with
  `plugin->text("key")`. English is complete; other languages may lack keys.
- **Draw like the rest of the screen**: sizes with `tessera::ui::mm()` / `ui::px()`, colours with a theme role
  (`theme::INK`, `theme::MUTED`, `theme::ACCENT`, ...), only the screen's fonts (`tessera::Font`). Never a colour as a
  number, never a font of your own, never an LVGL click handler (use `Tile::on_tap`), never a timer or task (use
  `on_tick`), never a network connection from the screen (data comes through `fetch`).
- **Nothing exists off the glass.** Make every LVGL object in `create()` inside `context.parent`. The core deletes them;
  a destructor only frees your own memory.
- **Paint, do not rebuild.** `on_state` and `on_tick` set texts and positions with `ui::set_text`, `ui::set_font`,
  `ui::set_color`, which change a property only when it differs.
- **Name what the plugin may do** in `permissions`: every host a fetch reaches in `network`, every Home Assistant entity
  the screen reads in `read_entities`, every action it calls in `home_assistant_actions`. A plugin with `network` has
  the attribute `cloud` and a `privacy` link.
- **Memory**: the manifest's `memory` for a tile is what one tile costs in the screen's layout memory. Keep it honest:
  a few hundred bytes for a handful of labels, about 1200 for a list.
- **Secrets stay in the app.** An API key is an input of `kind: secret`; it goes into a fetch's header or query, never
  into the URL path, never to the screen, never into YAML. A key for a service Home Assistant talks to (OpenAI,
  Anthropic, Spotify) stays in that Home Assistant integration; the plugin never asks for it.
- **Settings are ESPHome entities.** A value a person changes after the build (a volume, a switch, a word, a test
  button) is a template entity in `plugin.yaml` and a line in the manifest's `settings`, whose `key` is the entity's
  name written as an id ("Tap sound" is `tap_sound`). Never a setting of the plugin's own in the app, never a global
  the editor cannot see.
- **Home Assistant is the backend.** No plugin code runs in the Tessera app. What needs a server goes into Home
  Assistant: an existing integration, or one of your own.
- **Plain English in READMEs**, with a `## Set up` section of numbered steps a person can follow in the Tessera app.
- **A changelog line for every version.** Raise `version` and add its `## <version>` heading at the top of
  `CHANGELOG.md`, with a few bullet lines of what a person notices and what they must do. The app shows them when it
  offers the update (docs/MAKING_A_PLUGIN.md, "The changelog").

## When you are done

```sh
python3 tools/check.py plugins/<id>     # must print "<id>: ok"
```

Then build it on a real screen ([docs/TESTING.md](docs/TESTING.md)): push it to GitHub and add the repository's link
in the app (without a release that is a test of the default branch, and a newer push shows as an update), or copy the
folder into Home Assistant's config as `tessera-plugins/<id>/`. A plugin that passes the check but was never on a
screen is not done: say so, and say which board you did not try.
