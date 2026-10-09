# Publishing a plugin

## Where a plugin comes from

A plugin reaches the Tessera app in one of four ways:

| Way | What a screen builds | What shows as an update |
|---|---|---|
| The index (this repository) | The commit the index names | A newer version in the index |
| A link to a repository with a release | The commit of the newest release, when it was read | A newer release |
| A link to a branch, or to a repository without a release (its default branch) | The commit the branch was at, when it was read | A newer commit on that branch |
| A folder in Home Assistant's config, `tessera-plugins/<id>/` | What the folder holds, at every build | Nothing: every build takes the folder |

Every way but a folder is pinned to one commit, so what a screen builds is what the editor showed. The app reads the
index at most every ten minutes and asks a linked repository for a newer release or commit at most every hour. A newer
one shows as an update on the screen's Plugins tab and on the Plugins page, and one tap builds it. Nothing reaches a
screen by itself. An update keeps what was filled in when the plugin was added; one that asks for other rights than
before waits for a yes.

A link follows its repository by GitHub's number for it, not by its name: a repository you rename keeps its screens, and
a new repository that takes an old name is never built.

## Which plugin: id and origin

The id is a plugin's name for ever. It is in the firmware, in every layout with one of its tiles
(`plugin:<id>.<tile>`) and in the secrets filled in for it: never change it.

Where a plugin comes from is its origin: `github.com/<owner>/<repo>`, with `/<folder>` when the plugin is in a folder of
the repository, or a folder in Home Assistant's config. Two plugins can have one id (a fork, a copy being made), and the
origin says which of them a screen runs.

- A secret is kept for the origin it was filled in for. A fork with the same id never gets it.
- A screen that runs another origin of an id is not updated to the index's plugin of that id. The person switches in the
  plugin's details, when they want to.

## The index

The app reads one file for the list: `index.json` in this repository (on `main`). It lists every plugin with its
manifest, its texts and its README at one commit, the plugin versions that are blocked, the plugins Tessera recommends,
and whose each id is. `tools/build_index.py` writes it; nobody edits it by hand. The app reads it with a conditional
request and keeps the last one for when the internet is away. Beside it, `likes.json` has how many people like each
plugin (below).

## Tessera's own plugins: `plugins/`

The plugins in `plugins/` are made and reviewed by Tessera. Each is pinned in the index to the last commit that changed
its folder, so a change elsewhere in this repository never makes a screen build something new.

To change one:

1. Change the plugin and raise its `version` in `tessera-plugin.yaml`.
2. `python3 tools/check.py plugins/<id>` and a build on a real screen ([TESTING.md](TESTING.md)).
3. Commit, then `python3 tools/build_index.py` and commit `index.json` (CI does the second step after a merge too).

## Your own plugin, in a repository of your own

A plugin can live in a repository of its own, made from [`template/`](../template)
(`python3 tools/new_plugin.py <id> ../my-plugin`). It stays yours: you publish its releases, you answer its issues, and
Tessera never needs a pull request for a new version.

There are two ways for people to find it:

- **A link.** Anyone can add it in the app under Plugins, **Add with a link**: `https://github.com/<you>/<repo>`, or
  `https://github.com/<you>/<repo>/tree/main/<folder>` when the plugin is in a folder. With a release the app takes the
  newest, and every release you publish after it shows as an update. Without a release the link is a test of your
  default branch (the simplest way to try a plugin: push, and add the link), and every newer push shows as an update.
  To test another branch of a repository with releases, choose **Test a branch** and name it. Nothing goes through this
  repository.
- **The community list.** List it once, and the Plugins page shows it to everyone with the label Community:

  1. Make a release in your repository (a tag with a GitHub release).
  2. Open a pull request here that adds one file, `community/<id>.yaml`, named after the plugin's id:

     ```yaml
     repo: https://github.com/someone/tessera-bin-day
     path: .                        # the folder with tessera-plugin.yaml
     maintainer: someone            # you: the owner of the repository
     ```

  3. CI checks that you own the repository and that your release passes `tools/check.py`. Once it is merged, the
     index follows your releases by itself: it is built again every hour and takes your newest release, checked with
     the same rules, pinned to its commit. A release that does not pass stays out, and the one before it stays listed.

  Add `ref: v1.2.0` only to hold the list at one release; without it the list follows your newest.

What a community plugin needs:

1. The person who lists it owns the repository (or is a member of its organisation).
2. The repository is public, has issues on, and has at least one release.
3. `tools/check.py` passes on the release.
4. Its licence goes with AGPL-3.0.
5. It builds on the boards it names (or on Tessera's sample boards for `boards: any`).
6. Its permissions name everything its code does.
7. What it needs is in the index: every plugin of `requires.plugins`, and a plugin or a board for every feature of
   `requires.features`, for one of its boards.

Tessera does not review community plugins or their releases: the app says so before anyone adds one, and asks them to
trust the maker. What it can do is stop a version: `blocked.yaml` (below) refuses it in every app, and a plugin that
breaks the rules leaves the list.

## Whose id it is

In the index an id belongs to the repository and the folder that first published it. `index.json` keeps that under
`claims`, also for a plugin that is left out for a while (a release that fails the check). A community entry under an id
that another repository or folder published first is left out of the index, with the reason in the index workflow's
log. Repositories are known by GitHub's number for them here too: a repository you rename keeps its ids, and a new
repository that takes an old name gets none of them. Tessera's own `plugins/` win a clash with a community entry.

To hand an id on (you move the plugin to another repository, or give it to someone else), add a line to
`transfers.yaml` in a pull request, from the account that has the id or with its consent in the pull request:

```yaml
- plugin: bin_day
  from: https://github.com/someone/tessera-bin-day
  to: https://github.com/someone-else/tessera-plugins/tree/main/bin_day
  reason: The maker handed it over in the pull request.
```

`from` and `to` are a repository, with `/tree/<branch>/<folder>` when the plugin is in a folder. The index then takes
the plugin from its new place, and the id belongs there from then on. A screen that runs the plugin from its old place
keeps that origin until the person switches.

## Finding a plugin

The Plugins page has a tab per type (Tiles, Functions, Hardware, read from the manifest: [MANIFEST.md](MANIFEST.md),
"The type of plugin") and one for what the screens have In use. The maker's `topics` are chips under the tabs. The list
is sorted by most liked, or by newest or by name; a filter shows what Tessera made, what the community made, or both;
and "Only what fits my screens" folds away what fits none of them, with the reason.

`featured.yaml` lists the plugins Tessera recommends, chosen by hand. The index carries them as `featured` (an id that
is not in the index is left out), and on the Plugins page a recommendation breaks a tie in the number of likes.

## Likes

A like is a heart a person gives a plugin in the Tessera app: a thank-you to its maker and a hint for others. Only a
plugin of the index that runs on one of their screens can have their like, one per app installation, and the app asks
once that the like counts in a public number. Tessera's website counts the likes, and `tools/build_likes.py` copies the
counts into `likes.json` every hour, in the index's commit. The app reads them from there, never from the website. The
file holds only plugins of the index.

## Labels in the app

| Label | Means |
|---|---|
| From Tessera | In `plugins/` of this repository, made and reviewed by Tessera, or a release of one of Tessera's own repositories added with a link. |
| Community | Listed through `community/`, or a release added with a link to its repository: checked automatically, not reviewed. Its maker publishes its updates. |
| Test | A branch, a repository without a release, or a folder in Home Assistant's config: someone's work in progress. |

## Blocked versions

`blocked.yaml` lists versions the app refuses to build, with the reason; a screen that already runs one shows the
reason in its Plugins tab, and its next build leaves the plugin out.

```yaml
- plugin: some_plugin
  versions: ["1.0.0"]
  reason: Sends the API key to a host it does not name.
```

## Versions

- `version` in the manifest: three numbers. Raise the last for a fix, the middle for something new, the first when a
  tile's options change in a way that loses what people set.
- `api`: the plugin API the plugin was written for. It builds on every core with the same major and at least that
  minor; a core with an older API refuses it and says why. While the API is 0.x a minor may still change a name or a
  signature, and Tessera's own plugins move with it. From 1.0 on only a break raises the major.
