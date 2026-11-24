# Audio — proposed

[Schematic](../sheets/audio.kicad_sch) · [Preview](../review/audio/schematic.svg)

`PIC32 AUDIO_PWM → attenuation/filter → PAM8302 board → speaker`

- Proposed **Adafruit 2130 amplifier** on the shared 5 V load rail, with a **4 Ω / 3 W speaker**.
- R60/R61 reduce 3.3 V PWM to at most ~0.30 Vpp before loading; C60/R62/C61 filter the carrier. Values are provisional.
- Firmware assumption: ≥62.5 kHz PWM carrier, duty modulation for ~0.5–2 kHz tones; steady duty for silence. No firmware added.
- The board already AC-couples its audio inputs. A− goes to ground; **neither speaker terminal goes to ground**. SD low mutes the amplifier. [Board connections](https://learn.adafruit.com/adafruit-pam8302-mono-2-5w-class-d-audio-amplifier/pinouts)
- SW30 provides manual mute without another GPIO. Automatic mute would require another output.

Before finalizing: confirm board/speaker, test volume, PWM residue, clipping and startup pop; include audio peaks in the shared **5 V / 3 A** budget. Begin with the onboard volume trim low. This circuit is for simple game tones; prerecorded speech would need a different audio source.

U30/LS30 expose logical assembly ports, not physical connector or bare-chip pin numbers. C62/C63 supplement the board’s bypassing; bulk value remains provisional.
