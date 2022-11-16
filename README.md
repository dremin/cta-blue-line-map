# CTA Blue Line Map

This is a project to display live train status of the CTA Blue Line on a strip map, using an ESP32, RGB LEDs, and PCA9685 PWM drivers.

## Pre-requisites

* Visual Studio Code
  * Platform IO extension installed
* ESP32 development board
* 7x PCA9685 PWM driver boards
* 33x RGB LEDs, connected in RGB order, starting with the Forest Park LED.
* [CTA Train Tracker API key](https://www.transitchicago.com/developers/traintrackerapply/)

## Instructions

1. Clone this repository and change to its directory:

```
git clone https://github.com/dremin/cta-blue-line-map.git
cd cta-blue-line-map
```

2. Create the secrets file:

```
cd include
cp secrets.h.example secrets.h
```

3. Open `secrets.h` and set your Wi-Fi SSID, password, and CTA Train Tracker API key.

4. Open the directory in Visual Studio Code with the Platform IO plugin, then upload!

## LED Colors

Each station's LED will display a different color, depending on the train(s) at the station:

- **No train**: Off
- **O'Hare-bound train**: Blue
- **Forest Park-bound train**: Red
- **Trains in both directions**: Purple
- **Jefferson Park-bound train**: Blue-Green
- **UIC-Halsted-bound train**: Red-Green
- **5000-series train (from 54/Cermak yard)**: Green

## To-Do

- Get more PWM drivers, uncomment code for them and test.