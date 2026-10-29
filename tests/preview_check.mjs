// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// Headless check of the browser preview (build/preview/index.html), phase by
// phase, each on a fresh simulator in its own page and module:
//   1  the test song, the loop, transport knobs, a jump, and the page routes (preview_1.mjs)
//   2  step entry and the settings page (preview_2.mjs)
//   keys  step entry's keys: SHIFT by note name in every octave, F5 to G5 as notes (preview_keys.mjs)
//   diagnostics  the hardware test build's page, A and B, in build/preview-test (preview_diag.mjs)
//   panel  every button light on, the wake, a page change at the LCD's device pace (preview_panel.mjs)
//   3  the selection, clipboard, undo and real-time takes (preview_3.mjs)
//   4  automation, chase, the conductor lane, program changes (preview_4.mjs)
//   4b glitch: repeats, ARP as the arpeggiator, mangle (preview_4b.mjs)
//   5a storage: save, load, kits, the recovery copy (preview_5a.mjs)
//   5b the workbench's Songs and Kits sections (preview_5b.mjs)
//   6  edit jobs (preview_6.mjs)
//   7  the clip grid and launching (preview_7.mjs)
//   7b clips in the song: takes, blocks, Expand, the open clip (preview_7b.mjs)
//   grid rec  REC into a cell: a fixed length, FREE, overdub, the count-in, undo (preview_grid_rec.mjs)
//   buttons  SAVE and the undo combos, FX's tap to the effects page, the engine's tempo (preview_buttons.mjs)
//   rec  0.4.0: REC tapped, SHIFT+REC, the record settings, SHIFT+SEL, playback following the view, blocks (preview_rec.mjs)
//   8  chord mode: keys, strums, smooth voicing, recording, step entry (preview_8.mjs)
//   yes/no  OCT+ and OCT- on the edit jobs and the song list: asking, cancelling, renaming (preview_yesno.mjs)
// Each records named checks with actual and expected values, and every
// failure is printed with its phase and name. Screenshots go to docs/.
// The diagnostics phase needs ./build.sh --test-preview, the rest ./build.sh --preview.
//   PLAYWRIGHT_MODULE=/path/to/node_modules/playwright node tests/preview_check.mjs
// PREVIEW_DUMP=1 prints every phase's full record too. PREVIEW_PHASES=3,7 runs
// only those phases (yes/no by that name).
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import { report } from './preview_lib.mjs';
import { phase1 } from './preview_1.mjs';
import { phase2 } from './preview_2.mjs';
import { phaseKeys } from './preview_keys.mjs';
import { phaseDiag } from './preview_diag.mjs';
import { phasePanel } from './preview_panel.mjs';
import { phase3 } from './preview_3.mjs';
import { phase4 } from './preview_4.mjs';
import { phase4b } from './preview_4b.mjs';
import { phase5a } from './preview_5a.mjs';
import { phase5b } from './preview_5b.mjs';
import { phase6 } from './preview_6.mjs';
import { phase7 } from './preview_7.mjs';
import { phase7b } from './preview_7b.mjs';
import { phaseGridRec } from './preview_grid_rec.mjs';
import { phaseButtons } from './preview_buttons.mjs';
import { phaseRec } from './preview_rec.mjs';
import { phase8 } from './preview_8.mjs';
import { phaseYesNo } from './preview_yesno.mjs';

const require = createRequire(import.meta.url);
const { chromium } = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const phases = [['1', phase1], ['2', phase2], ['keys', phaseKeys], ['diagnostics', phaseDiag], ['panel', phasePanel], ['3', phase3],
  ['4', phase4], ['4b', phase4b], ['5a', phase5a], ['5b', phase5b], ['6', phase6], ['7', phase7], ['7b', phase7b], ['grid rec', phaseGridRec], ['buttons', phaseButtons], ['rec', phaseRec], ['8', phase8],
  ['yes/no', phaseYesNo]];
const only = process.env.PREVIEW_PHASES ? process.env.PREVIEW_PHASES.split(',') : null;
const browser = await chromium.launch();
const runs = [];
for (const [name, run] of phases) if (!only || only.includes(name)) runs.push(await run(browser, root));
await browser.close();
const ok = report(runs);
console.log(ok ? 'preview_check: passed' : 'preview_check: FAILED');
process.exit(ok ? 0 : 1);
