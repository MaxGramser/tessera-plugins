# The manifest: tessera-plugin.yaml

The manifest tells the Tessera app what a plugin is, what it may do and what to show. It holds no code and no words:
names, labels and hints are keys into the plugin's `translations/<language>.json`, part `app`.

The app checks every manifest with the same code before it shows a plugin (`plugin_manifest.py` in the Tessera
repository; `tools/check.py` runs it). A field the app does not know is an error, so a typo shows up at once; a field
whose name starts with `x-` is ignored, for notes of your own.

## A complete example

```yaml
id: ov_departures
version: 1.0.0
api: "0.1"
icon: bus
maintainer: MaxGramser
license: MIT
stage: example
requires:
  esphome: 2026.6.2
boards: any
flash_kb: 12

permissions:
  network: [v0.ovapi.nl]
attributes: [cloud]
privacy: https://github.com/MaxGramser/tessera-plugins/tree/main/plugins/ov_departures#privacy

tiles:
  - id: next
    name: tile_name
    icon: bus
    sizes: { min: 1x1, max: 3x2 }
    memory: 1200
    data: departures
    example: tile_example
    preview: { badge: "{line}", title: "{to}", countdown: at }
    options:
      - { id: stop, kind: text, label: stop, hint: stop_hint }
      - { id: line, kind: choice, options_from: lines, label: line, hint: line_hint }
      - { id: walk, kind: number, min: 0, max: 30, step: 1, unit: min, default: 0, label: walk, hint: walk_hint }

fetch:
  - id: departures
    url: http://v0.ovapi.nl/tpc/{stop}
    every: 60s
    map:
      items: "$.{stop}.Passes[*]"
      fields:
        line: LinePublicNumber
        to: DestinationName50
        at: { path: ExpectedDepartureTime, as: epoch, tz: Europe/Amsterdam }
      where: { line: "{line}" }
      sort: at
      limit: 6
  - id: lines
    url: http://v0.ovapi.nl/tpc/{stop}
    every: 1h
    map:
      items: "$.{stop}.Passes[*]"
      value: LinePublicNumber
      label: [LinePublicNumber, DestinationName50]
```

## The fields

An id (of the plugin, a tile, an option, an input, a part or a fetch) is 1 to 32 characters of `a-z`, `0-9` and `_`,
starting with a letter.

### Who and what

| Field | Required | What |
|---|---|---|
| `id` | yes | The plugin's id. Unique in the index, the same as its folder and its component. |
| `version` | yes | Three numbers, `1.0.0`. Raise it for every change that reaches screens. |
| `api` | yes | The plugin API it was written for, in quotes: `"0.2"`. It builds on every core with the same major and at least that minor; the core offers plugin API 0.6 now. Name the lowest minor whose parts you use ([FIRMWARE_API.md](FIRMWARE_API.md), "Versions"). |
| `icon` | yes | <a id="icon"></a>A Material Design Icons name from Tessera's icon set (`screen_manager/app/tile_icons.py` in the Tessera repository, such as `bus`, `train`, `calendar`, `thermometer`, `lightbulb`). The screen's icon font holds only that set. |
| `maintainer` | yes | The GitHub name of whoever looks after the plugin. |
| `license` | yes | An SPDX name that goes with AGPL-3.0: `MIT`, `Apache-2.0`, `BSD-2-Clause`, `BSD-3-Clause`, `ISC`, `MPL-2.0`, `LGPL-2.1-or-later`, `LGPL-3.0-or-later`, `GPL-3.0-or-later`, `GPL-3.0-only`, `AGPL-3.0-or-later`, `AGPL-3.0-only`, `Unlicense`, `0BSD`, `CC0-1.0`. |
| `stage` | no | How far along it is, in your word: `stable` (ready for every day), `beta` (works, still finding its feet) or `example` (there to show what a plugin can do and to learn from). The app shows a Beta or Example badge; without it the plugin is `beta`. |

The plugin's name and one-line summary are not fields: they are `name` and `summary` in `translations/en.json`, part
`app`, which every plugin must have.

### What it needs

| Field | Default | What |
|---|---|---|
| `requires.esphome` | none | The oldest ESPHome it builds with, such as `2026.6.2`. |
| `requires.psram` | `false` | `true` for a plugin that needs a board with PSRAM (not the CYD). |
| `requires.plugins` | `[]` | Plugins that must be on the screen first. |
| `boards` | `any` | `any`, or a list of board keys from Tessera's `boards.yaml` (`[cyd, guition]`) for a plugin with hardware of one board. |
| `flash_kb` | `0` | About how much flash it adds, in KB. The app uses it to say whether it fits a 4 MB board. |

### What it may do

The screen runs a plugin's code with every right the screen has, so a plugin names what it does. The app shows it
before anyone adds the plugin, and asks again when an update asks for more.

| Field | What |
|---|---|
| `permissions.network` | The hosts its fetches reach: names only, no address, nothing on a home network (`.local`, `192.168.x.x`). A fetch to any other host is refused. |
| `permissions.read_entities` | Home Assistant entities the screen itself reads (an ESPHome `homeassistant` sensor). `"{calendar}"` stands for the entity a person chose in the input `calendar`. |
| `permissions.home_assistant_actions` | Home Assistant actions the screen calls (`tessera::action`). |
| `permissions.ha_commands` | Home Assistant commands the app may ask on the plugin's behalf (`tessera::send`, API 0.2): `call_service:<domain>.<service>` for an action that answers (`call_service:calendar.get_events`), or a websocket command (`history/history_during_period`). Commands that read or change Home Assistant itself (`config/...`, `auth`, `supervisor`, `fire_event`, `render_template`, plain `call_service`, ...) are refused. Every one asked is logged. |
| `attributes` | Any of `cloud` (uses a service outside the home), `commercial`, `ai-developed`. A plugin with `permissions.network` has `cloud`. |
| `privacy` | An https link to what the service sees. Required with `cloud`. A section of the README is fine. |

### What a person fills in: `inputs`

Asked when the plugin is added to a screen.

```yaml
inputs:
  - { id: api_key, kind: secret, scope: all, label: api_key, hint: api_key_hint }
  - { id: pin, kind: gpio, scope: screen, label: pin }
```

| Field | What |
|---|---|
| `kind` | `secret` (kept by the app, never shown again, never on the screen or in YAML; used in a fetch), `text`, `gpio` (a free pin of the board), or `entity` (API 0.2: an entity of `domains`, for the plugin's own `homeassistant` sensors). |
| `domains` | For `entity`, and only there: the Home Assistant domains it takes, `[calendar]`. The editor offers the entities of those domains. |
| `scope` | `all`: asked once for every screen. `screen`: per screen. Default: `all` for a secret, `screen` for the rest. |
| `label`, `hint` | Text keys. |

A `text`, `gpio` or `entity` input reaches the screen's build as a substitution in capitals: input `pin` is `${PIN}` in
`plugin.yaml`. A `secret` never does. At most 8 inputs. Give each a default in `plugin.yaml` (`defaults: { PIN: GPIO22 }`)
so a build without the value still works.

### Optional parts: `parts`

A part is an extra ESPHome file a person turns on per screen (large test tools, for example).

```yaml
parts:
  - { id: tests, file: parts/tests.yaml, label: tests, hint: tests_hint, flash_kb: 440, default: false }
```

### Tiles: `tiles`

| Field | Required | What |
|---|---|---|
| `id` | yes | The tile type's id; `add_tile("<id>", ...)` in the C++ uses the same. A layout calls it `plugin:<plugin>.<id>`. |
| `name` | yes | Text key: what the library and the inspector call it. |
| `icon` | no | From Tessera's icon set; the plugin's icon when left out. The editor shows it, and a screen without the plugin draws it on its placeholder. |
| `sizes` | yes | `{ min: 1x1, max: 2x2 }`, columns x rows. The editor offers the sizes in between that fit the screen's grid. |
| `memory` | yes | What one tile costs of the screen's layout memory, in bytes (64 to 16384). The screen and the app add it to the layout's budget. About 400 for a few labels, 1200 for a list. |
| `domains` | no | The Home Assistant domains of the entity the tile belongs to, `[calendar]`, as a tap action and an input of kind `entity` name theirs (API 0.4; `entity:` before it). The inspector offers the entities of those domains, also of a domain Tessera draws no tile for; the tile gets the entity's state, name and `attributes`, again at every change. |
| `attributes` | no | With `domains`: the attributes the tile gets, at most 16. One named `..._at`, `..._time` or `...date` that holds a moment comes as seconds since 1970. A list comes whole when it fits (API 0.5; 16 items before): texts and numbers, not objects. |
| `fields` | no | With `domains` (API 0.5): values taken out of the entity's attributes with the paths and kinds of a fetch's map ([FETCH.md](FETCH.md)), at most 8, each named apart from `attributes`, `state` and `name`. `{ prices: { path: "raw_today[*].value", as: numbers } }` turns a list of 96 objects into one list of 96 numbers, which fits where the objects would not. |
| `has_attributes` | no | With `domains` (API 0.5): attributes an entity must have to be offered in the inspector, at most 8. `[raw_today]` lists the price sensor instead of every sensor in the house. Only the list: the tile keeps its entity when an attribute is gone for a while (an entity that is unavailable has none). |
| `data` | no | The id of the fetch whose answer the tile gets in `on_state`. |
| `example` | no | Text key: a line the editor shows on the tile's placeholder. |
| `preview` | no | How the editor draws the tile from its data, so the page in the editor looks like the glass (it cannot run your C++). With `data` or `domains`. `badge`, `title` and `value` are templates of the first item's fields (`"{line}"`, `"{to}"`; for a tile of an entity `{state}`, `{name}` and its attributes); `countdown` names a field with `as: epoch`, and the editor counts down to it in whole minutes (the editor's clock moves every 30 seconds). `value` or `countdown`, not both. Without a preview the editor shows the icon, the name and `example`. |
| `options` | no | At most 12 options the inspector shows, below. |

An option:

| Field | What |
|---|---|
| `id`, `kind`, `label` | Required. `kind` is `text`, `choice`, `number` or `toggle`. |
| `hint` | Text key under the option. |
| `default` | The value when nothing is chosen. It must fit the option. |
| `choices` | For `choice`: `[{ value: "a", label: key }]`, at most 48. |
| `options_from` | For `choice`: the id of a fetch that fills the list (its `map` has `value` and `label`). |
| `min`, `max`, `step`, `unit` | For `number`: the range (both required), the step (default 1), a unit of up to 8 characters. |

A text option's value is at most 64 characters. The tile gets every option in `context.options` of `create()`, with the
defaults filled in. Options are also the placeholders of the tile's fetch: `{stop}` in a URL is the option `stop`.

### Fetches: `fetch`

At most 8. [FETCH.md](FETCH.md) explains them in full.

| Field | What |
|---|---|
| `id` | The fetch's id; a tile names it in `data`, an option in `options_from`. |
| `url` | `https://` (or `http://` for a fetch that carries no secret), a host from `permissions.network`, placeholders such as `{stop}` from the options of the tiles that use it and from the inputs. A secret may stand in the query, never in the path. |
| `headers` | Up to 8, with placeholders: `{ Authorization: "Bearer {api_key}" }`. |
| `every` | How old an answer may get: `30s`, `5m`, `1h`, `1d`. At least 30 seconds. |
| `map` | What of the answer reaches the screen. |

### Answers: `answers` (API 0.5)

What the screen gets of the answer to a command of `permissions.ha_commands`: its fields, with the paths and kinds of a
fetch's map. A command without an entry here comes as Home Assistant answered it, bounded.

```yaml
permissions:
  ha_commands: ["call_service:nordpool.get_prices_for_date"]
answers:
  - command: call_service:nordpool.get_prices_for_date
    fields:
      prices: { path: "[*][*].price", as: numbers }    # 96 prices of a quarter of an hour
      start: { path: "[*][0].start", as: epoch }       # when the first one starts
```

`on_message` then gets `{"re": 1, "ok": true, "result": {"prices": [83.1, 81.4, ...], "start": 1791410400}}`: about
700 bytes instead of 8 KB of objects, of which a quarter would have fitted. At most 8 entries, one per command.

### Cards: `cards` (API 0.2)

A screen of the plugin's own over the page ([FIRMWARE_API.md](FIRMWARE_API.md), "A card").

```yaml
cards:
  - { id: upcoming, name: card_name }          # wide: true for the whole width of the glass
```

| Field | What |
|---|---|
| `id` | `add_card("<id>", ...)` in the C++ uses the same. |
| `name` | Text key: what the editor calls it. On the screen the card's title is the tile's name, or what `open_card` passes. |
| `wide` | `true`: as wide as the glass (a picture, a timeline); else a hand's width, as Tessera's own cards. |

### Tap actions: `tap_actions` (API 0.2)

Something a person can set one of Tessera's own tiles to do on a tap, in the tile's inspector.

```yaml
tap_actions:
  - { id: upcoming, label: tap_upcoming, domains: [sensor], card: upcoming }
```

| Field | What |
|---|---|
| `id` | `add_tap_action("<id>", ...)` in the C++ uses the same. |
| `label` | Text key: the choice in the inspector's tap list. |
| `domains` | The kinds of tile it is offered for. |
| `card` | Optional: the card it opens, for the editor's description. |

### Top bar items: `bar_items` (API 0.2)

```yaml
bar_items:
  - { id: soon, label: bar_soon, icon: recycle, example: bar_example }
```

| Field | What |
|---|---|
| `id` | `add_bar_item("<id>", ...)` in the C++ uses the same. |
| `label` | Text key: the item's name under "From plugins" when adding to a top bar. |
| `icon` | From Tessera's set; the plugin's icon when left out. |
| `example` | Text key: what the editor's mockup of the bar shows. |

### Settings a person changes: `settings` (API 0.2, more kinds in 0.6)

A setting is an ESPHome entity of the plugin's `plugin.yaml`. Home Assistant sees it like any entity of the screen, the
screen can show it on its own settings page, and the Tessera editor shows it in the plugin's details on the screen's
Plugins tab, under **Settings**, "On this screen". A change there takes effect at once, through Home Assistant, without
a build.

```yaml
# tessera-plugin.yaml
settings:
  - { key: tap_sound, label: setting_tap_sound, hint: setting_tap_sound_hint }
  - { key: speaker_volume, label: setting_volume }
  - { key: test_wake_word, label: setting_test, hint: setting_test_hint, status: test_wake_word_result }
```

```yaml
# plugin.yaml: the entities those keys name
switch:
  - platform: template
    name: Tap sound                 # key tap_sound
    optimistic: true
    restore_mode: RESTORE_DEFAULT_ON
number:
  - platform: template
    name: Speaker volume            # key speaker_volume
    optimistic: true
    min_value: 0
    max_value: 100
    step: 5
    unit_of_measurement: "%"
button:
  - platform: template
    name: Test wake word            # key test_wake_word
    on_press:
      - text_sensor.template.publish: { id: my_plugin_test_result, state: "Listening" }
text_sensor:
  - platform: template
    name: Test wake word result     # status test_wake_word_result
    id: my_plugin_test_result
```

| Field | What |
|---|---|
| `key` | The entity's `name` in `plugin.yaml` written as an id, the way ESPHome does it: lowercase, a space becomes `_`, anything but letters, digits, `_` and `-` becomes `_`. "Tap sound" is `tap_sound`, "Mic gain (dB)" is `mic_gain__db_`. Give settings plain names and the key is obvious. |
| `label`, `hint` | Text keys: the row's name and the line under it. |
| `status` | (API 0.6) Only for a button: the key of a text sensor whose state the row shows beside the button, so a test can say how it went ("Heard: Okay Nabu"). The editor reads it again a few times after a press. |

The kind of row follows from the entity's platform:

| Platform in `plugin.yaml` | The editor shows | Since |
|---|---|---|
| `switch` | A switch | 0.2 |
| `number` | A number with its min, max, step and unit, with - and + | 0.2 |
| `select` | Its options as buttons, a dropdown from six options on (at most 48) | 0.2 (dropdown 0.6) |
| `text` | A text field, saved with Enter or when it is left, within the entity's `min_length` and `max_length`; hidden when its `mode` is `password` | 0.6 |
| `button` | A **Run** key, with its `status` beside it | 0.6 |

**How the app finds the entity.** It looks in Home Assistant's entity registry for an entity of that screen's device
whose name in ESPHome gives the key. A person who renames the entity in Home Assistant keeps the setting; so does a
screen with a name other than you expected. Do not build on Home Assistant's `unique_id` or the YAML `id`: neither is
what the key means. (Before API 0.6 the app matched the end of the entity id; it still does for an entity Home
Assistant's registry does not have.)

**Settings, inputs or parts?**

| What the person changes | Use | A change |
|---|---|---|
| A value the plugin reads while it runs: a volume, a switch, a word, a test | `settings` with an ESPHome entity | Takes effect at once |
| A key or value the same for every screen, used by the app (a fetch's API key) | `inputs` with `scope: all` | Kept in the app; a secret never reaches the screen |
| A value the build needs: a pin, a model to build in | `inputs` with `scope: screen` | Builds the screen again |
| An optional piece of firmware | `parts` | Builds the screen again |

The editor shows all of them together in the plugin's details on the screen's Plugins tab: "On this screen" (the
settings), "For every screen" (inputs with `scope: all`) and "When building" (inputs with `scope: screen` and the
parts), with a **Save and build** key that lights up when something differs from what the screen was built with.

The app changes only an entity a plugin on that screen names here, of that screen's own device. A `status` is only
shown, never set.

## Limits at a glance

| What | Most |
|---|---|
| Tiles per plugin | 8 |
| Options per tile | 12 |
| Inputs | 8 |
| Parts | 4 |
| Fetches | 8 |
| Cards, tap actions | 8 each |
| Top bar items | 4 |
| Attributes of a tile's entity | 16, and 8 `fields`, 8 `has_attributes` |
| Settings | 8; a select's options 48 |
| Answers mapped | 8 |
| Home Assistant commands | 8 |
| Fields per mapped item | 8 |
| Items in a mapped list | 48 |
| Bytes of a tile's data on the wire | about 2.6 KB (the last items are dropped to fit) |
| Text of a mapped field | 48 bytes |
| An answer of a web service | 64 KB, in 10 seconds |
