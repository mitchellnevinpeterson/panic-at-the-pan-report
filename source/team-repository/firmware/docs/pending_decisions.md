# Decisions before active firmware

## Hardware

- Verify actual course 5 V / 3 A supply, actuator loads, and power distribution.
- Implement [pin map V2](../../hardware/docs/pin_map.md); UART/programming pins remain reserved.
- SPI1 serves displays; SPI2 serves MCP23S17. Pan layout is centered 3–4–3
  (10 MAX7219 modules); confirm auxiliary counts, orientation and display frame
  ownership. The copied lab driver supports one 32x8 chain only.
- Select ultrasonic, joystick, switches, audio amplifier/speaker, and actuators.
- Implement/calibrate joystick angle following and center-to-home behavior for
  the proposed STEP/DIR spatula driver; confirm motor and mechanism.
- Determine pouch gate travel and servo pulse range. The flame actuator was removed;
  heat uses an LED bar.
- Record switch active levels and whether both toggle edges or press-only edges
  count for the chosen physical switches.

## Gameplay and calibration

- Dish, recipe mask, ingredient identities, pictorial instructions.
- Positive heat targets and optimal stir durations; all point weights and negative
  score policy. `GameTuning.configured` remains false.
- Hand-distance mapping and out-of-range behavior, joystick deadband/debounce,
  stir detection, and animation frame period.
- Burn hysteresis: the brief lists 1.4x, while the FSM only suggests it.
- New ingredient while overheating: the FSM retains the current state/timer;
  confirm whether carrying the previous ingredient's burn time is intended.
- Activity while a hand remains present or the joystick remains held: decide the
  repeated activity cadence so an engaged player does not time out accidentally.
- First ingredient while the joystick is already displaced: initial state sampling
  must not lose a stir-start edge that occurred before the game.

## Integration

- Define bounded event payloads, especially `STIR_RESULT` and fixed-point heat.
- Implement FSM transitions with timer cancellation, pending-event handling, and
  event fan-out failure policy. Preserve final stir scoring after `GAME_END`.
- Replace the blocking lab display transfer in the active driver with progress
  driven by hardware completion/events. Review inherited SPI stubs/font bounds.
- Add keyboard test mode and transition tracing, then bring up one hardware
  device at a time. The scaffold build is not checkpoint-2 functionality.
- Reconcile the original story/FSM text with approved mechanism changes before
  implementing outputs. Original project documents were left intact.
