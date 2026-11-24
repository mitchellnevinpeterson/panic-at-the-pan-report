# Panic at the Pan — interactive project website

A self-contained static website for ME218a Team 12. Built for GitHub Pages, without npm, a server backend, paid services, remote fonts, or API keys.

## Publish on GitHub Pages

### A separate website repository

1. Create a **public** GitHub repository, for example `panic-at-the-pan-report`.
2. Extract the website ZIP. Upload the **contents** of this folder to the repository: `index.html` must be at the repository root, alongside `style.css`, `app.js`, `assets/`, `downloads/`, `source/`, and the other files. Upload the files, not the ZIP itself.
3. Open the repository's **Settings → Pages**.
4. Under **Build and deployment**, choose **Deploy from a branch**. Select **main** and **/(root)**. Save.
5. Wait for GitHub's deployment to finish. The Pages settings will show the live website URL, normally `https://YOUR-USERNAME.github.io/panic-at-the-pan-report/`.
6. Visit the published URL, try the demo and downloads, then submit that URL in the course spreadsheet.

### Add to the existing team repository

Copy these website contents into a `docs/` folder in the team repository. Keep the existing firmware/hardware folders intact. In Settings → Pages choose `main` and `/docs`. No path edits are necessary: website links are relative, including source, images, video and downloads.

The `.nojekyll` file keeps this as a plain static site. If you use GitHub's web upload interface and it omits that hidden file, create an empty `.nojekyll` in the publishing folder.

Official instructions: https://docs.github.com/en/pages/getting-started-with-github-pages/configuring-a-publishing-source-for-your-github-pages-site

Website hosting repository: https://github.com/mitchellnevinpeterson/panic-at-the-pan-report

GitHub Pages address: https://mitchellnevinpeterson.github.io/panic-at-the-pan-report/

The team repository remains private. GitHub access or authentication is not embedded in the site.

## Local preview (fixed portable build)

Double-click `index.html` to open it in Chrome or Safari. Styling, interface images, controls, and the source viewer are bundled inside the HTML; they no longer require companion resource loading or a local server. Keep the `assets/` and `downloads/` folders beside it for the video, PDFs, and downloads.

An HTTP preview still works if preferred:

```sh
python3 -m http.server 8766
```

Open `http://localhost:8766`.

## Editing the portable build

Edit `templates/index.html`, `templates/function.html`, `templates/wisdom.html`, `style.css`, `app.js`, or the source files, then run:

```sh
python3 build_portable.py
```

This regenerates the three HTML entry pages with inline CSS, images and JavaScript. It embeds the actual source text for file:// browsing, avoiding local fetch restrictions. Video and report downloads remain relative links.

## Interactive features

- Continuous simulated hand-distance control, live heat readout, gradient bar and glow.
- Five illustrative ingredient toggles and an in-pan display.
- A draggable joystick; arrow keys and Home also work.
- Opt-in browser-synthesized background music, sizzle and zone chimes; stops when the tab becomes hidden.
- Controller tabs and six-service software explainer.
- Circuit-sheet accordions and CAD image lightbox.
- Searchable real C/header source viewer and raw-file links.
- Bill-of-materials category filters.
- Responsive mobile navigation, reduced-motion support and keyboard focus styles.
- Real team video, assembly photos, report PDF, source appendix, checkpoint PDFs and editable Overleaf ZIP.
- Stand-alone function and Gems of Wisdom pages with print styles.

The cooking demo is an illustration. Its icons, immediate zone changes and browser audio do not claim to reproduce final gameplay, recipes, score rules or the exact PIC32 audio waveform.

## Editing guide

- `templates/index.html`: editable report text, controls and BOM rows.
- `index.html`: generated portable entry page (the one to open and host).
- `style.css`: colors, type, layouts, animation and mobile/print styling.
- `app.js`: controls, audio, tabs, source viewer, BOM filters and image dialog.
- `function.html` / `wisdom.html`: stand-alone report pages.
- `source-index.json`: source viewer inventory.
- `source/`: supplied historical code snapshots and hardware/design files.
- `downloads/`: self-hosted report/source/checkpoint PDFs and Overleaf ZIP.
- `assets/`: photos, CAD, circuit drawings and the compressed real demonstration video.
- `QA.md`: verification notes.

## Required draft updates

The website transparently marks these as pending:

1. Integrated source is now included from wire_management at 0ffeea5; confirm the demonstrated revision.
2. The installed UART settings and application message format. Checkpoint 2 proposes UART2, 3.3 V, 115200 baud, 8N1, crossed TX pin 21 / RX pin 17. It is labeled a proposal.
3. Parts-price subtotal is $103.86, excluding tax/shipping. This includes the $9.99 sensor pack, $12.88 LCD, $24.95 e-paper display and $10 Gushers. The speaker was free. Other unlisted components were reclaimed from lab recycling bins or personal past-project stock with $0 new spending; final quantities, source categories and non-exempt market values still need reconciliation.
4. Final vector schematics and remaining mechanism dimensions. The supplied envelope is 51 × 36.5 × 14 cm; the site preserves its 1 cm difference from the 50 cm footprint specification.

The team reports complete operation and full grading credit. No grade sheet or quantitative reliability log was supplied. The site records that outcome as team-reported.

## Replacing the old source after the push

Copy the real game-controller and peripheral-controller code into clearly named folders under `source/`, then regenerate `source-index.json` or edit its list. Each entry has `path`, `name`, `group` and `label`. Update the source-group options and app filtering if adding a new group. Replace the source PDF/Overleaf ZIP and remove the pending-final-code text only after reviewing that actual revision.

Keep existing course/framework comments and attribution. No new blanket license is asserted over the supplied lab/framework files. `SOURCE-NOTES.md` records the available-source distinction.

## Report deadlines from the supplied specification

- Draft hosted URL: November 30, 2026, 4 p.m.
- Final revised report: December 4, 2026, 5 p.m.

The website must be published and its URL submitted; a local preview or ZIP alone is not the final hosted submission.
