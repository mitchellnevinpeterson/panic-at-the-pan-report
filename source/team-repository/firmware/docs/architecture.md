# Firmware architecture

## Scope and authority

The course PDF sets requirements. `panic_at_the_pan_fsm.md` defines behavior;
`panic_at_the_pan_story.md` supplies the build brief and named timing constants.
The user's latest decisions supersede older mechanism assumptions: the joystick
selects direction for circular spoon motion, the flame rises, sealed Gushers
pouches dispense, and existing monochrome MAX7219 panels are reused.

This change establishes module boundaries and a target build only. State enums
list the specified states, including leaf states of Game/Cooking and Pan/Playing,
but no gameplay transitions have been implemented. Do not interpret an enum's
presence as completed behavior.

## Service ownership

| Service | Owns | Intended inputs |
|---|---|---|
| GameSM | Game/idle clocks, result timing, Finish LED, elapsed panel, dispenser | Ingredient added, Finish, activity, its timer expirations |
| IngredientSM | Once-per-game ingredient mask | Debounced switch flips, end/reset |
| HeatSM | Heat/target, burn state, sizzle, heat indication, fire lift | Ingredient added, distance samples, end/reset |
| StirSM | Current ingredient's stir duration, spoon direction | Ingredient added, stir start/stop, ticks, end/reset |
| ScoreSM | Sole score accumulator and score display | Ingredients, heat points, burns, stir results, reset |
| PanDisplaySM | Pan icons and animations | Ingredients, stirring, end/reset, icon/animation timers |

Service code must not include `xc.h` or manipulate registers. Device drivers own
hardware access; configuration owns physical pin assignments. A driver can serve
several logical outputs, but each logical output must retain one service owner.

The matrix transport will be shared by multiple logical displays. Before wiring
the copied global driver into any service, choose separate chip-select chains or
one chain with an explicit frame layout and one serialized transfer owner. Two
services must not edit the same global frame or overlap transfers.

## Framework integration

The six services are registered in `config/ES_Configure.h`. The initial priority
order, low to high, is Pan Display, Score, Stir, Heat, Ingredient, Game. Each has
a provisional 16-event queue. Validate priorities/queue depths with measured event
bursts during integration; these are engineering choices, not course mandates.

Nine framework timers are reserved: four Game, two Heat, one Stir, two Pan.
The course framework delivers `ES_TIMEOUT` with the timer ID in `EventParam`.
The FSM's names such as `BURN_TIMEOUT` describe that routed timeout; they do not
need redundant event IDs. No timers are started by the scaffold services.

`CheckGameInputs()` intentionally returns false. Later split polling by device,
debounce without delay loops, and post explicit events. Keyboard mode should
inject the same events once service handlers exist. Preserve the course scheduler
and timer implementation; do not introduce a second scheduler or an RTOS.

## Event payload contract to finalize

The inherited `ES_Event_t.EventParam` is **16 bits**. It cannot carry an arbitrary
pointer on this PIC32 and cannot hold both an ingredient index and a full 16-bit
millisecond duration without an encoding decision.

| Event | Intended payload / integration work |
|---|---|
| SWITCH_FLIPPED, INGREDIENT_ADDED | Ingredient index 0..4; validate bounds |
| FINISH_PRESSED, STIR_START, STIR_STOP, USER_ACTIVITY, GAME_RESET | No payload |
| HEAT_POINTS | Fixed-point closeness representation TBD; never cast a float directly into EventParam |
| BURNED | One penalty per burn; no payload needed in current FSM |
| STIR_RESULT | Ingredient plus duration: choose documented encoding or a bounded result queue; no stack pointers |
| GAME_END | Completed/timeout reason encoding TBD |
| ES_TIMEOUT | Allocated timer ID |

Check every event post result. A failed fan-out must be handled explicitly;
otherwise some services can accept an ingredient while others miss it. Review
end-of-game ordering, queued samples, and the last stir result. Score deliberately
remains active after `GAME_END` until reset so final stir points can arrive.

## Tooling choices

The build uses an explicit source list and project-relative include paths instead
of copying generated IDE directories. This keeps files reusable in an MPLAB
project without hidden references to the original lab folder. Plain Make is
available on this laptop; CMake is not required for this small C-only build.

Official references consulted on 2026-11-07:

- [Microchip XC32 DFP option](https://onlinedocs.microchip.com/oxy/GUID-91118B1F-52AE-4822-A75E-B65FA4C48896-en-US-2/GUID-508506FB-BEFB-4101-BA76-26A921891125.html): selecting external device support with `-mdfp`.
- [Microchip builds outside MPLAB X](https://developerhelp.microchip.com/xwiki/bin/view/software-tools/ides/x/outside/make/): compiler/build integration context. The standalone Makefile is project-authored.
- [CMake target include directories](https://cmake.org/cmake/help/latest/command/target_include_directories.html): target-scoped dependency guidance reviewed while choosing a minimal build system.

The architecture follows the supplied course framework and project ownership
rules. No external repository or generated vendor framework has replaced them.
