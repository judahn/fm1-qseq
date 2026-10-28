// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// Headless check of the browser preview (build/preview/index.html), phase by
// phase, each on a fresh simulator in its own page and module:
//   1  the test song, the loop, transport knobs, a jump, and the page routes (preview_1.mjs)
//   2  step entry and the settings page (preview_2.mjs)
//   diagnostics  each delivery method on the hardware page (preview_diag.mjs)
//   3  the selection, clipboard, undo and real-time takes (preview_3.mjs)
//   4  automation, chase, the conductor lane, program changes (preview_4.mjs)
//   4b glitch: repeats, the stutter, the FX lane, mangle (preview_4b.mjs)
//   5a storage: save, load, kits, the recovery copy (preview_5a.mjs)
//   5b the workbench's Songs and Kits sections (preview_5b.mjs)
//   6  edit jobs (preview_6.mjs)
//   7  the clip grid and launching (preview_7.mjs)
// Each records named checks with actual and expected values, and every
// failure is printed with its phase and name. Screenshots go to docs/.
//   PLAYWRIGHT_MODULE=/path/to/node_modules/playwright node tests/preview_check.mjs
// PREVIEW_DUMP=1 prints every phase's full record too. PREVIEW_PHASES=3,7 runs
// only those phases.
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import { report } from './preview_lib.mjs';
import { phase1 } from './preview_1.mjs';
import { phase2 } from './preview_2.mjs';
import { phaseDiag } from './preview_diag.mjs';
import { phase3 } from './preview_3.mjs';
import { phase4 } from './preview_4.mjs';
import { phase4b } from './preview_4b.mjs';
import { phase5a } from './preview_5a.mjs';
import { phase5b } from './preview_5b.mjs';
import { phase6 } from './preview_6.mjs';
import { phase7 } from './preview_7.mjs';

const require = createRequire(import.meta.url);
const { chromium } = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const phases = [['1', phase1], ['2', phase2], ['diagnostics', phaseDiag], ['3', phase3], ['4', phase4], ['4b', phase4b],
  ['5a', phase5a], ['5b', phase5b], ['6', phase6], ['7', phase7]];
const only = process.env.PREVIEW_PHASES ? process.env.PREVIEW_PHASES.split(',') : null;
const browser = await chromium.launch();
const runs = [];
for (const [name, run] of phases) if (!only || only.includes(name)) runs.push(await run(browser, root));
await browser.close();
const ok = report(runs);
console.log(ok ? 'preview_check: passed' : 'preview_check: FAILED');
process.exit(ok ? 0 : 1);
