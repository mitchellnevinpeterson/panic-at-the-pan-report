# Firmware

**Buildable scaffold only.** Six services are registered; gameplay, keyboard simulation, sensors and actuators are not implemented. Placeholder drivers report `DRIVER_NOT_CONFIGURED`.

## Build

From the repository root:

```sh
make -C firmware check
```

Requires Make, Python 3, XC32 and the PIC32MX Device Family Pack. Produces ELF/HEX files in `build/` and verifies copied-source hashes; does not flash hardware. [Toolchain setup and overrides](docs/build.md).

## Structure

| Folder | Purpose |
|---|---|
| `app/` | Startup and event checkers |
| `config/` | Services, timers, constants and hardware pin-map reference |
| `services/` | Game, Ingredient, Heat, Stir, Score and Pan Display |
| `drivers/` | Device interfaces, stubs and preserved lab drivers |
| `vendor/es_framework/` | Course Events and Services framework |
| `docs/` | Architecture, decisions and provenance |

## Before implementation

- Implement [pin map V2](../hardware/docs/pin_map.md) and calibration; UART/core-timer setup is inherited from the labs.
- Adapt the display driver's blocking SPI wait and fixed 32×8 buffer.
- Review incomplete SPI APIs and the font bounds warning. Reused drivers are compiled but not called.
- Unsupported service events currently halt the framework with `FailedRun`.

[Architecture](docs/architecture.md) · [Pending decisions](docs/pending_decisions.md) · [Source provenance](docs/reuse.md) · [Build validation](docs/validation.md)
