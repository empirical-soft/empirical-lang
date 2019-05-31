#!/bin/bash

# Generate changelog for web repo
# (We need the sed because Git replaces '\n' with ' ' for subject fields.)

git tag -l --format="#### %(refname:strip=2)%09(%(creatordate:short))%0a%0a%(subject)%0a" --sort=-v:refname "*.*.*" | sed 's/ - /\
- /g' > ../website/_includes/changelog.md

# Generate table of contents for tutorial

cd doc
cat tutorial_running.md tutorial_core.md | ../thirdparty/github-markdown-toc/gh-md-toc - | sed "s/^      //" > ../../website/_includes/tutorial_toc.md
cp tutorial*.md ../../website/_includes/

