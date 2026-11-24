# Lab code provenance

All copied sources are from the local course materials supplied by the team.
Original comments and attribution are retained; no new license is asserted over
the course files. Do not publish them under a blanket project license without
checking the course authors' terms.

| Destination | Local source | Treatment |
|---|---|---|
| `vendor/es_framework/FrameworkHeaders/` | `../Lab3/Lab3_Part4/FrameworkHeaders/` | Every `.h` except app-specific `ES_Configure.h` copied unchanged |
| `vendor/es_framework/FrameworkSource/` | `../Lab3/Lab3_Part4/FrameworkSource/` | All `.c` copied unchanged |
| `drivers/reused/PIC32_SPI_HAL.c/.h` | Lab 3 Part 4 ProjectSource/ProjectHeaders | Latest local revision of the Lab 2 SPI work, unchanged |
| `drivers/reused/DM_Display.c/.h` | Lab 3 Part 4 ProjectSource/ProjectHeaders | Existing display HAL, unchanged |
| `drivers/reused/FontStuff.c/.h` | Lab 3 Part 4 ProjectSource/ProjectHeaders | Existing font helpers, unchanged |

Lab 1 contains schematic/report material, not a standalone C module needed here.
The latest Lab 3 versions avoid copying competing implementations of the same
Lab 2 functions. Lab main programs, test services, IDE caches, compiled objects,
and archives are intentionally excluded.

`reuse_manifest.json` records project-root-relative source/destination paths and
SHA-256 for each copied file. `python3 firmware/tools/check_reuse.py` verifies the
copies without requiring access to the sibling lab folders.

The only project-authored file under the framework tree is
`FrameworkHeaders/ES_Configure.h`, a forwarding include to
`firmware/config/ES_Configure.h`. Some upstream source files use relative includes;
the shim preserves them while putting editable registration in one place.

## Platform assumptions inherited unchanged

- 40 MHz SYSCLK, 20 MHz peripheral clock, ES tick on the MIPS core timer.
- Configuration words select internal FRCPLL and PGEC1/PGED1 debugging.
- UART1 currently maps RX to RB6 and TX to RB7, 115200 baud. The alternative
  RB2/RB3 branch in `terminal.c` is disabled (`USE_RB2_3` is not defined).
- `LED_DEBUG` is commented out in `ES_Port.c`; enabling it reserves RB15.
- Lab SPI setup used SCK1 RB14, SDO1 RA1, and SS1 RA0. That setup is **not called**
  by the scaffold. [Pin map V2](../../hardware/docs/pin_map.md) retains these display
  pins with manual SS and adds independent SPI2 for MCP23S17; initialization is pending.
- Lab SPI1 SS-edge detection uses external interrupt flag INT4. Any active reuse
  must reserve that resource or replace the completion mechanism.

Compilation is evidence of toolchain compatibility, not proof that every copied
API was exercised in the labs. See `../README.md` for the blocking display call,
unimplemented SPI entry points, and font warning discovered during inspection.
