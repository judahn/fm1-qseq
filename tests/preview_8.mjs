// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// Phase 8 of the preview check: chord mode (design 8g) on the panel. The
// settings page turns it on for track 1. A type key picks major, and a root
// held plays the chord strummed up over a 16th, each note in its exact
// block. A type picked while the root is held plays the new chord at once.
// A chord let go mid-strum leaves the rest unplayed. F5, F#5 and G5 play
// the last chord again, an octave down and up. Smooth voicing over C, F, G
// and C. A recorded chord lands as notes, quantized as a whole by its first
// note. In step entry a root enters its chord strummed, the type keys work
// there too, F5 enters the last chord again, and with SHIFT held no key
// strikes a chord. A drum track ignores chord mode, and the settings are in
// the saved song. Saves docs/phase8-*.png.
import { simPage, savePng, checker } from './preview_lib.mjs';

export async function phase8(browser, root) {
  const page = await simPage(browser, root);
  const out = await page.evaluate(async () => {
    const r = previewRig(), { B, c } = r;
    const { tap, key, turn, render, info, png } = r, pageNow = r.page;
    const press = k => c.sim_control(1, k, 1), lift = k => c.sim_control(1, k, 0);
    const heard = () => r.heard().filter(h => !h.kind);
    const ons = list => list.filter(h => h.velocity).map(h => [h.block, h.pitch]);
    const offs = list => list.filter(h => !h.velocity).map(h => h.pitch).sort((a, b) => a - b);
    const setting = t => info(49, t);
    const notes = p => Array.from({ length: info(0, p) }, (_, i) => [info(1, p, i), info(2, p, i), info(30, p, i)]).filter(n => !n[2]);
    const o = {};
    render(32);
    tap(B.SEQ);
    o.songPage = pageNow();
    o.engines = [0, 1, 2, 3, 4, 5, 6, 7].map(t => info(34, t));
    const bpm = info(6, 0, 0), blocks = clocks => Math.floor(Math.floor(clocks * 264600000 / (480 * bpm)) / 128);

    // 1. The settings page's fifth page: chord mode on for track 1, a 16th strum, up.
    tap(B.GLO); turn(1, 4);
    o.setupPage = pageNow();
    turn(2, 1); turn(4, 3);
    o.setting = setting(0);
    o.knobs = r.knobs();
    render(4);
    o.setupPng = png();
    tap(B.GLO);
    o.back = pageNow();

    // 2. G4 picks major, then C4 held: C major strummed up, 0, 60 and 120 clocks.
    c.sim_song_capture(1);
    key(14);
    o.major = setting(0) >> 8;
    let at = info(50);
    press(7); render(60);
    o.cMajor = { got: ons(heard()), want: [[at, 60], [at + blocks(60), 64], [at + blocks(120), 67]] };
    // 3. A4, minor, picked while C4 is held: the major ends and C minor plays at once.
    c.sim_song_capture(1);
    at = info(50);
    key(16);
    render(60);
    o.retype = { offs: offs(heard()), got: ons(heard()), want: [[at, 60], [at + blocks(60), 63], [at + blocks(120), 67]],
      type: setting(0) >> 8 };
    c.sim_song_capture(1);
    lift(7); render(4);
    o.released = offs(heard());
    // 4. D4 let go two blocks in: only its first note ever sounds.
    c.sim_song_capture(1);
    press(9); render(2); lift(9); render(60);
    o.cut = { ons: ons(heard()).map(n => n[1]), offs: offs(heard()) };
    // 5. F5, F#5 and G5: D minor, the last chord, again, an octave down and up.
    o.top = [24, 25, 26].map(k => { c.sim_song_capture(1); press(k); render(60); lift(k); render(2); return ons(heard()).map(n => n[1]); });

    // 6. Smooth, the strum off: C, F, G, C (C4, F3, G3, C4) in major.
    tap(B.GLO); turn(3, 2); turn(4, -3); tap(B.GLO);
    key(14);
    o.smooth = [7, 0, 2, 7].map(k => { c.sim_song_capture(1); press(k); render(4); lift(k); render(2); return ons(heard()).map(n => n[1]); });

    // 7. Recorded: an overdub with a 16th input quantize, close, an 8th strum
    // up. The chord lands as notes, its first on the grid, the others 120 and
    // 240 after it.
    tap(B.GLO); turn(3, -2); turn(4, 4); tap(B.GLO);
    const before = notes(0), noteKey = n => n[0] + ':' + n[1];
    r.long(B.REC);                                  // the record settings, on OVERDUB
    o.chooser = pageNow();
    turn(3, -2); turn(4, 5);
    tap(B.REC);
    o.recording = info(29, 0, 0);
    render(230);
    press(7); render(120); lift(7); render(40);
    tap(B.PLAY); tap(B.PLAY);                       // PLAY ends the take, PLAY again stops
    const all = notes(0), had = new Map();
    for (const n of before) had.set(noteKey(n), (had.get(noteKey(n)) || 0) + 1);
    const added = all.filter(n => { const k = noteKey(n); if (had.get(k)) { had.set(k, had.get(k) - 1); return false; } return true; });
    o.recorded = { added: added.length, starts: added.map(n => n[0]), pitches: added.map(n => n[1]) };
    o.songPng = png();

    // 8. Step entry: the strum back to a 16th. C4 enters C major strummed at
    // the pointer, then A4 (minor) and D4 enter D minor after it. F5 enters
    // D minor again, once. With SHIFT held a root and a type key do neither.
    tap(B.GLO); turn(4, -1); tap(B.GLO);
    turn(6, 20);                                    // PRESETS to the song's end: the stop went back to the take's chord (0.5.0)
    r.long(B.REC); turn(2, 3); tap(B.REC);          // the record settings on STEP, and REC: step entry
    o.stepPage = pageNow();
    const fresh = (before, from) => { /* the notes added since `before`, from clock `from` */
      const was = new Map();
      for (const n of before) was.set(noteKey(n), (was.get(noteKey(n)) || 0) + 1);
      return notes(0).filter(n => { const k = noteKey(n); if (was.get(k)) { was.set(k, was.get(k) - 1); return false; } return true; })
        .map(n => [n[0] - from, n[1]]);
    };
    const pointer = info(17), step0 = notes(0);
    press(7); render(2); lift(7); render(2);
    key(16);
    const second = info(17);
    press(9); render(2); lift(9); render(2);
    const entered = fresh(step0, pointer);
    o.step = { added: entered.length, entered, second: second - pointer };
    const third = info(17), step1 = notes(0);
    press(24); render(2); lift(24); render(2);
    o.again = { entered: fresh(step1, third), moved: info(17) - third };
    const step2 = notes(0), type = setting(0) >> 8;
    c.sim_song_capture(1);
    r.held(B.FX, () => { press(7); render(2); lift(7); press(19); render(2); lift(19); render(2); });
    o.shifted = { entered: fresh(step2, 0).length, heard: heard().length, sameType: (setting(0) >> 8) === type };
    render(2);
    o.stepPng = png();
    r.step();                                       // SHIFT+REC leaves (0.5.0)

    // 9. Track 8, the 808, with chord mode set on: it shows DRUMS, a type key
    // changes nothing, and a key plays its pad, no chord.
    turn(0, 7);
    tap(B.GLO);
    turn(2, 1);
    o.drumSetting = setting(7) & 1;
    o.drumKnobs = r.knobs();
    tap(B.GLO);
    c.sim_song_capture(1);
    key(16);
    o.drumType = setting(7) >> 8;
    r.meter = true;
    press(0); render(40); lift(0); render(4);
    r.meter = false;
    o.drum = { chordNotes: heard().length, level: r.level() };

    // 10. Saved: the song's payload, version 4 on, holds the settings.
    const link = FM1SongWorkbench.link.simulator(async frames => r.core.command({ type: 'protocol', frames }));
    const id = await link.saveWorking(true), saved = FM1SongWorkbench.songFile.decode((await link.readSong(id)).bytes);
    o.saved = { version: saved.version, chords: Array.from(saved.chords), types: Array.from(saved.chordTypes) };
    o.live = [setting(0), setting(7)];
    return o;
  });
  for (const [name, key] of [['settings', 'setupPng'], ['song', 'songPng'], ['step', 'stepPng']]) savePng(root, 'phase8-' + name, out, key);
  await page.close();
  const t = checker('8'), q = out;
  t.eq('the song view', q.songPage, 19);
  t.eq('GLO, then ALGO four times: the chords page', q.setupPage, 21);
  t.eq('K1 on, K3 a 16th: track 1\'s setting', q.setting & 255, 1 | 3 << 3);
  t.match('the knobs show it', q.knobs.join(' | '), /MODE ON \| VOICING CLOSE \| STRUM 1\/16 \| DIRECTION UP/);
  t.eq('GLO goes back', q.back, 19);
  t.eq('G4 picks major', q.major, 1);
  t.eq('C4: C major strummed up over a 16th, each note in its exact block', q.cMajor.got, q.cMajor.want);
  t.eq('A4 while C4 is held: the major\'s notes end', q.retype.offs, [60, 64, 67]);
  t.eq('and C minor strums at once', q.retype.got, q.retype.want);
  t.eq('the type is minor now', q.retype.type, 2);
  t.eq('letting go ends C minor', q.released, [60, 63, 67]);
  t.eq('D4 let go two blocks in: only its first note sounds', q.cut.ons, [62]);
  t.eq('and only that one ends', q.cut.offs, [62]);
  t.eq('F5, F#5, G5: the last chord, an octave down, an octave up', q.top, [[62, 65, 69], [50, 53, 57], [74, 77, 81]]);
  t.eq('smooth: C, F, G, C move by steps', q.smooth, [[60, 64, 67], [60, 65, 69], [59, 62, 67], [60, 64, 67]]);
  t.eq('REC held: the record settings', q.chooser, 23);
  t.eq('recording', q.recording, 1);
  t.eq('the chord recorded as three notes', q.recorded.pitches, [60, 64, 67]);
  t.yes('quantized as a whole: the first on the 16th grid, the strum kept', q.recorded.starts.length === 3 && q.recorded.starts[0] % 120 === 0 &&
    q.recorded.starts[1] - q.recorded.starts[0] === 120 && q.recorded.starts[2] - q.recorded.starts[0] === 240, q.recorded.starts);
  t.eq('STEP picked on the record settings, REC: step entry', q.stepPage, 20);
  t.eq('six notes entered, two chords', q.step.added, 6);
  t.eq('C major then D minor, each strummed at its step', q.step.entered, [[0, 60], [60, 64], [120, 67],
    [q.step.second, 62], [q.step.second + 60, 65], [q.step.second + 120, 69]]);
  t.eq('F5 in step entry enters the last chord, D minor, again, once', q.again.entered, [[0, 62], [60, 65], [120, 69]]);
  t.eq('and moves on one step', q.again.moved, q.step.second);
  t.eq('with SHIFT a root and a type key strike nothing, enter nothing, pick no type', q.shifted, { entered: 0, heard: 0, sameType: true });
  t.eq('the 808 takes the setting', q.drumSetting, 1);
  t.match('and the page says it\'s a drum track', q.drumKnobs[0], /MODE DRUMS/);
  t.eq('a type key on the 808 changes nothing', q.drumType, 0);
  t.eq('a key on the 808 plays no chord', q.drum.chordNotes, 0);
  t.over('it plays its pad', q.drum.level, 1e-4);
  t.eq('the saved song is version 5 (0.6.6, 4 before)', q.saved.version, 5);
  t.eq('with tracks 1 and 8\'s settings', [q.saved.chords[0] | q.saved.types[0] << 8, q.saved.chords[7] | q.saved.types[7] << 8], q.live);
  return { phase: '8', checks: t.list, out };
}
