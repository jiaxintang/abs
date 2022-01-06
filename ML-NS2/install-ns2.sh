#!/bin/sh
#!/bin/bash

sudo apt-get -y --force-yes install build-essential
sudo apt-get -y --force-yes install tcl8.5 tcl8.5-dev tk8.5 tk8.5-dev
sudo apt-get -y --force-yes install libxmu-dev libxmu-headers

cd ns-allinone-2.35
./install
cd ../CoDel/ns-allinone-2.35
./install
cd ../../
