#!/bin/bash
# update and initialize submodules after checkout
git submodule update --init --recursive

# install dependencies
xargs sudo apt-get -y install < requirements.txt