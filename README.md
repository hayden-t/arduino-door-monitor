# arduino-door-monitor

https://hackaday.io/project/191066-dementia-support-unit-bedroom-door-monitor

An Arduino based alert and logging system, including portable receivers for tracking 30+ room doorway motion sensors.

Folders:

doors_base - source code for the ATmega 256 base unit that collects, logs, displays, alerts and pushes alerts to the mobile units

doors_mobile - source code for the ATmega 328 handheld portable units that receive event broadcasts from the base unit

doors_node - source code for doorway monitor node, discontinued instead to wire the sensors to a shared hub node

doors_relay - source code for the shared node hubs that are wired to the motion sensors and relay event via radio to the hub

hardware - arduino ide board profiles for running a 328 at 1.8v brownout 

images - self explanatory

libraries - all the arduino libraries needed for the source code

pcb - fritzing pcb layouts for the doors_node units


layout.odg - a map of the rooms showing tree network of the different units

manual.odg - a manual for end user reference

