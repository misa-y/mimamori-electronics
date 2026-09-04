# Mimamori electronics prototype

This folder contains a drop-in Arduino sketch for the Wokwi circuit shown in the supplied screenshot. A clearer screenshot was used to trace this pin map:

| Component | ESP32 pin |
|---|---:|
| DHT22 data | GPIO 18 |
| DS18B20 data | GPIO 4 |
| Heart-rate analog output | GPIO 34 |
| MPU6050 SDA / SCL | GPIO 21 / GPIO 22 |
| LED (vibration substitute) | GPIO 26 |
| Buzzer | GPIO 12 |
| Response button | GPIO 27, wired to GND and using `INPUT_PULLUP` |

The DS18B20 data line requires a 4.7 kΩ pull-up to 3.3 V. The LED requires a series resistor (typically 220–330 Ω). A real vibration motor, speaker, or audio module must not be driven directly from an ESP32 GPIO; use an appropriate transistor/MOSFET driver and power design.

## Risk model

The demonstration score combines environmental heat index, skin temperature, heart rate, movement, and duration of heat exposure:

- LOW: 0–3 points
- MODERATE: 4–6 points
- HIGH: 7 or more points
- Hard HIGH override: heat index at least 54 °C or skin temperature at least 37.5 °C

The thresholds are transparent prototype heuristics for comparing scenarios—not a medical device algorithm. They require clinical input, calibration against the final sensors and enclosure, human-factors testing, and validation before use with people.

## Voice and caregiver behavior

Wokwi's buzzer can demonstrate alert timing but cannot reproduce stored speech. The sketch therefore prints the exact voice prompt to the Serial Monitor while playing distinct moderate/high tones. In hardware, replace `tonePattern()` with a driver for an I2S amplifier/audio module or synthesized speech subsystem.

The caregiver notification is represented by a Serial Monitor event after 15 seconds of unacknowledged HIGH risk. A production version would replace it with BLE/Wi-Fi/LTE messaging and should also define delivery confirmation, retry behavior, offline handling, privacy, and escalation rules.

## Suggested component test order

1. Run `LIVE`; confirm DHT22 temperature and humidity change with its sliders.
2. Change the DS18B20 temperature and confirm the skin reading.
3. Change the custom heart-rate chip output and confirm BPM changes.
4. Change MPU6050 acceleration and confirm `Movement` changes.
5. Confirm MODERATE activates the LED and one short tone.
6. Confirm HIGH flashes the LED and repeats the higher tone.
7. Confirm the button records a response and suppresses escalation.
8. Run all cases in `TEST-SCENARIOS.md` and compare them with the Unity cases.
