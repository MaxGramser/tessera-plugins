#!/usr/bin/env python3
"""Check plugins the way the Tessera app reads them, before a push or a pull request.

    python3 tools/check.py                      # every folder in plugins/ and template/
    python3 tools/check.py plugins/my_plugin    # one plugin (any folder with a tessera-plugin.yaml)

It runs the app's own manifest check: plugin_manifest.py from the Tessera repository (homeassistant_espscreen,
screen_manager/app/plugin_manifest.py), fetched into tools/.cache/ and fetched again when it is an hour old (offline,
the copy there serves). The same file decides in the app whether a
plugin is shown, so a plugin that passes here is read the same way there. Then the rules the manifest check cannot see:
the folder layout, the translations (English complete), the README, the licence, the drawing rules for the firmware
code (docs/FIRMWARE_API.md, "Rules"), what plugin.yaml may set (docs/LIMITS.md, "plugin.yaml"), and, across the index,
that every plugin and feature a plugin needs is there (docs/MANIFEST.md, "What it needs and brings").

TESSERA_MANIFEST=/path/to/plugin_manifest.py uses a local copy instead (a checkout of the Tessera repository), and
TESSERA_BOARDS=/path/to/boards.json the boards a local checkout knows (screen_manager/app/boards.json).
"""
import importlib.util
import json
import os
import re
import sys
import time
import urllib.request
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[1]
SOURCE = 'https://raw.githubusercontent.com/MaxGramser/homeassistant_espscreen/{ref}/screen_manager/app/{name}'
CACHE = ROOT / 'tools' / '.cache'

# Code rules for a plugin's C++ (docs/FIRMWARE_API.md): what the core forbids itself, so a plugin draws like the rest.
CODE_RULES = [
    (re.compile(r'lv_color_hex\s*\(|0x[0-9a-fA-F]{6}\b'), 'a colour as a number: use a theme role (tessera::ui::color)'),
    (re.compile(r'heap_caps_get_largest_free_block|heap_caps_get_info'), 'a walk of the heap: it shifts the picture of an RGB panel'),
    (re.compile(r'LV_EVENT_CLICKED|LV_EVENT_PRESSED'), 'an LVGL tap of its own: use Tile::on_tap, which has the touch filter in front'),
    (re.compile(r'lv_timer_create|xTaskCreate|delay\s*\(\s*\d'), 'a timer, a task or a wait of its own: use on_tick'),
    (re.compile(r'lv_font_t\s+\w+\s*=|font:\s*$|LV_FONT_DECLARE'), 'a font of its own: use the screen\'s fonts (tessera::Font)'),
    (re.compile(r'http_request|HTTPClient|esp_http_client|WiFiClient'), 'a connection from the screen: data comes through the app (fetch)'),
]

# What plugin.yaml (and a part's file) never sets at its top level (docs/LIMITS.md, "plugin.yaml"). It is merged into
# the screen's own configuration, so these would change what the core and the screen's own YAML hold, or open the
# screen to the network and fetch from it, where a plugin's data comes through the app.
OWNED_KEYS = ('esphome', 'esp32', 'esp8266', 'rp2040', 'psram', 'wifi', 'ethernet', 'network', 'api', 'ota', 'logger',
              'improv_serial', 'esp32_improv', 'external_components', 'packages', 'substitutions')
NETWORK_KEYS = ('web_server', 'captive_portal', 'mqtt', 'http_request', 'online_image', 'socket', 'udp',
                'packet_transport', 'wireguard')
# Under these keys an ESPHome configuration does something (an automation), and an `id:` there names a part that
# exists, it does not make one.
ACTION_KEYS = re.compile(r'^(on_.+|.+_action|then|else)$')


def fetched(name, env, max_age=3600):
    """A file of the Tessera repository's screen_manager/app/: the path in `env` when set, else a copy in tools/.cache/,
    fetched again when it is older than `max_age` seconds (offline, the copy there serves)."""
    local = os.environ.get(env)
    if local:
        return Path(local)
    path = CACHE / name
    if not path.is_file() or time.time() - path.stat().st_mtime > max_age:
        path.parent.mkdir(parents=True, exist_ok=True)
        ref = os.environ.get('TESSERA_REF', 'dev')
        try:
            with urllib.request.urlopen(SOURCE.format(ref=ref, name=name), timeout=20) as response:
                path.write_bytes(response.read())
        except OSError:
            if not path.is_file():
                raise
    return path


def manifest_module(max_age=3600):
    path = fetched('plugin_manifest.py', 'TESSERA_MANIFEST', max_age)
    spec = importlib.util.spec_from_file_location('plugin_manifest', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


PM = None


def pm(fresh=False):
    """The app's manifest module. `fresh`: fetched again now, not from a copy up to an hour old (the index names the API
    of the core as it is)."""
    global PM
    if PM is None:
        PM = manifest_module(0 if fresh else 3600)
    return PM


def plugin_api():
    return '.'.join(map(str, pm().PLUGIN_API))


BOARDS = None


def board_features():
    """{board key: {features}}: what each board brings itself, from the core's boards.json (written from boards.yaml),
    read like the manifest module. A board without a `features` list brings none."""
    global BOARDS
    if BOARDS is None:
        BOARDS = {}
        try:
            shapes = json.loads(fetched('boards.json', 'TESSERA_BOARDS').read_text(encoding='utf-8'))
        except (OSError, ValueError) as error:
            fail('boards.json', f'cannot read the boards of the Tessera repository ({error})')
        for key, shape in shapes.items():
            if isinstance(shape, dict):
                features = shape.get('features') or []
                BOARDS.setdefault(str(shape.get('board') or key), set()).update(
                    f for f in features if isinstance(f, str))
    return BOARDS


# Where the docs name the API the core offers now. The phrase "plugin API <n>" (lowercase plugin) always means the
# current one; a part's own version is written "API 0.2" or "(0.2)". So the docs cannot fall behind the core unnoticed.
DOC_FILES = ('README.md', 'AGENTS.md', 'llms.txt', 'docs/FIRMWARE_API.md', 'docs/MANIFEST.md', 'docs/MAKING_A_PLUGIN.md')
CURRENT_API = re.compile(r'plugin API (?:is )?(\d+\.\d+)')


def check_docs():
    """Every doc that names the plugin API the core offers names the one the manifest check knows."""
    wanted = plugin_api()
    for name in DOC_FILES:
        text = (ROOT / name).read_text(encoding='utf-8')
        found = CURRENT_API.findall(text)
        if not found:
            fail(name, f'names no plugin API; it should say "plugin API {wanted}" where it means the current one')
        stale = sorted(set(found) - {wanted})
        if stale:
            fail(name, f'says plugin API {", ".join(stale)}; the core offers {wanted}')


def fail(folder, message):
    raise SystemExit(f'{folder}: {message}')


# ---- plugin.yaml: ESPHome's YAML, read as data ----

class Tagged(str):
    """A value with one of ESPHome's tags (!lambda, !secret, !include, !extend, !remove, ...), kept as its plain text."""
    tag = ''


class EsphomeLoader(yaml.SafeLoader):
    """Reads ESPHome YAML without ESPHome: a tag is no error, its value stays as it is written."""


def _tagged(loader, suffix, node):
    if isinstance(node, yaml.SequenceNode):
        return loader.construct_sequence(node, deep=True)
    if isinstance(node, yaml.MappingNode):
        return loader.construct_mapping(node, deep=True)
    value = Tagged(loader.construct_scalar(node))
    value.tag = '!' + suffix
    return value


EsphomeLoader.add_multi_constructor('!', _tagged)


def esphome_yaml(folder, name, file):
    """A plugin's ESPHome file as data, or SystemExit."""
    try:
        data = yaml.load((folder / file).read_text(encoding='utf-8'), Loader=EsphomeLoader)
    except (OSError, yaml.YAMLError) as error:
        fail(name, f'{file}: {error}')
    if data is None:
        return {}
    if not isinstance(data, dict):
        fail(name, f'{file}: an ESPHome configuration is a mapping of components')
    return data


def entries_of(value):
    """The entries of a component: a list of mappings, or one mapping."""
    return [item for item in (value if isinstance(value, list) else [value]) if isinstance(item, dict)]


def made_ids(node):
    """Every id an ESPHome configuration makes (`id: x`), not one an automation names or one it extends (`!extend x`)."""
    if isinstance(node, dict):
        for key, value in node.items():
            if key == 'id' and isinstance(value, str):
                if not isinstance(value, Tagged):
                    yield value
            elif not ACTION_KEYS.match(str(key)):
                yield from made_ids(value)
    elif isinstance(node, list):
        for item in node:
            yield from made_ids(item)


def check_esphome(folder, name, file, provides):
    """The rules for an ESPHome file of a plugin: no key the core owns or that opens the screen, and an id with
    Tessera's prefix only for a feature the plugin brings. Returns the file as data."""
    data = esphome_yaml(folder, name, file)
    for key in data:
        if key in OWNED_KEYS:
            fail(name, f'{file}: "{key}" belongs to the core and the screen\'s own YAML; a plugin never sets it')
        if key in NETWORK_KEYS:
            fail(name, f'{file}: "{key}" opens the screen to the network or fetches from it; data comes through the app')
    promised = {pm().FEATURES[feature][1] for feature in provides}
    for made in made_ids(data):
        if made.startswith('ts_') and made not in promised:
            fail(name, f'{file}: id {made}: ids that start with ts_ are Tessera\'s; a plugin makes one only for a '
                       f'feature it provides (docs/MANIFEST.md, "Features")')
    return data


def check_provides(name, manifest, esphome):
    """A plugin that provides a feature makes it: the feature's ESPHome component with the feature's id."""
    for feature in manifest['provides']:
        domain, wanted = pm().FEATURES[feature]
        ids = [item.get('id') for item in entries_of(esphome.get(domain))]
        if not any(isinstance(i, str) and not isinstance(i, Tagged) and i == wanted for i in ids):
            fail(name, f'plugin.yaml: provides {feature}, so it makes a "{domain}:" with id: {wanted}')


def check_folder(folder):
    """(raw manifest, translations, readmes) of a plugin folder, or SystemExit with what is wrong."""
    folder = Path(folder)
    name = folder.name
    try:
        raw = yaml.safe_load((folder / 'tessera-plugin.yaml').read_text(encoding='utf-8'))
    except (OSError, yaml.YAMLError) as error:
        fail(name, f'tessera-plugin.yaml: {error}')
    translations = {}
    for path in sorted((folder / 'translations').glob('*.json')):
        try:
            translations[path.stem] = json.loads(path.read_text(encoding='utf-8'))
        except ValueError as error:
            fail(name, f'translations/{path.name}: {error}')
    if 'en' not in translations:
        fail(name, 'translations/en.json is required')
    try:
        manifest = pm().check(raw, translations['en'])
    except pm().ManifestError as error:
        fail(name, f'tessera-plugin.yaml: {error}')
    if manifest['id'] != name and folder.parent.name == 'plugins':
        fail(name, f'the folder must be called {manifest["id"]}, as the plugin\'s id')
    english = set(translations['en'].get('screen') or {})
    for lang, data in translations.items():
        extra = set(data.get('screen') or {}) - english
        if extra:
            fail(name, f'translations/{lang}.json: screen texts English does not have: {", ".join(sorted(extra))}')
    if not (folder / 'plugin.yaml').is_file():
        fail(name, 'plugin.yaml is required (what a screen gets)')
    check_provides(name, manifest, check_esphome(folder, name, 'plugin.yaml', manifest['provides']))
    for part in manifest['parts']:
        if not (folder / part['file']).is_file():
            fail(name, f'parts: {part["file"]} is not in the folder')
        check_esphome(folder, name, part['file'], manifest['provides'])
    components = folder / 'components'
    if not components.is_dir() or not any(components.iterdir()):
        fail(name, 'components/<name>/ is required (the ESPHome component)')
    readme = {}
    for path in sorted(folder.glob('README*.md')):
        language = path.stem.split('.', 1)[1] if '.' in path.stem else 'en'
        readme[language] = path.read_text(encoding='utf-8')
    if 'en' not in readme:
        fail(name, 'README.md is required')
    if not re.search(r'(?m)^##\s+(Set ?up|Setup)\b', readme['en']):
        fail(name, 'README.md needs a "## Set up" section')
    if folder.parent.name == 'plugins' and not (folder / 'LICENSE').is_file() and not (folder.parent.parent / 'LICENSE').is_file():
        fail(name, 'a LICENSE is required')
    for path in sorted(components.rglob('*')):
        if path.suffix not in ('.h', '.cpp', '.c', '.hpp'):
            continue
        text = path.read_text(encoding='utf-8', errors='replace')
        code = re.sub(r'//[^\n]*|/\*.*?\*/', '', text, flags=re.S)
        for rule, why in CODE_RULES:
            match = rule.search(code)
            if match:
                line = code[:match.start()].count('\n') + 1
                fail(name, f'{path.relative_to(folder)}:{line}: {why}')
    return raw, translations, readme


# ---- Across the index: what a plugin needs is there ----

def shares_board(a, b):
    """Whether two plugins' `boards` meet on at least one board (`any` meets everything)."""
    return a == 'any' or b == 'any' or bool(set(a) & set(b))


def needs_problem(manifest, known):
    """What a plugin needs that the index does not have, as a sentence, or None. `known` is {id: checked manifest} of
    every plugin in the index. A plugin it needs must be there and fit one of its boards; a feature it needs (also one a
    part uses) must be brought by a plugin for one of its boards, or by one of its boards itself (boards.json)."""
    for need in manifest['requires']['plugins']:
        other = known.get(need)
        if other is None:
            return f'requires.plugins: {need} is not in the index'
        if not shares_board(manifest['boards'], other['boards']):
            return f'requires.plugins: {need} is made for other boards ({", ".join(other["boards"])})'
    wanted = dict.fromkeys(manifest['requires']['features'])
    for part in manifest['parts']:
        wanted.update(dict.fromkeys(part['features']))
    for feature in wanted:
        if any(feature in other['provides'] and other['id'] != manifest['id']
               and shares_board(manifest['boards'], other['boards']) for other in known.values()):
            continue
        boards = board_features()
        if any(feature in features and (manifest['boards'] == 'any' or board in manifest['boards'])
               for board, features in boards.items()):
            continue
        return f'needs the feature {feature}, and no plugin in the index and no board brings it for its boards'
    return None


def needs_cycle(known):
    """A circle of plugins that need each other, as the ids around it ([a, b, a]), or None."""
    state = {}

    def visit(plugin, path):
        state[plugin] = 'open'
        for need in known[plugin]['requires']['plugins']:
            if need not in known:
                continue
            if state.get(need) == 'open':
                return path[path.index(need):] + [need]
            if need not in state:
                found = visit(need, path + [need])
                if found:
                    return found
        state[plugin] = 'done'
        return None

    for plugin in sorted(known):
        if plugin not in state:
            found = visit(plugin, [plugin])
            if found:
                return found
    return None


def index_manifests():
    """{id: checked manifest} of the community plugins of the committed index.json, read as the app reads them
    (leniently). This repository's own plugins are read from plugins/ instead, as they are now."""
    path = ROOT / 'index.json'
    known = {}
    if not path.is_file():
        return known
    for item in json.loads(path.read_text(encoding='utf-8')).get('plugins') or []:
        if not isinstance(item, dict) or item.get('label') == 'tessera':
            continue
        try:
            manifest = pm().check(item['manifest'], strict=False)
        except (pm().ManifestError, KeyError, TypeError):
            continue
        known[manifest['id']] = manifest
    return known


def own_manifests():
    """{id: checked manifest} of this repository's own plugins, as they are in plugins/ now."""
    known = {}
    for folder in sorted((ROOT / 'plugins').iterdir()) if (ROOT / 'plugins').is_dir() else []:
        if (folder / 'tessera-plugin.yaml').is_file():
            manifest = pm().check(yaml.safe_load((folder / 'tessera-plugin.yaml').read_text(encoding='utf-8')),
                                  strict=False)
            known[manifest['id']] = manifest
    return known


def check_needs(manifests, known):
    """Every plugin of `manifests` ({id: checked}) finds what it needs among `known` and them; no circle. SystemExit."""
    known = {**known, **manifests}
    for plugin, manifest in sorted(manifests.items()):
        problem = needs_problem(manifest, known)
        if problem:
            fail(plugin, problem)
    cycle = needs_cycle(known)
    if cycle:
        fail('requires.plugins', f'plugins that need each other: {" -> ".join(cycle)}')


# ---- The files of this repository ----

ENTRY_FIELDS = {'repo', 'path', 'ref', 'maintainer', 'topics'}
GITHUB = re.compile(r'^https://github\.com/([A-Za-z0-9_.-]+?)/([A-Za-z0-9_.-]+?)(?:\.git)?/?$')
GITHUB_FOLDER = re.compile(r'^https://github\.com/([A-Za-z0-9_.-]+?)/([A-Za-z0-9_.-]+?)(?:\.git)?'
                           r'(?:/tree/[^/]+(?:/(.+?))?)?/?$')


def normal_repo(url):
    """A repository's URL as the index writes and compares it: lowercase owner and name, no .git, no trailing slash."""
    match = GITHUB.match(str(url or '').strip())
    if not match:
        raise ValueError(f'{url} is no GitHub repository')
    return f'https://github.com/{match.group(1).lower()}/{match.group(2).lower()}'


def normal_path(path):
    path = str(path or '.').strip().strip('/')
    while path.startswith('./'):
        path = path[2:]
    return path or '.'


def place(url):
    """(repository, folder) of a link to a plugin: https://github.com/<owner>/<repo>, with /tree/<ref>/<folder> when it is
    in a folder (the ref says nothing about where the plugin lives)."""
    match = GITHUB_FOLDER.match(str(url or '').strip())
    if not match:
        raise ValueError(f'{url} is no link to a GitHub repository or a folder in one')
    return f'https://github.com/{match.group(1).lower()}/{match.group(2).lower()}', normal_path(match.group(3))


def check_entry(path):
    """An entry of community/<id>.yaml: a public GitHub repository of the maintainer, the plugin's folder in it, and
    optionally the release tag to list (the newest release when left out). SystemExit with what is wrong."""
    path = Path(path)
    try:
        entry = yaml.safe_load(path.read_text(encoding='utf-8'))
    except (OSError, yaml.YAMLError) as error:
        fail(path.name, str(error))
    if not isinstance(entry, dict) or set(entry) - ENTRY_FIELDS or not {'repo', 'maintainer'} <= set(entry):
        fail(path.name, f'an entry has repo and maintainer, and may have {", ".join(sorted(ENTRY_FIELDS - {"repo", "maintainer"}))}')
    if not re.fullmatch(r'[a-z][a-z0-9_]{0,31}', path.stem):
        fail(path.name, 'the file is named after the plugin\'s id: community/<id>.yaml')
    match = re.fullmatch(r'https://github\.com/([A-Za-z0-9_.-]+)/([A-Za-z0-9_.-]+)', str(entry['repo']))
    if not match:
        fail(path.name, 'repo is https://github.com/<owner>/<repository>')
    if str(entry['maintainer']).lower() != match.group(1).lower():
        fail(path.name, 'the maintainer is the owner of the repository')
    if '..' in str(entry.get('path', '.')) or str(entry.get('path', '.')).startswith('/'):
        fail(path.name, 'path is a folder inside the repository')
    return entry


def read_featured():
    """featured.yaml: the ids of the plugins Tessera recommends to start with, in its order."""
    path = ROOT / 'featured.yaml'
    if not path.is_file():
        return []
    try:
        featured = yaml.safe_load(path.read_text(encoding='utf-8')) or []
    except yaml.YAMLError as error:
        fail('featured.yaml', str(error))
    if not isinstance(featured, list) or not all(isinstance(i, str) and re.fullmatch(r'[a-z][a-z0-9_]{0,31}', i)
                                                 for i in featured):
        fail('featured.yaml', 'a list of plugin ids')
    return list(dict.fromkeys(featured))


def read_transfers():
    """transfers.yaml: ids that moved to another repository or folder, with the consent of the one that had it
    (docs/PUBLISHING.md, "Whose id it is"). [{plugin, from: (repo, folder), to: (repo, folder), reason}]."""
    path = ROOT / 'transfers.yaml'
    if not path.is_file():
        return []
    try:
        moves = yaml.safe_load(path.read_text(encoding='utf-8')) or []
    except yaml.YAMLError as error:
        fail('transfers.yaml', str(error))
    out = []
    for i, move in enumerate(moves if isinstance(moves, list) else [None]):
        if not isinstance(move, dict) or set(move) != {'plugin', 'from', 'to', 'reason'} \
                or not re.fullmatch(r'[a-z][a-z0-9_]{0,31}', str(move['plugin'])) or not str(move['reason']).strip():
            fail('transfers.yaml', f'[{i}]: a move is {{plugin, from, to, reason}}')
        try:
            out.append({'plugin': move['plugin'], 'from': place(move['from']), 'to': place(move['to']),
                        'reason': str(move['reason'])})
        except ValueError as error:
            fail('transfers.yaml', f'[{i}]: {error}')
    return out


def main():
    folders = [Path(arg) for arg in sys.argv[1:]] or [
        *(f for f in sorted((ROOT / 'plugins').iterdir()) if (f / 'tessera-plugin.yaml').is_file()), ROOT / 'template']
    checked = {}
    for folder in folders:
        raw, _, _ = check_folder(folder)
        manifest = pm().check(raw)
        checked[manifest['id']] = manifest
        print(f'{folder.name}: ok')
    # What the plugins need, against the index as it would be: the community plugins of the committed index.json, this
    # repository's own plugins, and the folders checked (the template needs nothing and brings nothing).
    known = {**index_manifests(), **own_manifests()}
    check_needs(checked if sys.argv[1:] else known, known)
    print('needs: ok')
    if not sys.argv[1:]:
        for entry in sorted((ROOT / 'community').glob('*.yaml')):
            check_entry(entry)
            print(f'community/{entry.name}: ok')
        read_featured()
        read_transfers()
        print('featured.yaml, transfers.yaml: ok')
        check_docs()
        print('docs: ok')
    print(f'plugin API {plugin_api()}: {len(folders)} checked')


if __name__ == '__main__':
    main()
