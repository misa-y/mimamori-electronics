# Mimamori Wokwi test scenarios

Enter each command in the Serial Monitor. The format is:

`SIM airC,humidity,heartBpm,skinC,movementG,exposureMinutes`

| Scenario | Serial command | Expected result |
|---|---|---|
| Cool, resting | `SIM 24,45,70,32.5,0.05,0` | LOW, score 0 |
| Warm but stable | `SIM 30,55,82,33.4,0.08,5` | LOW, environmental contribution shown |
| Moderate heat strain | `SIM 33,65,102,35.1,0.12,12` | MODERATE, LED and short buzzer warning |
| Exertion in heat | `SIM 35,60,112,35.4,0.35,18` | HIGH, repeating alert pattern |
| Prolonged severe exposure | `SIM 38,70,125,36.2,0.25,35` | HIGH; caregiver alert after 15 seconds without response |
| Hot and motionless | `SIM 36,68,108,35.8,0.01,25` | HIGH; low movement listed as a contributing factor |
| Skin-temperature override | `SIM 25,45,75,37.6,0.05,0` | HIGH due to hard safety override |

For the acknowledgement test, run any HIGH scenario and press the physical Wokwi button before 15 seconds. The Serial Monitor should print `RESPONSE RECEIVED`. Repeat without pressing it; after 15 seconds it should print `CAREGIVER ALERT`.

Use `LIVE` to return control to the Wokwi component sliders.

These values are prototype demonstration inputs. They are not clinical validation cases.
