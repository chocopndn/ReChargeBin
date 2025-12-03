# ReChargeBin: An Arduino-Based Solar Charging Station Powered by Plastic Bottles

**Submitted to:** [Christian R. Remigio](https://web.facebook.com/Ichan0201) - Application Development

## Overview

ReChargeBin uses plastic bottles as the trigger for a solar-powered charging station. Arduino 2 measures bottle height and sends the size to Arduino 1. Arduino 1 weighs the bottle using an HX711 load cell. If the bottle passes size and weight checks, the system activates a relay timer that gives charging time. The solar panel rotates using LDR sensors to track sunlight. The bin detects when it is full and blocks new bottles.

## Why Two Arduino Uno Boards Are Used

- Tasks are split to avoid timing conflicts:
  - Arduino 1 handles weighing, relays, and timers.
  - Arduino 2 handles sensors, servos, LCD, and solar tracking.
- The HX711 load cell needs stable timing, so isolating it prevents interference from servos and ultrasonic sensors.
- The separation keeps the wiring simpler and makes testing and debugging easier.

## Key Functions

- Bottle size detection
- Weight validation
- Accept/Reject control
- Servo-driven top and bottom lids
- Solar tracking with LDR sensors
- Relay timers for charging time
- LCD and LED status indicators
- Bin-full detection
- Serial communication between two Arduinos

## Components

- Arduino Uno ×2
- HX711 load cell module
- Ultrasonic sensors
- Servo motors (top, bottom, solar)
- LDR sensors
- Solar panel
- TM1637 displays
- Relay modules
- 16×2 I2C LCD

## Software Libraries

- SoftwareSerial
- HX711
- TM1637Display
- LiquidCrystal_I2C
- Servo

## Project Members

- [Jovilyn Areglado](https://web.facebook.com/ziemustrive)
- [Edzel Catuiran](https://web.facebook.com/edzel.catuiran.21)
- [Jay El Lasam](https://web.facebook.com/jl.lasam.1)
- [Domingo Bautista VII](https://web.facebook.com/itsdomd7th)
- [Brithney Oratel](https://web.facebook.com/joy.oratel)
- [Dhanrev Mina](https://web.facebook.com/CHOCODHANDHAN/)
