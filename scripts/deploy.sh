#!/bin/sh

# Run this script after a build to copy the binary to the web repo

cd build
zip deploy.zip empirical
if [[ $(uname) == "Darwin" ]]; then
  mv deploy.zip ../../website/mac.zip
else
  mv deploy.zip ../../website/linux.zip
fi

