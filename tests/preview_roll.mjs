// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// Step entry in 0.5.0, in the preview check. PLAY plays from the pointer's
// bar and stays, the playhead runs across the roll, the keys only sound,
// and PLAY again comes back to that bar with the pointer where it was. REC
// is a take here, staying here after, and SHIFT+REC leaves. ALGO zooms the
// roll (4 bars, 2 bars, 1 bar, half a bar), named in the header, with bar,
// beat and 16th or 32nd lines where they are 4 px apart, bar numbers at 2
// and 4 bars, and notes 1 px short. ARP steps the glitch pages, where ALGO
// still zooms. LFO and PRESETS sweep a selection, both ends in, keys toggle pitches in it,
// VELOCITY changes it relatively, and SEL deletes it in one undo step.
// Saves docs/step-roll-2bars.png and docs/step-sweep.png.
import { simPage, savePng, checker } from './preview_lib.mjs';

export async function phaseRoll(browser, root) {
  const page = await simPage(browser, root);
  const out = await page.evaluate(() => {
    const r = previewRig(), { B, c } = r, o = {};
    const { tap, key, held, turn, render, info } = r, pageNow = r.page;
    const zoom = () => info(58, 0, 0), glitch = () => info(58, 0, 1), shown = () => info(58, 0, 2), swept = () => info(58, 0, 5);
    const at = (x, y) => r.pixels()[y * 240 + x];
    const span = () => { /* the header's span label, read back as text is not possible: its width in DIM at y 18 */
      const lcd = r.pixels(); let left = 240;
      for (let y = 18; y < 25; y++) for (let x = 180; x < 240; x++) if (lcd[y * 240 + x] === 0x738e) left = Math.min(left, x);
      return 234 - left;
    };
    const BAR = 0x5acb, BEAT = 0x4228, FINE = 0x2104, BACK = 0, SHADE = 0x0861, GREEN = 0x07e0, TEXT = 0xef5d;
    const notes = t => Array.from({ length: info(0, t) }, (_, i) => ({ clock: info(1, t, i), pitch: info(2, t, i), velocity: info(18, t, i) }));
    render(32);
    tap(B.SEQ);
    turn(6, 12);                                    // PRESETS to the song's end, bar 13, where track 1 is empty
    const p0 = info(8);
    r.step();

    // 1. ALGO zooms: 1 bar to start, then 4 bars, 2 bars, half a bar, and
    // the lines at each, on an empty bar (y 60 is clear of the C line).
    const z = o.zoom = {};
    render(4);
    z.start = [zoom(), span()];
    const lines = () => [at(22, 60), at(36, 60), at(64, 60), at(120, 60), at(15, 60), at(11, 60)];
    const numbers = () => r.differ(r.pixels(), 123, 42, 135, 49, 0);
    z.bar = [lines(), numbers()];
    turn(1, -2); render(4);
    z.four = [zoom(), span(), lines(), numbers()];
    turn(1, -5); render(4);
    z.clamped = zoom();
    turn(1, 1); render(4);
    z.two = [zoom(), span(), lines(), numbers()];
    turn(1, 2); render(4);
    z.half = [zoom(), span(), lines()];
    turn(1, 3); render(4);
    z.top = zoom();
    turn(1, -1);                                    // 1 bar again

    // 2. Two 16ths back to back on C4 at gate 100%: a 1 px gap between them.
    held(B.FX, () => key(7));                       // SHIFT+C: 16ths
    turn(4, 2);                                     // K3: gate 100%
    key(7); key(7);
    render(4);
    o.gap = [at(20, 105), at(21, 105), at(22, 105)];
    o.gapNotes = notes(0).filter(n => n.clock >= p0).length;

    // 3. PLAY plays from the pointer's bar and stays, the playhead on the
    // roll. Keys only sound. PLAY again stops back at the bar, the pointer
    // where it was.
    const pl = o.play = {};
    const pointer = info(17), before = info(0, 0);
    c.sim_song_capture(1);
    tap(B.PLAY);
    render(60);
    pl.playing = [pageNow(), info(7), info(8) > p0];
    const lcd = r.pixels();
    let green = -1;
    for (let x = 8; x < 232; x++) if (lcd[100 * 240 + x] === GREEN) green = x;
    pl.playhead = green;
    render(60);
    pl.moved = info(8) > p0 + 100;
    key(9); key(11); held(B.LFO, () => key(14));     // picking is no entry either
    pl.keys = info(0, 0) - before;
    held(B.FX, () => key(8));                       // SHIFT+C#: no rest while playing
    pl.rest = info(17) - pointer;
    tap(B.PLAY);
    pl.stopped = [pageNow(), info(7), info(8) - p0, info(17) - pointer, info(59) - p0];
    c.sim_song_capture(0);

    // 4. REC here: a take on the track from the pointer's bar, with its
    // count-in, and the knobs record. REC ends it, still here, playing on.
    // SHIFT+REC leaves for the song view, the playhead at the pointer.
    const tk = o.take = {};
    tap(B.REC);
    tk.started = [pageNow(), info(29, 0, 0), info(7), shown(), r.knobs()[0].startsWith('LENGTH')];
    let n = 0;
    while (info(28) === 0xffffffff && n++ < 3000) render(1); // past the count-in
    const had = info(0, 0);
    c.sim_control(1, 12, 1); render(30); c.sim_control(1, 12, 0); render(10);
    tk.recorded = info(0, 0) - had;                 // in as the key comes up
    tap(B.REC);
    tk.ended = [pageNow(), info(29, 0, 0), info(7)];
    tap(B.PLAY);
    tk.back = info(8) - p0;
    held(B.LFO, () => turn(6, 1));                  // nothing to do with the take: a sweep of one step
    turn(6, 3);                                     // and the pointer on, which ends the sweep
    const left = info(17);
    r.step();
    tk.left = [pageNow(), info(8) - left, info(59) - left];

    // 5. The glitch layer: ARP steps pages 1, 2, 3 and off, and ALGO zooms on it.
    const gl = o.glitch = {};
    r.step();
    gl.pages = [glitch()];
    for (let i = 0; i < 4; i++) { tap(B.ARP); gl.pages.push(glitch()); }
    tap(B.ARP);
    turn(1, -1);
    gl.zoomed = [glitch(), zoom()];
    tap(B.ARP); tap(B.ARP); tap(B.ARP);
    gl.off = glitch();
    turn(1, 1);

    // 6. A sweep: bar 14 gets alternating C4 and E4 16ths. Hold LFO and
    // turn PRESETS to the bar's last 16th: all 16. C4 alone with LFO and its key, E4 back
    // in, and out again. VELOCITY lifts the C4s by 5, SEL deletes them, and
    // one undo brings them back.
    const sw = o.sweep = {};
    const move = n => { for (; n; n -= Math.sign(n) * Math.min(Math.abs(n), 100)) turn(6, Math.sign(n) * Math.min(Math.abs(n), 100)); };
    move(-400);                                     // the pointer to the start (a turn is at most 127)
    move(13 * 16);                                  // bar 14, at the song's end
    for (let i = 0; i < 16; i++) key(i % 2 ? 11 : 7);
    turn(6, -16);
    const bar = info(17);
    const inBar = () => notes(0).filter(n => n.clock >= bar && n.clock < bar + 1920);
    sw.entered = inBar().length;
    c.sim_control(0, B.LFO, 1);
    turn(6, 15);                                    // the pointer on the bar's last 16th
    sw.all = [swept(), info(58, 0, 3) & 1, info(58, 0, 4) - bar];
    key(7);
    sw.c4 = swept();
    key(11);
    sw.both = swept();
    render(2);                                      // a poll between two taps of one key
    key(11);
    sw.again = swept();
    render(4);
    o.sweepPng = r.png();
    sw.band = [at(60, 60), at(100, 60)];          /* the band over the first beats */
    c.sim_control(0, B.LFO, 0);
    const v0 = inBar().map(n => n.velocity);
    turn(3, 5);                                     // K2: VELOCITY on the C4s
    sw.velocity = inBar().map((n, i) => n.velocity - v0[i]).join();
    tap(B.SCL);
    sw.deleted = inBar().map(n => n.pitch).join();
    held(B.HOME, () => tap(B.SAVE));
    sw.undone = inBar().length;
    turn(6, 1);                                     // the pointer on without LFO: the sweep goes
    sw.cleared = info(58, 0, 3) & 1;
    r.step();

    // For the guide: bars 7 and 8 of the test song at 2 bars, the 64th run.
    while (info(8) > 6 * 1920) turn(6, -1);
    r.step();
    turn(1, -1); render(4);
    o.twoPng = r.png();
    turn(1, 1);
    r.step();
    return o;
  });
  savePng(root, 'step-roll-2bars', out, 'twoPng');
  savePng(root, 'step-sweep', out, 'sweepPng');
  await page.close();
  const t = checker('roll'), q = out, z = q.zoom, BAR = 0x5acb, BEAT = 0x4228, FINE = 0x2104, BACK = 0, SHADE = 0x0861;
  t.eq('step entry opens at 1 bar, named in the header (its width in DIM)', z.start, [2, 29]);
  t.eq('1 bar: 16th lines faint, beats a step brighter, no 32nds, no bar numbers', z.bar,
    [[FINE, FINE, BEAT, BEAT, BACK, BACK], 0]);
  t.eq('ALGO left twice: 4 bars, bar lines at each bar, beat lines, no 16ths', [z.four[0], z.four[1], z.four[2]],
    [0, 36, [BEAT, BEAT, BAR, BAR, BACK, BACK]]);
  t.over('4 bars: a bar number over the third bar', z.four[3], 0);
  t.eq('no further out than 4 bars', z.clamped, 0);
  t.eq('2 bars: beat lines 28 px apart, the second bar line at x 120, no 16ths', [z.two[0], z.two[1], z.two[2]],
    [1, 36, [BACK, BEAT, BEAT, BAR, BACK, BACK]]);
  t.over('2 bars: bar 14 numbered over its line', z.two[3], 0);
  t.eq('half a bar: 32nds and 16ths faint, the beat at x 120', z.half, [3, 41, [FINE, FINE, FINE, BEAT, BACK, BACK]]);
  t.eq('no closer than half a bar', z.top, 3);
  t.eq('two 16ths back to back: a 1 px gap where the first ends', q.gap, [0x3c9f, BACK, 0x3c9f]);
  t.eq('both went in', q.gapNotes, 2);
  t.eq('PLAY plays and stays in step entry', q.play.playing, [20, 1, true]);
  t.over('the playhead is on the roll', q.play.playhead, 8);
  t.yes('and moves on', q.play.moved);
  t.eq('keys while playing enter nothing, picking or not', q.play.keys, 0);
  t.eq('SHIFT+C# while playing moves nothing', q.play.rest, 0);
  t.eq('PLAY again stops back at the pointer\'s bar, the start point, the pointer where it was', q.play.stopped,
    [20, 0, 0, 0, 0]);
  t.eq('REC tapped: a take here, from a stop with its count-in, the roll shown, the knobs recording', q.take.started,
    [20, 1, 1, 20, false]);
  t.eq('a key played in the take is in as it comes up', q.take.recorded, 1);
  t.eq('REC ends it, still in step entry, playing on', q.take.ended, [20, 0, 1]);
  t.eq('PLAY stops it back at the pointer\'s bar', q.take.back, 0);
  t.eq('SHIFT+REC leaves for the song view, the playhead and the start point at the pointer', q.take.left, [19, 0, 0]);
  t.eq('ARP steps the glitch layer: pages 1, 2 and 3, then off', q.glitch.pages, [0, 1, 2, 3, 0]);
  t.eq('ALGO zooms on the glitch layer', q.glitch.zoomed, [1, 1]);
  t.eq('ARP on to off', q.glitch.off, 0);
  const s = q.sweep;
  t.eq('a bar of alternating 16ths entered', s.entered, 16);
  t.eq('LFO and PRESETS sweep the bar: 16 notes, from where it began', s.all, [16, 1, 0]);
  t.eq('LFO and C4 in the sweep: the C4s alone', s.c4, 8);
  t.eq('LFO and E4: the E4s in too', s.both, 16);
  t.eq('LFO and E4 again: out', s.again, 8);
  t.eq('the band shades the sweep, beats still alternating', s.band, [0x0884, 0x10a6]);
  t.eq('VELOCITY lifts the C4s by 5 and leaves the E4s', s.velocity, Array.from({ length: 16 }, (_, i) => (i % 2 ? 0 : 5)).join());
  t.eq('SEL deletes the C4s', s.deleted, Array(8).fill(64).join());
  t.eq('one undo brings them back', s.undone, 16);
  t.eq('the pointer moved without LFO: no sweep', s.cleared, 0);
  return { phase: 'roll', checks: t.list, out };
}
