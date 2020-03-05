## OPEN-LED-RACE wireless remote

[**OpenLedRace**](https://openledrace.net) is a game that uses an addressable LED strip as a track where cars are represented as lights. It is controlled by an Arduino board. It uses single button controllers to use it. In original setup these controllers are cabled into Arduino board.

In order to give more flexibility I've implemented wireless controllers that make use of my library project [**EnigmaIOT**](https://github.com/gmag11/EnigmaIOT). It allows communicating nodes with a gateway using ESP-NOW protocol. This means a low latency.

Controllers are made over ESP8266 platform and runs on a single 3.7V lithium battery and receiver is an EnigmaIOT gateway flashed over a ESP32 (although it can run on a ESP8266 too).

I've designed an ergonomic controller housing to make it ideal for children. They can play with it while they move around the track.

This repository contains all code, binaries and 3D designs needed to convert your OpenLedRace track to use wireless controllers.

Of course, it may be used to control other single button projects.