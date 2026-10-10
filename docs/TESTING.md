# Testing a plugin

Four ways, from quick to real. A plugin is done when it ran on a real screen.

| Way | What one change costs | What it shows |
|---|---|---|
| 1. The check | Seconds, no screen | The manifest as the app reads it, the files, the texts, `plugin.yaml`, a few code patterns, and what the plugin needs. Not whether the C++ compiles. |
| 2. A link to GitHub | A push; then up to an hour until the app sees the new commit (it asks GitHub at most every hour); then a tap, a full firmware build in the app and an update over Wi-Fi | The whole route a person takes: the Plugins page, the build, the screen. |
| 3. A test folder in Home Assistant | A copy into Home Assistant's config; then **Build again**: a full firmware build in the app and an update over Wi-Fi, no wait for GitHub | The same, without publishing anything. |
| 4. A build on your own computer | A build with `esphome run` (the first one long, later ones only what changed), uploaded over USB or Wi-Fi | Compiler errors and the screen's log the soonest. |

A full firmware build in the app takes minutes, and longer on a small Home Assistant host; the update over Wi-Fi
follows it. So for a run of small changes to the C++, the build on your own computer is the quickest, and the link to
GitHub the slowest.

## 1. The check (seconds)

```sh
pip install pyyaml                  # once
python3 tools/check.py plugins/my_idea
```

It needs Python 3 with PyYAML. It runs the Tessera app's own manifest check (`plugin_manifest.py`, downloaded from the
Tessera repository's `dev` branch into `tools/.cache/` and again when that copy is an hour old) and the rules the app
cannot see: the files, the translations, the README, the `CHANGELOG.md` (its first heading is the manifest's version)
and the code rules of [FIRMWARE_API.md](FIRMWARE_API.md). It also checks `plugin.yaml` ([LIMITS.md](LIMITS.md),
"plugin.yaml"); that the manifest's `api` fits the core it checks against ([FIRMWARE_API.md](FIRMWARE_API.md),
"Versions"); that no folder but `template/` keeps the template's placeholders (`maintainer: your-github-name`, the
LICENSE's `<year> <your name>`, the README's line that it is the template); and that what the plugin needs is in the
index: the plugins of `requires.plugins`, and a plugin or a board for every feature it needs (the boards from Tessera's
`boards.json`).

Working on Tessera itself? Point it at your checkout:
`TESSERA_MANIFEST=../homeassistant_espscreen/screen_manager/app/plugin_manifest.py python3 tools/check.py`, and
`TESSERA_BOARDS=../homeassistant_espscreen/screen_manager/app/boards.json` for its boards. `TESSERA_REF=main` checks
against the released core instead of `dev`.

The check reads files; it does not build. Only a build (ways 2 to 4) says whether the C++ compiles.

## 2. A test from GitHub (up to an hour from a push to an update)

The simplest way onto a real screen: push the plugin to a repository of your own, without a release, and add it with a
link.

1. Push the plugin to GitHub: the whole repository is the plugin, or the plugin is a folder in it.
2. Open Tessera, go to Plugins, **Add with a link**, and paste `https://github.com/<you>/<repo>`, or
   `https://github.com/<you>/<repo>/tree/main/<folder>` for a folder. The app reads the plugin and says whether it fits
   before anything is built.
3. Add it to a screen. A repository without a release is a test of its default branch, with the label **Test**: the
   screen builds the commit the app read. The build's progress and log are in the screen's Plugins tab.
4. Place the tile in Layout and set its options.
5. Push a change. The app asks GitHub for a newer commit at most every hour, so it can take up to an hour before it
   offers the change as an update on the screen's Plugins tab; one tap builds it, a full firmware build followed by an
   update over Wi-Fi. Nothing is built without that tap.

Once the repository has releases, a plain link takes the newest release. To keep testing a branch, choose
**Test a branch** in the same dialog and name it.

## 3. A test folder in Home Assistant (a build per change)

Nothing published at all, and every build takes the folder as it is:

1. Copy the plugin's folder into Home Assistant's config, beside the `esphome` folder, under its id:

   ```
   /config/tessera-plugins/my_idea/tessera-plugin.yaml
   /config/tessera-plugins/my_idea/plugin.yaml
   /config/tessera-plugins/my_idea/components/my_idea/...
   ```

   The Samba share, the File editor or `scp` all work.
2. Open Tessera, go to Plugins. The plugin is there with the label **Test**. A folder that is not a valid plugin shows
   what is wrong with it under the list.
3. Add it to a screen. The app writes the screen's plugins file with the folder in it and builds the screen.
4. Place the tile in Layout and set its options.
5. After a change in the folder, open the plugin in the screen's Plugins tab and press **Build again**: a full
   firmware build in the app and an update over Wi-Fi, with no wait for GitHub.

A test folder has no updates: every build takes what the folder holds.

## 4. A build on your own computer (the quickest round)

The screen's YAML and its plugins file build anywhere ESPHome runs:

```yaml
# kitchen.yaml, the screen's own YAML (Tessera writes it; the line under packages is added with the first plugin)
packages:
  display:
    url: https://github.com/MaxGramser/homeassistant_espscreen
    ref: dev
    files: [packages/guition.yaml]
  tessera_plugins: !include kitchen.plugins.yaml
```

```yaml
# kitchen.plugins.yaml, written by Tessera; for a plugin you are working on, point it at your folder:
packages:
  plugin_my_idea: !include ../tessera-plugins/my_idea/plugin.yaml
external_components:
  - source: { type: local, path: ../tessera-plugins/my_idea/components }
```

```sh
esphome run kitchen.yaml
```

The first build of a screen takes long; after that ESPHome builds only what changed, and `esphome run` uploads over USB
or Wi-Fi and then shows the screen's log at once. Start from a screen the app already runs the plugin on (a test
folder, way 3), so the app knows the plugin and sends its tiles their data as after a build of its own.

Building with plugins needs a core with the plugin API: Tessera's `dev` branch until it is released.

## What to look at on the glass

- Every size the tile offers (`sizes` in the manifest), on the smallest screen you have (a CYD if you can) and a large
  one.
- Light and dark (the screen's settings page), so `on_theme` is right.
- Every waiting state: no option filled in, the service down (a wrong host in a copy of the manifest), the first
  seconds after a start.
- A page with the tile, then another page and back: the tile shows the right data at once.
- Memory: the editor's Layout shows how full the screen's memory is; a tile's `memory` that is too low makes that number
  wrong.

## Reporting a problem with a plugin

Open an issue on the plugin's repository, with the board, the screen's firmware version (Screen settings), the plugin's
version and what the screen shows. The screen's log (ESPHome's logs) says when a plugin's tile is made and what it got.
