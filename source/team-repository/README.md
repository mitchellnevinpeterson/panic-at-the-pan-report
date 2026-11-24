# Panic at the Pan

ME218a · Team 12. A PIC32 cooking arcade: select ingredients, control heat with a hovering hand, and stir with a joystick.

**Status:** firmware scaffold builds; gameplay and hardware drivers are unfinished. KiCad includes an overview and a detailed Power + PIC32 draft.

## Start here

- [Hardware](hardware/README.md) — schematic plan and next steps.
- [Purchase list](purchase_list.md) — parts, availability and budget.
- [Firmware](firmware/README.md) — structure and build command.
- [Game brief](panic_at_the_pan_story.md) / [state machines](panic_at_the_pan_fsm.md) — intended behavior.
- [Course specification](ME218a_F26_Project_Spec_V2.pdf) — authoritative requirements.
- [Original slides](ME218A%20Design%20Review.pptx) — earlier concept.

Current mechanisms: joystick-controlled stirring direction, rising flame, and one sealed Gushers pouch per completed game. Reuse existing MAX7219 displays.

**Next:** add supply/board datasheets, then review the Power + PIC32 circuit before wiring.
