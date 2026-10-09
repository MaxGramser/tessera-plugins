#!/usr/bin/env python3
"""Write index.json: every plugin the Tessera app can offer, with its manifest, texts and README at a fixed commit.

    python3 tools/build_index.py            # writes index.json
    python3 tools/build_index.py --check    # fails when index.json is not what it would write
    python3 tools/build_index.py --entries [community/<id>.yaml ...]   # checks community entries at their release

Two kinds of entry:
- plugins/<id>/ in this repository. Each is pinned to the last commit that changed its folder, so a change elsewhere
  in the repository never makes a screen build something new.
- community/<id>.yaml: a plugin in its own repository (repo, path, maintainer, and a release tag, or none to follow the
  newest release). It is cloned at that release, checked with the same rules as Tessera's own (tools/check.py), and
  pinned to the commit. One that fails is left out of the index with the reason on stderr; the others still go in, and
  a release that fails leaves the plugin out until a release passes.

Whose id it is: an id belongs to the repository and folder that first published it in the index. index.json keeps that
in `claims`, also for an id that is not listed for a while, and an entry of another repository or folder under the id
is left out, with the reason on stderr, unless transfers.yaml lists the move. Repositories are compared by GitHub's
number for them (`repo_id`), so a renamed repository keeps its ids and a new repository under an old name gets none.
Tessera's own plugins/ win a clash with a community entry. A plugin that needs a plugin or a feature the index does not
have is left out too (tools/check.py, "Across the index").

featured.yaml names the plugins Tessera recommends to start with (`featured`, only ids that are listed). The plugin
API is the one of the manifest module, fetched again for every build. tools/build_likes.py writes likes.json after it.

The app reads index.json from raw.githubusercontent.com (main), at most every ten minutes, and runs nothing from it: a
manifest is a description (docs/MANIFEST.md). CI runs this script after every merge and every hour, so a maker's new
release reaches the app within the hour without a pull request (.github/workflows/index.yml).
"""
import json
import os
import subprocess
import sys
import tempfile
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[1]
INDEX = ROOT / 'index.json'
sys.path.insert(0, str(ROOT / 'tools'))
from check import (check_entry, check_folder, check_needs, index_manifests, needs_cycle, needs_problem,  # noqa: E402
                   normal_path, normal_repo, own_manifests, plugin_api, pm, read_featured, read_transfers)

REPO = normal_repo('https://github.com/MaxGramser/tessera-plugins')


def git(*args):
    return subprocess.run(['git', '-C', str(ROOT), *args], check=True, capture_output=True, text=True).stdout.strip()


def github(path):
    """An answer of GitHub's API (with GITHUB_TOKEN when CI has one, without its hourly limit)."""
    request = urllib.request.Request(f'https://api.github.com/{path}',
                                     headers={'Accept': 'application/vnd.github+json', 'User-Agent': 'tessera-plugins',
                                              **({'Authorization': f'Bearer {os.environ["GITHUB_TOKEN"]}'}
                                                 if os.environ.get('GITHUB_TOKEN') else {})})
    with urllib.request.urlopen(request, timeout=20) as response:
        return json.load(response)


def latest_release(owner, repo):
    """The tag of a repository's newest release."""
    return github(f'repos/{owner}/{repo}/releases/latest')['tag_name']


REPO_IDS = {}


def repo_id(repo):
    """GitHub's number of a repository (https://github.com/<owner>/<repo>): it stays the same through a rename, and a
    new repository that takes an old name gets another."""
    if repo not in REPO_IDS:
        owner, name = repo.split('/')[-2:]
        REPO_IDS[repo] = int(github(f'repos/{owner}/{name}')['id'])
    return REPO_IDS[repo]


def own_plugin(folder, own_id):
    manifest, translations, readme = check_folder(folder)
    sha = git('log', '-1', '--format=%H', '--', str(folder.relative_to(ROOT)))
    date = git('log', '-1', '--format=%cs', '--', str(folder.relative_to(ROOT)))
    if not sha:
        raise SystemExit(f'{folder.name}: not committed yet; commit it, then build the index')
    return {
        'id': manifest['id'], 'repo': REPO, 'repo_id': own_id, 'path': normal_path(folder.relative_to(ROOT)),
        'label': 'tessera', 'status': 'ok', 'release': {'version': manifest['version'], 'sha': sha, 'date': date},
        'manifest': manifest, 'translations': translations, 'readme': readme,
    }


def community_plugin(path):
    """An entry of community/: the plugin at its release, checked, pinned to the commit; None (and why on stderr) when
    it cannot go in."""
    try:
        entry = check_entry(path)
        repo = normal_repo(entry['repo'])
        owner, name = repo.split('/')[-2:]
        number = repo_id(repo)
        tag = entry.get('ref') or latest_release(owner, name)
        with tempfile.TemporaryDirectory() as tmp:
            subprocess.run(['git', 'clone', '--quiet', '--depth', '1', '--branch', tag, repo, tmp], check=True,
                           capture_output=True, text=True, timeout=120)
            sha = subprocess.run(['git', '-C', tmp, 'rev-parse', 'HEAD'], check=True, capture_output=True, text=True).stdout.strip()
            date = subprocess.run(['git', '-C', tmp, 'log', '-1', '--format=%cs'], check=True, capture_output=True,
                                  text=True).stdout.strip()
            folder = Path(tmp) / entry.get('path', '.')
            manifest, translations, readme = check_folder(folder)
    except (SystemExit, subprocess.SubprocessError, OSError, ValueError, KeyError) as error:
        print(f'{path.stem}: left out ({error})', file=sys.stderr)
        return None
    if manifest['id'] != path.stem:
        print(f'{path.stem}: left out (its manifest says id {manifest["id"]})', file=sys.stderr)
        return None
    return {
        'id': manifest['id'], 'repo': repo, 'repo_id': number, 'path': normal_path(entry.get('path', '.')),
        'label': 'community', 'status': 'ok', 'release': {'version': manifest['version'], 'sha': sha, 'date': date, 'tag': tag},
        'manifest': manifest, 'translations': translations, 'readme': readme,
    }


# ---- Whose id it is ----

def claims_of(old):
    """{id: {repo, repo_id, path}} from the committed index: its `claims`, and the entries of an index from before them."""
    claims = {}
    for item in [*(old.get('plugins') or []), *({'id': k, **v} for k, v in (old.get('claims') or {}).items())]:
        try:
            claims[item['id']] = {'repo': normal_repo(item['repo']), 'repo_id': item.get('repo_id'),
                                  'path': normal_path(item.get('path'))}
        except (KeyError, TypeError, ValueError):
            continue
    return claims


def same_place(claim, entry):
    """Whether an entry is where its id's claim says: the same repository (by GitHub's number when both have it) and
    the same folder."""
    if claim['path'] != entry['path']:
        return False
    if claim.get('repo_id') and entry.get('repo_id'):
        return claim['repo_id'] == entry['repo_id']
    return claim['repo'] == entry['repo']


def claim_problem(entry, claims, transfers):
    """Why an entry cannot have its id (another repository or folder published it first, and transfers.yaml does not
    move it here), or None."""
    claim = claims.get(entry['id'])
    if claim is None or same_place(claim, entry):
        return None
    at, seen = (claim['repo'], claim['path']), set()
    while at not in seen:
        seen.add(at)
        move = next((m for m in transfers if m['plugin'] == entry['id'] and m['from'] == at), None)
        if move is None:
            break
        at = move['to']
        if at == (entry['repo'], entry['path']):
            return None
    where = claim['repo'] + ('' if claim['path'] == '.' else f' ({claim["path"]})')
    return f'the id belongs to {where}, which published it first; transfers.yaml can move it'


def needs_met(plugins):
    """The plugins whose needs the index meets. A community plugin that needs what is not there is left out (and in
    turn what needed it), with the reason on stderr; one of Tessera's own stops the build."""
    while True:
        known = {p['id']: pm().check(p['manifest'], strict=False) for p in plugins}
        out = []
        for plugin in plugins:
            problem = needs_problem(known[plugin['id']], known)
            if problem and plugin['label'] == 'tessera':
                raise SystemExit(f'{plugin["id"]}: {problem}')
            if problem:
                print(f'{plugin["id"]}: left out ({problem})', file=sys.stderr)
                continue
            out.append(plugin)
        cycle = needs_cycle({p['id']: known[p['id']] for p in out})
        if cycle:
            community = [p for p in out if p['id'] in cycle and p['label'] != 'tessera']
            if not community:
                raise SystemExit(f'plugins that need each other: {" -> ".join(cycle)}')
            for plugin in community:
                print(f'{plugin["id"]}: left out (plugins that need each other: {" -> ".join(cycle)})', file=sys.stderr)
            out = [p for p in out if p not in community]
        if len(out) == len(plugins):
            return out
        plugins = out


def own_repo_id(claims):
    """GitHub's number of this repository; offline, the one the index has for it."""
    try:
        return repo_id(REPO)
    except (OSError, ValueError, KeyError) as error:
        known = next((c['repo_id'] for c in claims.values() if c['repo'] == REPO and c.get('repo_id')), None)
        if known is None:
            raise SystemExit(f'GitHub did not say the number of {REPO} ({error})')
        return known


def build(old):
    claims = claims_of(old)
    transfers = read_transfers()
    own_id = own_repo_id(claims)
    plugins = [own_plugin(folder, own_id) for folder in sorted((ROOT / 'plugins').iterdir())
               if (folder / 'tessera-plugin.yaml').is_file()]
    own = {plugin['id'] for plugin in plugins}
    for path in sorted((ROOT / 'community').glob('*.yaml')):
        found = community_plugin(path)
        if not found or found['id'] in own:
            continue
        problem = claim_problem(found, claims, transfers)
        if problem:
            print(f'{found["id"]}: left out ({problem})', file=sys.stderr)
            continue
        plugins.append(found)
    plugins = needs_met(plugins)
    for plugin in plugins:
        claims[plugin['id']] = {'repo': plugin['repo'], 'repo_id': plugin['repo_id'], 'path': plugin['path']}
    listed = {plugin['id'] for plugin in plugins}
    featured = read_featured()
    for plugin in featured:
        if plugin not in listed:
            print(f'featured.yaml: {plugin} is not in the index', file=sys.stderr)
    blocked = yaml.safe_load((ROOT / 'blocked.yaml').read_text()) or []
    return {'format': 1, 'plugin_api': plugin_api(), 'blocked': blocked,
            'featured': [plugin for plugin in featured if plugin in listed],
            'claims': dict(sorted(claims.items())), 'plugins': plugins}


def entries(paths):
    """--entries: every community entry named (all without names) passes at its release, may have its id, and finds
    what it needs in the index; else SystemExit."""
    paths = [Path(p) for p in paths] or sorted((ROOT / 'community').glob('*.yaml'))
    old = json.loads(INDEX.read_text()) if INDEX.is_file() else {}
    claims, transfers, own = claims_of(old), read_transfers(), own_manifests()
    failed, found = [], {}
    for path in paths:
        entry = community_plugin(path)
        problem = entry and ('the id is one of Tessera\'s own plugins' if entry['id'] in own
                             else claim_problem(entry, claims, transfers))
        if problem:
            print(f'{entry["id"]}: not listed ({problem})', file=sys.stderr)
        if entry is None or problem:
            failed.append(path.stem)
            continue
        found[entry['id']] = pm().check(entry['manifest'])
    if failed:
        raise SystemExit(f'not listed: {", ".join(failed)} (the reasons are above)')
    check_needs(found, {**index_manifests(), **own})
    print(f'{len(paths)} community entries pass')


def main():
    pm(fresh=True)
    if '--entries' in sys.argv:
        entries([arg for arg in sys.argv[1:] if arg != '--entries'])
        return
    old = json.loads(INDEX.read_text()) if INDEX.is_file() else {}
    index = build(old)
    same = {k: v for k, v in old.items() if k != 'generated'} == index
    if '--check' in sys.argv:
        if not same:
            raise SystemExit('index.json is out of date: run python3 tools/build_index.py')
        print('index.json is up to date')
        return
    if same:
        print('index.json unchanged')
        return
    index = {'format': 1, 'generated': datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ'), **index}
    INDEX.write_text(json.dumps(index, indent=1, ensure_ascii=False) + '\n')
    print(f'index.json: {len(index["plugins"])} plugins')


if __name__ == '__main__':
    main()
