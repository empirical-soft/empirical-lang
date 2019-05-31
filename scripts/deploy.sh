#!/bin/bash

# Run this script after a build to copy the binary to the web repo

cd build
zip deploy.zip empirical
if [[ $(uname) == "Darwin" ]]; then
  mv deploy.zip ../mac.zip
else
  mv deploy.zip ../linux.zip
fi

