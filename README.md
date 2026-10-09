# Tessera plugins

Plugins add something to a [Tessera](https://github.com/MaxGramser/homeassistant_espscreen) touch screen that not
everyone needs: a tile with the next bus, hardware on one board, a feature of your own. A plugin is built into the
firmware of the screens you choose, and the Tessera app in Home Assistant gives its tiles their data.

This repository holds:

| Folder | What |
|---|---|
| [`plugins/`](plugins) | The plugins Tessera ships, one folder each. Every folder is a complete plugin. |
| [`template/`](template) | A small plugin that works, to start your own from. |
| [`docs/`](docs) | How plugins work and how to make one, step by step. |
| [`tools/`](tools) | `check.py` (check a plugin), `new_plugin.py` (start one), `build_index.py` (write the index), `build_likes.py` (copy the likes). |
| `index.json` | The list the Tessera app reads. Written by `tools/build_index.py`, never by hand. |
| `blocked.yaml` | Plugin versions the app refuses to build. |
| `featured.yaml` | The plugins Tessera recommends to start with. |
| `transfers.yaml` | Plugin ids that moved to another repository. |
| `likes.json` | How many people like each plugin, copied from Tessera's website every hour by `tools/build_likes.py`. |

## The plugins

| Plugin | What it does | Boards |
|---|---|---|
| [Public transport (NL)](plugins/ov_departures) | The next bus, tram, metro or ferry from your stop, live from OVapi, counted down on the screen. | All |
| [Waste collection](plugins/waste_collection) | When the next bin goes out, from a calendar in Home Assistant, on a tile, a card and in the top bar. | All |
| [P4 panel audio](plugins/p4_audio) | The speaker and microphone of the Waveshare ESP32-P4 86 panel: a click on every tap, a volume, tests on the screen. | Waveshare ESP32-P4 86 panel |

## Using a plugin

1. Open Tessera in Home Assistant and go to **Plugins**, or to the **Plugins** tab of a screen. The tabs Tiles,
   Functions and Hardware say what a plugin adds, and In use what your screens run. Topics narrow the list, and it is
   sorted by most liked unless you choose newest or name.
2. Open a plugin, read what it does and what it may do, tick the screens it goes on, and apply. What it needs comes
   along: a plugin it names, or the plugin that brings a speaker the screen lacks. Each screen builds its firmware once
   with all of it.
3. In **Layout**, place the plugin's tile from the library's **Plugins** group and set its options in the inspector.

A plugin that is not in the list can be added with a link to its GitHub repository (**Add with a link**). Every screen
stays on the commit it was built with: a newer version shows as an update on the screen's Plugins tab, and one tap
builds it.

Plugins are new and only in the dev version of the app for now (the app added with `#dev` at the end of the repository
URL, or a local copy). The plugin API is 0.7: while it is 0.x it can still change in a minor, Tessera's own plugins
move with it, and a plugin names the version it was written for ([docs/FIRMWARE_API.md](docs/FIRMWARE_API.md),
"Versions").

## Making a plugin

```sh
git clone https://github.com/MaxGramser/tessera-plugins
cd tessera-plugins
python3 tools/new_plugin.py my_idea          # plugins/my_idea/, copied from template/
python3 tools/check.py plugins/my_idea       # the same check the app does
```

Then read [docs/MAKING_A_PLUGIN.md](docs/MAKING_A_PLUGIN.md). It goes through every file, how to try the plugin on
your own screen before anyone else sees it, and how it gets into the list. Every version gets a few lines in the
plugin's `CHANGELOG.md`: the app shows them when it offers the update.

A plugin can also live in a repository of your own. To try it, push it and add the repository's link in the app: a
repository without a release is a test of its default branch. To share it, list it once with a pull request that adds a
three-line file to `community/`; after that every release you publish reaches the app by itself within the hour,
without a pull request here ([docs/PUBLISHING.md](docs/PUBLISHING.md)).

Working with an AI assistant? Point it at [AGENTS.md](AGENTS.md) first: it has the rules and the order to read the
docs in.

## What a plugin can and cannot do

- **On the screen** a plugin is an ESPHome component written in C++. It draws its own tiles with the screen's fonts,
  colours and sizes, and hears about the screen's moments (start, a tick, standby, an update).
- **In the app** a plugin is only a description. The app reads its manifest and carries out what it asks with its own
  code: fetch JSON from a web service it names, keep a secret key, show its options and its README. The app never runs
  a plugin's code.
- A plugin draws only its own tiles. It cannot change how other tiles look, block taps, or reach your home network
  through the app.

[docs/LIMITS.md](docs/LIMITS.md) has the full list, with the way around where there is one.

## Licence

The tools, the template and Tessera's own plugins are MIT ([LICENSE](LICENSE)). A plugin is built into firmware that
is AGPL-3.0, so every plugin's licence must go with AGPL-3.0 ([docs/MANIFEST.md](docs/MANIFEST.md), "license").
