# Website verification

Verified locally in the in-app browser before delivery.

- Desktop viewport: 1440 × 1000.
- Mobile viewport: 390 × 844; mobile menu opens, navigation closes it, content fits.
- Continuous range control: 100 mm displayed TOO MUCH; 350 mm displayed NOT ENOUGH; reset restored 220 mm / JUST RIGHT.
- Ingredient toggles update the in-pan icon display.
- Joystick arrow-key input changed the spatula sweep and ARIA value; reset centers it. Pointer handling is implemented with pointer capture.
- Opt-in sound toggled on/off successfully; no browser console errors/warnings during the checks. Browser sound is an illustration, not firmware waveform playback.
- Controller tabs and service tabs update their panels.
- Source search for ultrasonic returned the actual files. Selected ultrasonic.c and scaffold GameSM.c loaded; the scaffold TODO text remained visible.
- The CAD image modal opened and closed.
- The power schematic accordion expanded and collapsed.
- BOM filtering showed the selected category's rows.
- Local HTML references, internal anchor targets, unique IDs and every source-index file path validated automatically.
- Both companion pages have print styles. Print pagination depends on browser/printer settings; the report PDF contains the one-page function and one-page wisdom sections.
- Node's JavaScript syntax check passed.
- Browser inspection found no broken loaded images and no overwide content outside intentional scrollable tables/code.
- Native video controls and a re-encoded, fast-start H.264/AAC team video are included. No grading or hardware retest is implied by website QA.
- All resources use relative paths for repository-subpath GitHub Pages hosting.

Portable revision: CSS, scripts, display images and all source text are embedded in index.html to avoid missing relative resources and file:// fetch restrictions. The final public GitHub Pages deployment is checked after upload. Final grading firmware, exact recipe/scoring/timing/protocol data and missing BOM values remain content gaps explicitly labeled in the site.
