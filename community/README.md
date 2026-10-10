# Community plugins

One file per plugin that lives in a repository of its own. Add yours with a pull request; after that, every release
you publish reaches the app by itself within the hour (docs/PUBLISHING.md, "Your own plugin, in a repository of your
own", which also says what is checked and what is yours to get right). Listing is not a review (docs/PUBLISHING.md,
"What listing means").

```yaml
# community/<id>.yaml
repo: https://github.com/someone/tessera-something
path: .                  # the folder with tessera-plugin.yaml
maintainer: someone      # the owner of the repository, who opens the pull request
# ref: v1.0.0            # only to hold the list at one release; without it the list follows the newest
```
