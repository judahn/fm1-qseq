// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// Phase 1 of the preview check: plays the test song through the simulator's
// panel controls and checks every note the player sent against exact timing
// worked out here in BigInt arithmetic, independent of the C player. Then a
// marker jump and the loop over many passes, the transport and loop knobs, a
// jump while playing landing on the bar line, SEL adding and removing
// markers, and the page route: SEQ to the clip grid and back, and SHIFT+GLO's
// device settings turning with ALGO.
// Saves docs/phase1-song-view.png.
import { simPage, savePng, checker } from './preview_lib.mjs';

export async function phase1(browser, root) {
  const page = await simPage(browser, root);
  const out = await page.evaluate(() => {
    const r = previewRig(), { B, c } = r, KEY_G4 = 14;
    r.meter = true;
    const notes = [];
    for (let t = 0; t < 8; t++)
      for (let i = 0; i < r.info(0, t); i++) notes.push({ track: t, clock: r.info(1, t, i), pitch: r.info(2, t, i) });
    const { tempos, den, frameNum } = r.tempo();

    // 1. The whole song from the top: SEQ opens the song view, PLAY starts it.
    r.render(32);
    c.sim_song_capture(1);
    r.tap(B.SEQ);
    const songPage = r.page();
    r.tap(B.PLAY);
    r.render(9000);
    const startBlock = r.info(9), list = r.heard(), ons = list.filter(n => n.velocity);
    const expected = notes.map(n => ({ track: n.track, pitch: n.pitch, block: startBlock + Number(frameNum(n.clock) / (den * 128n)) }));
    const tempoBlock = startBlock + Number(frameNum(tempos[1].clock) / (den * 128n));
    const whole = { songPage, startBlock, tempoChangeBlock: tempoBlock, playingAtEnd: r.info(7),
      notes: r.compare(expected, ons), offs: list.length - ons.length, balanced: r.balanced(list),
      afterTempoChange: ons.filter(n => n.block >= tempoBlock).length,
      perTrack: Array.from({ length: 8 }, (_, t) => ons.filter(n => n.track === t).length), rms: r.level() };
    // A few concrete notes, for the record: the 64th run and the bar 11 triplets.
    const sample = (track, clock) => {
      const n = notes.find(m => m.track === track && m.clock === clock), exact = Number(frameNum(clock)) / Number(den);
      return { clock, exactFrame: +exact.toFixed(3), expectedBlock: startBlock + Math.floor(exact / 128),
        heardBlock: ons.find(h => h.track === track && h.pitch === n.pitch && h.block >= startBlock + Math.floor(exact / 128) - 1)?.block };
    };
    whole.samples = [sample(0, 7 * 1920 + 1440), sample(0, 7 * 1920 + 1440 + 30), sample(0, 7 * 1920 + 1440 + 450),
      sample(0, 10 * 1920 + 80), sample(0, 10 * 1920 + 160)];

    // 2. SHIFT + G4 jumps to marker 2 while stopped, ENV turns the loop on
    // (bars 3 to 6), PLAY plays it. Every pass must keep exact time.
    r.rms = r.blocks = 0;
    c.sim_song_capture(1);
    r.held(B.FX, () => r.key(KEY_G4));
    const jumped = r.info(8);
    r.tap(B.ENV);
    r.tap(B.PLAY);
    r.render(7000);
    const loopStart = 2 * 1920, loopEnd = 6 * 1920, start2 = r.info(9), ons2 = r.heard().filter(n => n.velocity);
    const passFrames = frameNum(loopEnd) - frameNum(loopStart), lastBlock = start2 + 7000, expected2 = [];
    for (let pass = 0; pass < 4; pass++)
      for (const n of notes) {
        if (n.clock < loopStart || n.clock >= loopEnd) continue;
        const block = start2 + Number((frameNum(n.clock) - frameNum(loopStart) + BigInt(pass) * passFrames) / (den * 128n));
        if (block < lastBlock) expected2.push({ track: n.track, pitch: n.pitch, block });
      }
    const loop = { jumpedTo: jumped, startBlock: start2, passFrames: Number(passFrames) / Number(den),
      notes: r.compare(expected2, ons2), stillPlaying: r.info(7) === 1, rms: r.level() };
    const songPng = r.png();                       // the song view while the loop plays
    r.tap(B.PLAY);
    loop.balancedAfterStop = r.balanced(r.heard());
    r.render(2000); r.rms = r.blocks = 0; r.render(1000); // stopped, the engine falls silent: no stuck notes
    loop.rmsAfterStop = r.level();

    // 3. Panel controls. Encoders 2 to 5 are K1 to K4, 1 is ALGO and 6 PRESETS,
    // and SHIFT is FX.
    const controls = {};
    r.held(B.FX, () => r.tap(B.PLAY));             // SHIFT+PLAY: to the loop start
    controls.rewound = r.info(8);
    r.tap(B.ENV);                                   // loop off
    controls.loopOn = r.info(16) & 1;
    r.turn(1, 3);                                   // ALGO: zoom in to 2 bars, snap 1/8
    controls.zoomKnobs = r.knobs();
    r.turn(6, 1);                                   // PRESETS: one 8th
    controls.afterK1 = r.info(8);
    r.held(B.FX, () => r.turn(6, 1));               // SHIFT+PRESETS: next marker
    controls.afterShiftK1 = r.info(8);
    r.turn(4, -1); r.turn(5, 2);                    // K3 and K4: loop edges by snap (0.5.2)
    controls.loopEdges = [r.info(14), r.info(15)];
    r.turn(1, -3);
    // A marker jump while playing waits for the bar line: start at bar 1, ask
    // for marker 3 (BREAK, bar 7) in beat 2, then check the bar 2 line.
    r.held(B.FX, () => r.key(12));                  // F4: marker 1
    c.sim_song_capture(1);
    r.tap(B.PLAY);
    r.render(300);
    const start3 = r.info(9);
    r.held(B.FX, () => r.key(16));                  // A4: marker 3
    controls.pendingAfterRequest = (r.info(16) & 2) !== 0;
    controls.clockWhilePending = r.info(8);
    r.render(600);
    const barLine = start3 + Number(frameNum(1920) / (den * 128n)), list3 = r.heard().filter(n => n.velocity);
    controls.atBreak = notes.filter(n => n.clock === 6 * 1920).map(n => n.track + ':' + n.pitch).sort();
    controls.inBlock = list3.filter(n => n.block === barLine).map(n => n.track + ':' + n.pitch).sort();
    // After the bar line only bar 7 onward plays, in time from where bar 2 would have started.
    const lastBlock3 = start3 + 900, shift3 = frameNum(1920) - frameNum(6 * 1920);
    const afterJump = notes.map(n => ({ track: n.track, pitch: n.pitch, clock: n.clock,
      block: start3 + Number((frameNum(n.clock) + shift3) / (den * 128n)) })).filter(n => n.clock >= 6 * 1920 && n.block < lastBlock3);
    controls.afterJump = r.compare(afterJump, list3.filter(n => n.block >= barLine));
    controls.jumpBlock = barLine;
    controls.bar1PlayedOut = list3.filter(n => n.block > start3 + 300 && n.block < barLine).length;
    controls.clockAfterJump = r.info(8);
    r.tap(B.PLAY);
    controls.balancedAfterStop = r.balanced(r.heard());
    // SEL toggles a marker while stopped: on marker 2 (GROOVE, bar 3) it
    // removes it and HOME+SAVE undoes that. Removed again, SEL adds M1 there,
    // the first free name. Playing from there, SEL adds one at the nearest bar
    // line (zoomed out, the snap is a bar) and leaves bar 3's alone, and SEL
    // again on that spot adds nothing.
    const marks = () => Array.from({ length: r.info(12) }, (_, i) => r.info(13, 0, i));
    const markName = i => String.fromCharCode(...[0, 8, 16, 24].map(b => r.info(13, 1, i) >>> b & 255).filter(Boolean));
    const toggle = {};
    r.held(B.FX, () => r.key(KEY_G4));             // G4: marker 2
    toggle.before = marks();
    r.tap(B.SCL); toggle.removed = marks();
    r.held(B.HOME, () => r.tap(B.SAVE)); toggle.undone = marks();
    r.tap(B.SCL); r.tap(B.SCL); toggle.added = marks();
    toggle.addedName = markName(1);
    r.tap(B.PLAY); r.render(600);
    toggle.clock = r.info(8);
    r.tap(B.SCL); toggle.whilePlaying = marks();
    toggle.playedName = markName(2);
    r.tap(B.SCL); toggle.again = marks();
    r.tap(B.PLAY);
    controls.toggle = toggle;
    // SEQ: the song view to the clip grid and back (phase 7). The stock sync
    // and MIDI pages are in the device settings: SHIFT+GLO opens Bluetooth,
    // and ALGO turns on to the step page's 5 to 8 and stops there (the full build
    // has no diagnostics page), and back.
    const pages = [r.page()];
    r.tap(B.SEQ); pages.push(r.page());
    r.tap(B.SEQ); r.render(4); pages.push(r.page());
    r.held(B.FX, () => r.tap(B.GLO)); pages.push(r.page());
    for (let i = 0; i < 5; i++) { r.turn(1, 1); pages.push(r.page()); }
    for (let i = 0; i < 5; i++) { r.turn(1, -1); pages.push(r.page()); }
    r.tap(B.SEQ); pages.push(r.page());
    controls.pages = pages.join();
    return { whole, loop, controls, loopStart, loopEnd, songPng };
  });
  savePng(root, 'phase1-song-view', out, 'songPng');
  await page.close();
  const t = checker('1'), { whole: w, loop: l, controls: k, loopStart, loopEnd } = out;
  t.yes('whole song: every note on its exact block', w.notes.ok, w.notes);
  t.yes('whole song: note-ons and offs pair up', w.balanced);
  t.eq('whole song: stopped by itself at the end', w.playingAtEnd, 0);
  t.eq('SEQ opens the song view', w.songPage, 19);
  t.yes('loop: every pass on its exact blocks', l.notes.ok, l.notes);
  t.yes('loop: balanced after PLAY stops it', l.balancedAfterStop);
  t.over('whole song: the engine made sound (rms)', w.rms, 0.001);
  t.under('loop: silent once stopped (rms)', l.rmsAfterStop, 1e-4);
  t.eq('SHIFT+PLAY rewinds to the loop start', k.rewound, loopStart);
  t.eq('ENV turns the loop off', k.loopOn, 0);
  t.eq('PRESETS moves by the snap (an 8th zoomed in)', k.afterK1, loopStart + 240);
  t.eq('SHIFT+PRESETS goes to the next marker', k.afterShiftK1, 6 * 1920);
  const g = k.toggle, nearest = Math.round(g.clock / 1920 - 0.001) * 1920;
  t.eq('SEL on a marker, stopped, removes it', g.removed, g.before.filter(c => c !== 2 * 1920));
  t.eq('HOME+SAVE brings a removed marker back', g.undone, g.before);
  t.eq('SEL off a marker adds one there, named M1', [g.added, g.addedName], [g.before, 'M1']);
  t.eq('SEL while playing adds at the nearest bar line, named M2', [g.whilePlaying, g.playedName],
    [[...g.added, nearest].sort((a, b) => a - b), 'M2']);
  t.eq('SEL while playing never removes', g.again, g.whilePlaying);
  t.eq('K3 and K4 move the loop edges by snap', k.loopEdges, [loopStart - 240, loopEnd + 480]);
  t.yes('a jump asked for while playing waits', k.pendingAfterRequest);
  t.yes('the jump lands on the bar line', k.atBreak.length > 0 && JSON.stringify(k.inBlock) === JSON.stringify(k.atBreak),
    { atBreak: k.atBreak, inBlock: k.inBlock });
  t.yes('after the jump, bar 7 on plays in time', k.afterJump.ok, k.afterJump);
  t.yes('balanced after stopping the jump', k.balancedAfterStop);
  t.eq('SEQ grid route and SHIFT+GLO device route pages', k.pages, '19,27,19,7,1,1,1,1,1,1,1,1,7,7,19');
  return { phase: '1', checks: t.list, out };
}
