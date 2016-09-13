#!/bin/bash

m=$(sudo hcitool lescan>results.txt)
file="/home/pi/results.txt" #Name of the file where lescan results are stored.
devices=$(cat "$file")

echo "You are here..."
#echo $name
mac="68:9E:19:1F:C1:61"
echo $mac
dir="/home/pi/Downloads/bluez-5.41/"
cd $dir
make && sudo make install
cd "attrib"
handle="0x0013"
val="0100"
bhnd="0x002c"
c=$(sudo hcitool lecc $mac)
if [[ $c = "Connection handle 64" ]]
then rssi=$(sudo hcitool rssi $mac) | echo "Signal strength is: " | echo $rssi
else echo "Error while connecting to device."
fi
battery=$(./gatttool -b $mac --char-read -a $bhnd)
echo $battery
sleep 1
./gatttool -b $mac --char-write-req -a $handle -n $val --listen

