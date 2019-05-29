#!/bin/sh

# Update my GitHub repo; optionally remove a branch

git checkout master

if [[ $1 != "" ]];
then
  git push --delete origin $1
  git branch -D $1
fi

git pull upstream master
git push origin master

