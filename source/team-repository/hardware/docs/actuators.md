# Actuators — proposed

[Schematic](../sheets/actuators.kicad_sch) · [Preview](../review/actuators/schematic.svg)

| Part | Draft choice | Still needed |
|---|---|---|
| Spatula | Bipolar stepper + DRV8825 carrier + home switch | Motor torque, size, current and mounting |
| Treat release | 5 V positional servo + pouch gate | Servo model, travel, stall current and gate test |

The spatula homes at startup, then tracks angle by counting steps. Centering the joystick returns to that reference. Missed steps require homing again; this is not continuous position feedback. A normal continuous-rotation servo cannot provide this behavior alone.

- **12 V:** U20 candidate Pololu 2133; C40 = 100 µF electrolytic nearby. Set winding-current limit before use. Full-step mode shown; no motor hot-plugging. [Carrier documentation](https://www.pololu.com/product/2133)
- **Control:** `SPATULA_STEP`, `SPATULA_DIR`, `SPATULA_WAKE`, `SPATULA_HOME`. WAKE defaults low. After waking, wait ≥1.7 ms; STEP high/low ≥1.9 µs. [DRV8825 timing](https://www.ti.com/lit/ds/symlink/drv8825.pdf)
- **Treat:** `TREAT_PWM` passes through a proposed 5 V [SN74AHCT1G125](https://www.ti.com/product/SN74AHCT1G125). This small surface-mount part needs an adapter. Servo current comes directly from load distribution. C42 is a provisional bulk value.

**Pins reserved:** STEP RA4, DIR RB4, WAKE RB9, HOME RB10; treat PWM RB5/OC2. [Pin map V2](pin_map.md) fits with the MCP23S17. A change from STEP/DIR to another driver requires another pin review.

Motor/servo choices are proposals. Confirm current, protection, cable ratings and shared supply capacity. Assembly ports are logical labels, not header pin numbers. No flame actuator.

## Candidate — TB6612FNG

User identified this part; not selected or confirmed acquired. This dual H-bridge can drive two brushed DC motors or one compatible bipolar stepper. For a DC spatula motor, angle following would also require position feedback (e.g. encoder) and closed-loop control; PWM/direction alone do not set angle. For a stepper, it requires coil sequencing and lacks the DRV8825's STEP/DIR interface and adjustable current regulation, so it is not a drop-in replacement. Keep the current DRV8825 proposal pending motor selection; check motor stall/winding current, supply and carrier thermal limits before adopting TB6612FNG. [Pololu carrier documentation](https://www.pololu.com/product/713) · [Toshiba datasheet](https://toshiba.semicon-storage.com/info/TB6612FNG_datasheet_en_20141001.pdf?did=10660&prodName=TB6612FNG).
