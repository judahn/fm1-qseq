// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// Phase 4 of the preview check: automation, on a fresh simulator. The
// recording knobs page, a knob sweep recorded thinned and played back on
// exact blocks, chase when stopped, playing and jumping, the conductor lane
// (tempo and time signature), a program change, undo of each, and clearing
// a lane in the selection. Saves docs/phase4-*.png.
import { simPage, savePng, checker } from './preview_lib.mjs';

export async function phase4(browser, root) {
  const page = await simPage(browser, root);
  const out = await page.evaluate(() => {
    const r = previewRig(), { B, c, same } = r, info = r.info;
    const lane = (t, number, kind = 1) => Array.from({ length: info(0, t) }, (_, i) => i)
      .filter(i => info(30, t, i) === kind && (kind === 2 || info(2, t, i) === number))
      .map(i => ({ clock: info(1, t, i), value: kind === 2 ? info(2, t, i) : info(18, t, i) }));
    const undoKey = () => r.held(B.HOME, () => r.tap(B.SAVE));
    const cutoff = list => list.filter(h => h.kind === 0xb0 && h.track === 1 && h.a === 74);
    const o = { knobs: {}, sweep: {}, chase: {}, conductor: {}, program: {} };
    r.render(32);
    r.tap(B.SEQ);
    o.engines = [0, 1, 2, 3, 4, 5, 6, 7].map(t => info(34, t));

    // 1. The recording knobs page, for track 2 (the 303): K4 to the next parameter.
    const k = o.knobs;
    r.turn(0, 1);
    r.tap(B.GLO);
    r.turn(1, 2);
    k.page = r.page();
    k.before = [0, 1, 2, 3].map(i => info(31, 1, i) & 255);
    r.turn(5, 1);
    k.after = [0, 1, 2, 3].map(i => info(31, 1, i) & 255);
    r.render(4);
    k.png = r.png();
    r.tap(B.GLO);
    k.back = r.page();

    // 2. An overdub on track 2 with no count-in: K1 (cutoff) swept down and
    // back up, a turn every 58 ms (a 32nd is 62 ms here), so some 32nds get
    // two moves and keep the second.
    const s = o.sweep;
    r.held(B.FX, () => r.tap(B.PLAY));
    r.tap(B.REC); r.turn(2, 2); r.turn(3, -1); r.tap(B.REC);
    s.recording = info(29, 0, 0);
    r.render(100);
    const turns = [];
    for (let i = 0; i < 24; i++) {
      const at = info(28);
      r.turn(2, i < 12 ? -2 : 2);
      turns.push({ clock: at, value: info(31, 1, 0) >> 8 & 255 });
      r.render(20);
    }
    s.heldWhileTurning = info(31, 1, 0) >> 16 & 1;
    s.quietLane = info(33, 0, 0);                  // touch track | lane << 8
    r.render(4);
    s.png = r.png();
    r.render(120);                                 // 350 ms: the knob lets go
    s.heldAfter = info(31, 1, 0) >> 16 & 1;
    r.tap(B.PLAY);
    const grid = new Map();
    for (const t of turns) grid.set(t.clock - t.clock % 60, t.value);
    const expected = [...grid].map(([clock, value]) => ({ clock, value })).filter((e, i, a) => !i || e.value !== a[i - 1].value);
    const recorded = lane(1, 74);
    s.turns = turns.length;
    s.recorded = recorded.length;
    s.recordedOk = same(recorded, expected);
    s.thinned = recorded.every((e, i) => e.clock % 60 === 0 && (!i || e.clock - recorded[i - 1].clock >= 60));
    s.bytes = recorded.length * 8;
    s.first = recorded[0]; s.last = recorded[recorded.length - 1];

    // 3. Played back from the top: a chase at the start (the lane's first
    // value), then every change on its exact block.
    r.held(B.FX, () => r.tap(B.PLAY));
    c.sim_song_capture(1);
    r.tap(B.PLAY);
    r.render(600);
    const got = cutoff(r.heard()).map(h => [h.block, h.b]);
    r.tap(B.PLAY);
    const start = info(9), { block } = r.tempo();
    const want = [[start, recorded[0].value], ...recorded.map(e => [block(start, 0, e.clock), e.value])];
    s.playback = { expected: want.length, got: got.length, ok: same(got, want), first: got.slice(0, 3) };

    // 4. Chase: zoom to 8ths, put the playhead on beat 2 while stopped, and
    // the lane answers with its value just before it. Playing from there
    // starts with the same value. A marker jump while playing chases too.
    const ch = o.chase;
    r.turn(5, 3);
    r.held(B.FX, () => r.tap(B.PLAY));
    c.sim_song_capture(1);
    r.turn(2, 2);
    ch.playhead = info(8);
    const before = clock => { const e = recorded.filter(x => x.clock < clock); return e.length ? e[e.length - 1].value : recorded[0].value; };
    ch.want = before(ch.playhead);
    ch.firstValue = recorded[0].value;
    ch.stopped = cutoff(r.heard()).map(h => h.b);
    c.sim_song_capture(1);
    r.tap(B.PLAY);
    r.render(8);
    const firstPlay = cutoff(r.heard())[0];
    ch.atStart = firstPlay && firstPlay.block === info(9) ? firstPlay.b : -1;
    r.tap(B.PLAY);
    r.turn(5, -3);
    r.turn(2, 1);                                  // bar 2, past the sweep
    c.sim_song_capture(1);
    r.tap(B.PLAY);
    r.render(100);
    r.held(B.FX, () => r.key(12));                 // F4: marker 1, at the next bar line
    r.render(800);
    r.tap(B.PLAY);
    const barBlock = r.tempo().block(info(9), 1920, 3840);
    ch.jumpChase = cutoff(r.heard()).filter(h => h.block === barBlock).map(h => h.b);

    // 5. The conductor lane: SHIFT+LFO, SELECT up past track 1. ALGO sets
    // 140 BPM at bar 3, then PRESETS to time signatures and 3/4 from bar 4.
    const co = o.conductor;
    r.held(B.FX, () => r.tap(B.LFO));
    co.page = r.page();
    r.turn(0, -1); r.turn(0, -1);
    r.held(B.FX, () => r.tap(B.PLAY));
    r.turn(2, 2);
    r.turn(1, 20);
    co.tempos = Array.from({ length: info(4) }, (_, i) => [info(5, 0, i), info(6, 0, i)]);
    r.turn(6, 1);
    r.turn(2, 1);
    r.turn(1, -1);
    co.meters = Array.from({ length: info(32, 1) }, (_, i) => info(32, 0, i)).map(m => [m & 0xffff, m >> 16 & 255, m >>> 24]);
    // 6. A program change on track 1 (DX7) at bar 2.
    const pr = o.program;
    r.turn(0, 1);
    r.turn(6, 20);                                 // the last lane: programs
    r.held(B.FX, () => r.tap(B.PLAY));
    r.turn(2, 1);
    r.turn(1, 3);
    pr.lane = lane(0, 0, 2);
    r.turn(5, 2);                                  // 4 bars across for the picture
    r.render(4);
    o.autoPng = r.png();
    r.turn(5, -2);
    // Played from the top: notes keep exact time across the new tempo, and
    // the program change lands on its block.
    r.held(B.FX, () => r.tap(B.PLAY));
    c.sim_song_capture(1);
    r.tap(B.PLAY);
    r.render(2500);
    r.tap(B.PLAY);
    const start2 = info(9), block2 = r.tempo().block, heard2 = r.heard(), notes = [];
    for (let t = 0; t < 8; t++)
      for (let i = 0; i < info(0, t); i++)
        if (info(30, t, i) === 0) notes.push({ track: t, pitch: info(2, t, i), block: block2(start2, 0, info(1, t, i)) });
    const last = start2 + 2500, key = n => n.block + ':' + n.track + ':' + n.pitch;
    const wantNotes = notes.filter(n => n.block < last - 2).map(key).sort();
    const gotNotes = heard2.filter(h => h.kind === 0 && h.b && h.block < last - 2).map(h => key({ block: h.block, track: h.track, pitch: h.a })).sort();
    co.notes = { expected: wantNotes.length, got: gotNotes.length, ok: same(wantNotes, gotNotes) };
    co.after = notes.filter(n => n.block > block2(start2, 0, 3840) && n.block < last - 2).length; /* at 140 BPM */
    pr.played = heard2.filter(h => h.kind === 0xc0).map(h => [h.track, h.a, h.block]);
    pr.want = [[0, pr.lane[0]?.value, block2(start2, 0, 1920)]];
    // 7. Undo takes back the program change, the time signature and the
    // tempo, one step each.
    undoKey(); pr.lanesAfterUndo = lane(0, 0, 2).length;
    undoKey(); co.metersAfterUndo = info(32, 1);
    undoKey(); co.temposAfterUndo = info(4);
    // 8. SHIFT+SEL clears the shown lane in the selection: track 2's cutoff
    // over bars 1 and 2, then undo brings it back.
    r.turn(0, 1);
    r.turn(3, -3); r.turn(4, -4);
    co.loop = [info(14), info(15)];
    r.held(B.FX, () => r.tap(B.SCL));
    o.cleared = lane(1, 74).length;
    undoKey();
    o.restored = lane(1, 74).length;
    return o;
  });
  savePng(root, 'phase4-knobs', out.knobs, 'png');
  savePng(root, 'phase4-recording', out.sweep, 'png');
  savePng(root, 'phase4-automation', out, 'autoPng');
  await page.close();
  const t = checker('4'), { knobs: k, sweep: s, chase: ch, conductor: co, program: pr } = out;
  t.eq('the test song\'s engines', out.engines, [0, 1, 3, 4, 5, 0, 1, 2]);
  t.eq('GLO, ALGO twice: the recording knobs page', k.page, 21);
  t.eq('the 303\'s knobs: cutoff, resonance, env mod, decay', k.before, [74, 71, 1, 75]);
  t.eq('K4 picks the next parameter, volume', k.after, [74, 71, 1, 7]);
  t.eq('GLO goes back to the song view', k.back, 19);
  t.eq('the sweep is recording', s.recording, 1);
  t.eq('the knob is held while turning', s.heldWhileTurning, 1);
  t.eq('the song\'s own cutoff lane goes quiet on the take track', s.quietLane, 1 | 74 << 8);
  t.eq('the knob lets go after it stops', s.heldAfter, 0);
  t.yes('the sweep is recorded as turned, a 32nd apart', s.recordedOk, { recorded: s.recorded, first: s.first, last: s.last });
  t.yes('the recording is thinned to whole 32nds', s.thinned);
  t.under('fewer changes stored than turns made', s.recorded, s.turns);
  t.yes('played back: the chase, then each change on its block', s.playback.ok, s.playback);
  t.eq('chase while stopped: the value before the playhead', ch.stopped, [ch.want]);
  t.eq('chase as playing starts there', ch.atStart, ch.want);
  t.eq('chase at a jump, on the bar line', ch.jumpChase, [ch.firstValue]);
  t.eq('SHIFT+LFO opens the automation view', co.page, 24);
  t.eq('the conductor: 140 BPM at bar 3', co.tempos, [[0, 12000], [3840, 14000], [15360, 13300]]);
  t.eq('the conductor: 3/4 from bar 4', co.meters, [[0, 4, 4], [3, 3, 4]]);
  t.yes('notes keep exact time across the tempo changes', co.notes.ok, co.notes);
  t.over('notes checked across the song', co.notes.expected, 100);
  t.over('notes checked after the change to 140 BPM', co.after, 20);
  t.eq('one program change lane event', pr.lane.length, 1);
  t.eq('the program change is at bar 2', pr.lane[0]?.clock, 1920);
  t.eq('the program change plays on its block', pr.played, pr.want);
  t.eq('undo takes the program change back', pr.lanesAfterUndo, 0);
  t.eq('undo takes the time signature back', co.metersAfterUndo, 1);
  t.eq('undo takes the tempo back', co.temposAfterUndo, 2);
  t.eq('SHIFT+SEL clears the shown lane in the selection', out.cleared, 0);
  t.eq('undo brings the lane back', out.restored, s.recorded);
  return { phase: '4', checks: t.list, out };
}
