#!/usr/bin/env bash

sudo apt update
sudo apt install -y git wget flex bison gperf python3-venv python3-setuptools cmake ninja-build ccache libffi-dev libssl-dev dfu-util

git clone -b release/v5.0 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh 
. ./export.sh
cd ..
idf.py set-target esp32c3
idf.py build
idf.py uf2
cd build
esptool.py --chip esp32 merge_bin -o longrange-esp32c3-mqtt.bin @flash_args
cp longrange-esp32c3-mqtt.bin  ..
cp uf2.bin  ..