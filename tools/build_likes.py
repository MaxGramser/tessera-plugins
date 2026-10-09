#!/usr/bin/env python3
"""Write likes.json: how many people like each plugin of the index, copied from Tessera's website.

    python3 tools/build_likes.py

A like is given in the Tessera app and counted by the website; the app reads the counts from this file on GitHub, as it
reads index.json, and never asks the website. The file holds only plugins of index.json, sorted, and no time, so it
changes only when a count does. When the website does not answer (an error, a timeout, an answer that is no list of
counts), the file stays as it is. CI runs this after tools/build_index.py every hour (.github/workflows/index.yml).
"""
import json
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = 'https://tessera-maxgramser.on-forge.com/api/v1/addon/plugins/likes'
LIKES = ROOT / 'likes.json'
FORMAT = 1


def counts():
    """{id: count} from the website, or None (and why on stderr) when it gave none."""
    request = urllib.request.Request(SOURCE, headers={'Accept': 'application/json', 'User-Agent': 'tessera-plugins'})
    try:
        with urllib.request.urlopen(request, timeout=20) as response:
            data = json.loads(response.read(512 * 1024))['data']
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f'likes: the website gave no counts ({error}); likes.json stays as it is', file=sys.stderr)
        return None
    if not isinstance(data, dict) or not all(isinstance(n, int) and not isinstance(n, bool) and n >= 0
                                             for n in data.values()):
        print('likes: the website\'s answer is no list of counts; likes.json stays as it is', file=sys.stderr)
        return None
    return data


def main():
    found = counts()
    if found is None:
        if not LIKES.is_file():
            LIKES.write_text(json.dumps({'format': FORMAT, 'likes': {}}, indent=1) + '\n')
        return
    listed = {plugin['id'] for plugin in json.loads((ROOT / 'index.json').read_text()).get('plugins') or []}
    likes = {'format': FORMAT, 'likes': {plugin: found[plugin] for plugin in sorted(found) if plugin in listed}}
    text = json.dumps(likes, indent=1, ensure_ascii=False) + '\n'
    if LIKES.is_file() and LIKES.read_text() == text:
        print('likes.json unchanged')
        return
    LIKES.write_text(text)
    print(f'likes.json: {len(likes["likes"])} plugins')


if __name__ == '__main__':
    main()
