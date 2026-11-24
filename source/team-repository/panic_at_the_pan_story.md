# Panic at the Pan — Game Story and Build Brief

ME 218a Fall 2026 · Karl’s Kitchen · Team 12

**Plan update — November 8, 2026:** Heat and Stir changes are confirmed: remove the flame servo, show heat on an LED bar, and make the spatula follow joystick angle and return home at center. Display revision (November 9): the pan carries 3–4–3 centered rows of 8×8 modules (10 total). Flip detection, flip animation and flip-related activity/scoring changes are **optional**.

## 0. How to use this document

This brief describes **what the game is and how it should feel**, so a coding agent can set up the PIC32 firmware project. Read it together with two other files:

1. **ME218a_F26_Project_Spec_V2.pdf**: the course requirements. It wins on anything that is a *requirement*.
2. **panic_at_the_pan_fsm.md**: Draft 2 includes the confirmed Heat and Stir changes. Its optional section records proposed flip detection, animation and Game/Score handling; these are outside the confirmed baseline.

The purchase list, schematics and firmware also still reflect the earlier hardware plan. Update them after the open decisions below are settled.

Anything marked **TBD** has not been decided by the team. Do not invent values for TBD items. Leave a clearly named constant or a `// TODO(team):` comment instead.

---

## 1. The premise

A fire has knocked out the lab’s appliances, and the SPDL has become Karl’s Kitchen, an emergency mess hall running on low-voltage power. Our CHEF (Culinary Hardware Entertainment Fixture) is **Panic at the Pan**: a countertop cooking station where a player cooks a dish in about 60 seconds by juggling three things at once. They add the right ingredients, keep the heat right with their hand, and stir each ingredient for the right amount of time.

The fun comes from **doing everything in parallel**, like real cooking. While one hand controls the heat, the other flips ingredient switches and stirs. Nothing waits for anything else.

---

## 2. A game, start to finish

The machine sits in **Welcome mode**. The pan’s LED array pulses a dish icon, inviting someone to cook. The score panel reads 0 and the timer panel reads 0:00. The Finish button is dark.

A guest walks up and flips one of the five ingredient switches, say chicken. That first flip **starts the game** and also counts as the first ingredient. The pan array shows a big chicken drumstick for a moment, then settles into a view of what’s in the pan. The timer panel starts counting.

Chicken needs a certain amount of heat. The guest holds a hand over the **hover zone** above the ultrasonic sensor: the closer the hand, the hotter the pan. The **sizzle sound** and **LED heat bar** track the hand-controlled heat; more of the bar lights as heat rises, and the score panel ticks upward the closer they hold the heat to chicken’s target.

With the other hand they move a **joystick** to stir. A small physical **spatula rotates around the pan like a clock hand**, driven by one motor, and the pan array shows a swirl. Pointing the joystick up, right, down or left places the spatula at the corresponding angle. Circling the joystick makes the spatula follow around the pan. Releasing the joystick to center returns the spatula to a fixed **home angle**. Each ingredient has an ideal stir time, and the machine is quietly counting.

Before finishing with the chicken, they flip the onion switch. Onion has a **different heat target**, so the guest now has to move their hand to a new height to keep it cooking well. Chicken’s stir time is locked in and scored. Stirring now counts toward the onion. A player who adds an ingredient and never stirs it scores a stir time of zero for it, and that’s their choice.

The guest gets distracted and holds their hand too close. The heat goes far past the onion’s target. After three seconds of that, the onion **burns** and a burnt sound plays. There is **no moving flame or flame servo**. A visible burn cue is still **TBD**; the heat bar continues to communicate heat level. A burnt ingredient stays burnt. Lowering the heat doesn’t save it. The pan earns no heat points until the guest adds the next ingredient, which clears the burn warning and gives a fresh target.

**Optional pan flip:** the player lifts the pan about a horizontal bar passing through its handle. If included, a flip registers either when the pan passes a chosen angle or when its handle presses a switch during the lift. The LED panels move with the pan; a flip animation is proposed. A score bonus is **not yet part of scoring**. The sensing method and return requirement remain **TBD**; a full inversion is not specified.

At 30 seconds, the **Finish button lights up**, quietly telling the player they may stop whenever they like. They keep cooking until 50 seconds, then press it. (If they hadn’t, the game would have ended on its own at 60 seconds.)

The pan array shows **GAME OVER**, the score panel holds the final score, and the machine **dispenses a TREAT**. After a short pause, everything resets to Welcome mode for the next guest.

If a guest wanders off mid-game and nothing is touched for **20 seconds**, the machine shows a pictorial timeout (for example, a sleeping chef), gives **no** TREAT, and returns to Welcome mode.

---

## 3. Game rules

### 3.1 Timing

- The game starts on the first ingredient switch flip.
- The game ends automatically at **60 s** of elapsed time.
- The **Finish button** is ignored for the first **30 s**. After that it lights up and ends the game when pressed. This guarantees the spec’s rule that a game never takes less than 30 s.
- The timer panel shows elapsed time for the whole game.

### 3.2 Ingredients

- There are **five** ingredient switches, one per ingredient.
- Ingredients can be added in **any order**.
- Each ingredient can be added **once per game**. Repeat flips are ignored.
- A dish has a set of **right** ingredients. Adding a right one earns points and adding a wrong one loses points. **TBD:** the dish (or dishes), which ingredients are right, and how the player learns the recipe without written instructions.
- Switches are read on **edges** (any change counts as a flip), so the next player doesn’t need to reset them. **TBD:** toggle switches vs. spring-return levers.

### 3.3 Heat

- Heat is a **continuous** value computed from hand distance above the ultrasonic sensor. Closer means hotter. Out of range or no hand means minimum heat.
- The heat **target** is set by the **most recently added** ingredient. Each ingredient has its own target.
- Heat is sampled every 100 ms. Each sample earns points in proportion to how close heat is to the target. This continuous scoring is how we meet the spec’s “analog input, no simple thresholds” rule.
- The sizzle sound follows the heat value; the LED array bar shows increasing heat with increasing illuminated length. Segment count, color and target marker are **TBD**. Heat computation and scoring stay continuous even though the bar has discrete LEDs.

### 3.4 Burning

- **Burn line** = 1.5 × current target.
- Heat above the burn line starts a **3 s** burn timer. Dropping back below cancels it.
- If heat stays above the burn line for 3 s, the current ingredient **burns**:
  - A visible burn warning is shown (**TBD:** pattern and display; preserve readable heat feedback).
  - A burnt sound plays.
  - A one-time burn penalty is applied.
  - Heat points stop.
- A burnt ingredient **stays burnt**. The only way out is adding the next ingredient. That clears the burn warning, sets the new target, and resumes normal cooking.
- If the last ingredient burns, the burn warning remains until the game ends.
- No flame positioning servo or pop-out mechanism is used.

### 3.5 Stirring

- One motor rotates a physical spatula around the pan like a clock hand. The spatula follows joystick input during cooking.
- Control is a **two-axis joystick position follower**, superseding the earlier direction-only proposal. Joystick direction commands the corresponding spatula angle around the full circle; circling the joystick moves the spatula around the pan.
- Inside a center deadband, command a fixed home angle. Home angle, deadband, startup referencing, travel speed and behavior across the 0°/360° boundary are **TBD**.
- Motor type, position feedback, speed and torque remain **TBD**. Do not finalize the earlier continuous-rotation servo candidate before resolving position control.
- Stir-time detection must be revised for position following: holding a deflected joystick can hold the spatula stationary. **TBD:** count commanded angular movement or measured motor movement, and whether automatic return-home motion counts. Joystick input alone does not verify actual motor motion.
- Stop the spatula at game end or timeout. Define whether stirring pauses while the pan is raised.
- Stir time accumulates for the **most recently added ingredient** only.
- When a new ingredient is added (or the game ends), the previous ingredient’s total stir time is scored against its **optimal stir time**. Closer to optimal earns more points.
- The pan array shows a swirl while the player is stirring.

### Pan lift / flip — optional

- The player moves the pan by hand; no pan-flip motor is specified.
- A horizontal bar through the handle forms the pivot. The pan lifts away from the counter as it rotates about this bar.
- **Option A:** sense the pan passing a chosen tilt angle. Sensor type and angle are **TBD**.
- **Option B:** place a switch so the handle presses it at a chosen point in the lift. Switch placement establishes the trigger position; it need not measure angle continuously.
- An underside rest sensor remains an option if a separate return-to-counter signal is needed. Define whether a return is required before another flip can count.
- Optional flip animation: a detected flip triggers an animation on the pan display. The animation and its priority relative to ingredient icons and stirring are **TBD**.
- A flip bonus is optional and undecided; no flip points are included in the current scoring formulas.
- **TBD:** repeat detection, idle activity and behavior of heat/stirring while lifted. Define these before adding flip events to the FSM.

### 3.6 Scoring

The score panel shows a **live** score throughout the game, not just at the end. Score changes come from four sources:

| Source | When | Effect |
|---|---|---|
| Ingredient added | Each new ingredient | + `P_RIGHT` if in recipe, − `P_WRONG` if not |
| Heat | Every 100 ms sample while not burning | + `K_HEAT × closeness`, where closeness = max(0, 1 − \|heat − target\| / target) |
| Burn | Once, when an ingredient burns | − `P_BURN` |
| Stir | When an ingredient’s stir window closes | + `K_STIR × max(0, 1 − \|t − optimal\| / optimal)` |

These formulas are starting points. **TBD:** final point values, and whether the displayed score may go negative.

### 3.7 End of game and TREAT

- On game end (60 s or Finish): show GAME OVER on the pan array, hold the final score, dispense one TREAT, wait the result time (≤ 30 s per the spec), then return to Welcome mode.
- TREAT: one sealed Gushers pouch per completed game (requires teaching-team approval).

### 3.8 Idle timeout

- **20 s** with no user activity during a game → pictorial timeout on the pan array, no TREAT, then return to Welcome mode.
- Activity = any switch flip, Finish press, joystick stir, or a hand entering ultrasonic range.

---

## 4. The physical machine

| Element | Type | Role | Hardware status |
|---|---|---|---|
| 5 ingredient switches | Digital input | Add ingredients | Switch type TBD |
| Finish button with built-in LED | Digital input + LED output | End game after 30 s | TBD |
| Ultrasonic sensor (hover zone) | Non-contact, analog distance | Heat control | Model TBD (e.g. HC-SR04-style: trigger pulse + echo timing via input capture) |
| Joystick | Two analog inputs (ADC X/Y) | Direction commands spatula angle; center commands home | Home angle / deadband TBD |
| Spatula motor and rotating arm | Actuator | Rotate spatula around the pan like a clock hand | One motor; type, feedback and mounting TBD |
| Pivoting pan and horizontal handle bar | Player-operated mechanism | Allow pan lift / tilt | Lightweight laser-cut pan sized around a 3–4–3-module display; physical dimensions TBD |
| Pan-flip sensor (optional) | Input | Trigger optional flip animation | Angle threshold or handle-operated switch; return sensing TBD |
| TREAT dispenser | Actuator | Reward at game end | Mechanism TBD |
| Score LED panel (top left) | Display | Live score | Driver TBD (e.g. MAX7219 on SPI) |
| Timer LED panel (top right) | Display | Elapsed time | Driver TBD |
| Pan LED array | Display | Ingredient icons, swirl, flip animation, GAME OVER, timeout, attract | 3–4–3 centered rows (10 total), moving with pan; 24/32/24 pixels wide and 24 pixels high |
| LED array heat bar | Display | Heat level via illuminated length | Reuse existing panels if suitable; layout / colors TBD |
| Sound module | Audio | Sizzle (proportional), burnt sound | TBD |

**Pin assignments are TBD** and come from the team’s KiCad schematic. The target chip is the 28-pin **PIC32MX170F256B**, so pins are tight. Displays should share one SPI bus if their drivers allow daisy-chaining.

**Display size means modules, not inches:** the centered 3–4–3 arrangement uses 10 modules (640 LEDs) within a 32×24-pixel bounding canvas. Top/bottom rows have a 4-pixel inset on both sides. Three-module rows need individual modules or appropriately separable assemblies; do not assume four-module boards can be cut. Confirm the available strip count, actual board dimensions and mechanical fit before buying or cutting. Final pan dimensions, material, thickness and loaded mass remain TBD; the lightweight intent does not yet establish the weight with displays installed.

The LED panels tilt with the pan; whether the spatula motor also tilts is still **TBD**. Allow cable movement at the pivot. Recheck the pin budget for both joystick axes, the flip sensor and any motor position feedback. Review display chain length and power for the 10-module array.

---

## 5. How the design meets the spec

| Spec requirement | How Panic at the Pan meets it |
|---|---|
| Welcoming mode that invites play | Pulsing dish icon on the pan array |
| ~60 s game, never < 30 s | Auto-end at 60 s; Finish button disabled before 30 s |
| Creative display of elapsed time | Timer panel (**TBD:** make it more creative than plain digits) |
| 20 s idle → timeout, then reset | Idle timer in Game SM |
| ≥ 3 distinct inputs | Ingredient switches, ultrasonic hover, joystick, Finish button |
| ≥ 3 feedback modes | LED panels/heat bar, sizzle sound, motorized spatula, TREAT |
| Analog input used non-trivially | Ultrasonic distance → continuous heat → proportional sound and points |
| Non-contact sensing | Ultrasonic hover zone |
| Electromechanical actuation beyond TREAT | PIC32-controlled spatula motor |
| Visibly moving parts under PIC32 control | Rotating spatula and TREAT dispenser; pan tilt is player-operated |
| Completion indication ≤ 30 s, shows degree of completion | GAME OVER + final score + TREAT, then reset |
| Usable without words | Pictorial cues only; lit Finish button; icons on the pan array |
| Only the PIC32MX170F256B controls the CHEF | One PIC32 runs everything (see §6) |

---

## 6. Software architecture

### 6.1 Platform

- **MCU:** PIC32MX170F256B, programmed in **C**.
- **Framework:** the ME 218 **Events and Services (ES) framework** as distributed by the course. Use the team’s copy of the framework. Do not re-implement it or substitute an RTOS.
- **One PIC32** runs all services. (Using more than one has not been approved.)

### 6.2 Rules the code must follow

1. **No blocking code.** No delay loops, no `while` loops waiting on hardware. All timing uses framework timers that post timeout events.
2. **Each output device has exactly one owner service.** Other services request changes by posting events.
3. **The score has one owner** (Score service). Nobody else modifies it.
4. **Numbers are variables, not states.** Heat, target, elapsed time, stir time and score are module-level variables. States represent changes in behavior only.
5. Reconcile **panic_at_the_pan_fsm.md** with this story before implementing revised hardware behavior. Do not implement the removed flame mechanism or invent undecided pan-flip rules.

### 6.3 Services

| Service | States | Owns |
|---|---|---|
| **Game** | Welcome, Cooking (Early, Finishable), GameOver, TimedOut | Game clock, idle timer, Finish LED, timer panel, TREAT dispenser |
| **Ingredient** | Ready, Locked | Record of which ingredients were added |
| **Heat** | PanEmpty, Cooking, Overheating, Burning | Heat value and target, sizzle/burnt sound, LED heat bar and its burn warning (pattern TBD) |
| **Stir** | Idle, NotStirring, Stirring | Stir time per ingredient, spatula motor |
| **Score** | Idle, Active | Score, score panel |
| **Pan Display** | Attract, Playing (NewIngredient, PanView, Swirl), GameOverText, TimeoutIcon; optional flip animation integration TBD | Pan LED array; flip animation only if included |

### 6.4 Event sources (event checkers and interrupts)

| Source | Posts | To |
|---|---|---|
| Switch checker (debounced, edge-detected) | `SWITCH_FLIPPED(n)`, `USER_ACTIVITY` | Ingredient, Game |
| Finish button checker (debounced) | `FINISH_PRESSED`, `USER_ACTIVITY` | Game |
| Joystick checker (ADC, deadband + debounce) | `STIR_START`, `STIR_STOP`, `USER_ACTIVITY` | Stir, Pan Display, Game |
| Ultrasonic echo (input capture ISR) | Stores latest distance (no event); `USER_ACTIVITY` when a hand enters range | Heat reads it on each `HEAT_SAMPLE`; Game |

**TBD:** exact stir-detection rule for angle following. The earlier deflection-only proposal is insufficient because a held joystick can command a stationary angle. Pan-flip detection events, rearming and animation priority are not yet defined; Pan Display owns the animation output.

### 6.5 Timers

| Timer | Owner | Duration | Purpose |
|---|---|---|---|
| `GAME_TICK` | Game | 1 s, repeating | Elapsed time, 30 s and 60 s checks |
| `IDLE_TIMER` | Game | 20 s, restarted on activity | Idle timeout |
| `RESULT_TIMER` | Game | 10 s (≤ 30 s) | Hold GAME OVER and final score |
| `TIMEOUT_DISPLAY` | Game | 5 s | Hold timeout icon |
| `HEAT_SAMPLE` | Heat | 100 ms, repeating | Sample heat, update outputs, post points |
| `BURN_TIMER` | Heat | 3 s | Burn after sustained overheat |
| `STIR_TICK` | Stir | 50 ms, repeating | Spatula motor update, stir-time counting |
| `ICON_TIMER` | Pan Display | 1.5 s | How long a new-ingredient icon shows |
| `ANIM_TIMER` | Pan Display | TBD | Attract and swirl animation frames |

### 6.6 Suggested file layout

One `.c`/`.h` pair per service (`GameSM`, `IngredientSM`, `HeatSM`, `StirSM`, `ScoreSM`, `PanDisplaySM`), plus hardware driver modules (`Ultrasonic`, `Joystick`, a spatula motor driver once selected, `LedPanels`, `Sound`, `Dispenser`), `EventCheckers.c`, a `GameConfig.h` holding every tunable constant from §7, and the framework’s configuration header listing services, events, timers and event checkers.

---

## 7. Tunable parameters

Put all of these in one header (`GameConfig.h`) so the team can tune without hunting through code.

| Constant | Default | Notes |
|---|---|---|
| `GAME_LENGTH_S` | 60 | |
| `FINISH_ENABLE_S` | 30 | Must stay ≥ 30 (spec) |
| `IDLE_TIMEOUT_MS` | 20000 | Spec value |
| `RESULT_DISPLAY_MS` | 10000 | Must stay ≤ 30000 (spec) |
| `TIMEOUT_DISPLAY_MS` | 5000 | |
| `HEAT_SAMPLE_MS` | 100 | |
| `BURN_FACTOR` | 1.5 | Burn line = factor × target |
| `BURN_RECOVER_FACTOR` | 1.4 | Hysteresis for Overheating → Cooking |
| `BURN_TIME_MS` | 3000 | |
| `STIR_TICK_MS` | 50 | |
| `STIR_DEBOUNCE_MS` | TBD | |
| `ICON_DISPLAY_MS` | 1500 | |
| `DIST_MIN_CM`, `DIST_MAX_CM` | TBD | Hand distance range mapped to heat |
| `HEAT_TARGET[5]` | TBD | One per ingredient |
| `OPTIMAL_STIR_MS[5]` | TBD | One per ingredient |
| `RECIPE_MASK` | TBD | Which of the 5 ingredients are right |
| `P_RIGHT`, `P_WRONG`, `P_BURN`, `K_HEAT`, `K_STIR` | TBD | Scoring weights |

---

## 8. Keyboard test mode (Checkpoint 2)

Checkpoint 2 requires a state machine driven by keyboard input. Map keys to the same events the hardware produces, so every service can be tested without sensors:

| Key | Simulates |
|---|---|
| `1`–`5` | `SWITCH_FLIPPED(n)` |
| `f` | `FINISH_PRESSED` |
| `s` / `x` | `STIR_START` / `STIR_STOP` |
| `+` / `-` | Hand moves closer / farther (step the simulated distance) |
| `a` | `USER_ACTIVITY` |

Also print each service’s state transitions to the terminal for debugging.

---

## 9. Build order and deadlines

| Date | Milestone | Firmware focus |
|---|---|---|
| 11/8 | Checkpoint 1: schematic, state diagram, BOM | Project skeleton, services registered, events defined, empty state machines compile |
| 11/13 | Checkpoint 2: 3 inputs sensed, ≥ 3-state SM on keyboard, actuation working | Keyboard test mode; Game, Ingredient and Score SMs working; spatula motor moving |
| 11/17 | Checkpoint 3: everything integrated | All sensors, all actuators, all six services |
| 11/18 | Grading session and public show | Must work on the first try |

---

## 10. Open items (do not invent these)

1. The dish or dishes, the right ingredients, and how the player learns the recipe pictorially.
2. All per-ingredient heat targets and optimal stir times.
3. Scoring weights, and whether the score may go negative.
4. Follower home angle, center deadband, startup reference, wraparound and what motion earns stir time. Holding a fixed angle should not automatically be treated as continued rotation; automatic return-home counting is undecided.
5. Exact hardware parts, drivers and pin assignments (from the team schematic).
6. Spatula orbit, physical pan dimensions/material/loaded mass, motor mounting, required torque and rotation feedback.
7. Sound hardware and how the sizzle is varied.
8. A more creative time display than plain digits.
9. Heat-bar layout and visible burn warning, including the last ingredient burning.
10. Pan-flip method: angle threshold or handle-operated switch; trigger position and return sequence. Define the animation and decide whether to award a bonus.
11. Fit, available strip count and power for the 3–4–3-module pan display; whether the spatula motor tilts with the pan; cable routing, movement stops and clearance from fingers and the spatula. The LEDs do tilt with the pan.
12. Whether lifting pauses stirring or heat scoring, and what counts as activity.

---

## 11. Instructions for the coding agent

- Keep the course ES framework and six-service structure in §6.3. Update the FSM to match confirmed story changes before implementing them; flip detection, animation priority and any bonus rules remain TBD.
- Keep every hardware access behind a small driver module so services stay hardware-independent and testable from the keyboard.
- Use named constants from `GameConfig.h` for every number in §7.
- For TBD items, leave clear placeholders and list them in a short “what the team must fill in” note.
- Do not add an RTOS, a second processor, or blocking code.
- Explain your structure in comments so every team member can walk a CA through it. The teaching team will question the team on the design.
