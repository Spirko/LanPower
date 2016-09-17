# LanPower
Arduino project to controls power to my LAN devices.

My Cable Modem needs to be reset way too often, and it usually requires the Router to be reset afterwards.
This program runs on an Arduino Nano, and it controls 4 relays to turn my LAN devices on and off.

Initially (and upon button pres), the modem is power cycled, then the LAN is reset 3 minutes later.
A LAN reset consists of power cycling the router and then a switch and WiFi extender a minute later.

In the Arduino code, I debounce button presses and turn the button state into press and release events.
Homemade Alarms are used to turn the devices on and off.
