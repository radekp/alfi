#!/bin/sh
# http://playground.arduino.cc/Interfacing/LinuxTTY
stty -F /dev/arduino cs8 115200 ignbrk -brkint -icrnl -imaxbel -opost -onlcr -isig -icanon -iexten -echo -echoe -echok -echoctl -echoke noflsh -ixon -crtscts
sleep 2
./alfi
