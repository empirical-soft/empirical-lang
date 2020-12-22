# Maintainer's Notes

## Pull Requests

PRs are accepted as a **squash** merge. (All other types are disabled.)

GitHub adds more "content" to the commit message, like the PR number and sometimes the individual commits. Delete these; just go with the original commit message from the PR author.

After the merge, update local and remote repos:

```
$ ./scripts/gh_update.sh
```

## Deploy

Simply tag a version number:

```
$ git tag -a 1.2.3
```

Give bullet-points in the tag message:

```
- Added features
- Fixed bugs
```

Then push it back to GitHub:

```
$ git push upstream master --tags
```

Check the [Releases page](https://github.com/empirical-soft/empirical-lang/releases) to ensure that the binaries are present. (This takes a while since the CI must rebuild everything.)

Then update the [website](https://github.com/empirical-soft/empirical-soft.github.io):

```
$ make website
$ cd ../website
$ git commit -am "Bumped to 1.2.3"
$ git push origin master
```
