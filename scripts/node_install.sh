#!/bin/sh

if [ ! -d "blst/src" ]; then
  git clone --depth 1 -b master https://github.com/supranational/blst.git
fi

cd bindings/node.js

yarn install
