// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// The tune phase of the preview check (0.6.6): per-note pitch on the sampler.
// Settings page 5 on track 5, the sampler, is chromatic mode: on, pad 3. A
// live key plays the pad at its pitch from C4, the pad's tune moving for it
// and back at its setting once stopped, two octaves up and down at most 24.
// A take records the pad with the key's TUNE. In step entry, chromatic off,
// F3 enters pad 1 and SHIFT+K4 turns its TUNE, the footer saying so, and
// with chromatic on a key enters the pad with its pitch. Playing, each
// sampler note finds its pad at the setting plus its TUNE, untuned ones at
// the setting, and the stop puts every pad back. A tune the user turns
// meanwhile is theirs and stays. Played tunes don't make a saved song look
// changed, and the saved song holds chromatic mode. Track 1 (DX7) and track
// 8 (808) play a TUNE word as if it weren't there. Saves docs/tune-*.png.
import { simPage, savePng, checker } from './preview_lib.mjs';

export async function phaseTune(browser, root) {
  const page = await simPage(browser, root);
  const out = await page.evaluate(async () => {
    const r = previewRig(), { B, c } = r;
    const { tap, turn, render, info, png } = r, pageNow = r.page;
    const press = k => c.sim_control(1, k, 1), lift = k => c.sim_control(1, k, 0);
    const T1 = 0, T5 = 4, T8 = 7, PADS = [0, 1, 2, 3, 4, 5, 6, 7];
    const padTune = (pad, t = T5) => info(60, t, pad) & 255;
    const tunes = () => PADS.map(p => padTune(p));
    const tuneOf = w => { const v = (w >>> 20) & 63; return v > 31 ? v - 64 : v; };
    const notes = t => Array.from({ length: info(0, t) }, (_, i) => ({ i, at: info(1, t, i), pitch: info(2, t, i), kind: info(30, t, i),
      tune: tuneOf(info(35, t, i)) })).filter(n => !n.kind);
    const o = {};
    render(32);
    tap(B.SEQ);
    o.songPage = pageNow();
    turn(0, 4);                                     // SELECT: track 5, the sampler
    o.engine = info(34, T5);
    o.base = tunes();

    // 1. Settings page 5 on the sampler: chromatic mode, K1 on, K2 pad 3.
    tap(B.GLO); turn(1, 4);
    o.setupPage = pageNow();
    render(4);
    o.offKnobs = r.knobs();
    turn(2, 1); turn(3, 2);
    render(4);
    o.chromatic = info(61, T5) & 255;
    o.onKnobs = r.knobs();
    o.setupPng = png();
    turn(3, 10);
    o.padTop = info(61, T5) & 255;
    turn(3, -5);
    o.padBack = info(61, T5) & 255;
    tap(B.GLO);

    // 2. A live key: E4 is 4 above C4, so pad 3 plays at its setting plus 4,
    // and the setting comes back two blocks on, the key still down.
    const hit = k => { const seen = []; press(k); for (let i = 0; i < 8; i++) { render(1); seen.push(padTune(2)); } lift(k); render(12); return seen; };
    r.meter = true;
    o.e4 = hit(11);
    r.meter = false;
    o.e4Level = r.level();
    o.e4After = tunes();
    tap(B.OCT_UP); tap(B.OCT_UP); render(4);
    o.g5Up2 = hit(26);                              // G5 two octaves up: 43 semitones, 24 at most
    tap(B.OCT_DOWN); tap(B.OCT_DOWN); tap(B.OCT_DOWN); tap(B.OCT_DOWN); render(4);
    o.f3Down2 = hit(0);                             // F3 two down: -31, -24 at most
    tap(B.OCT_UP); tap(B.OCT_UP); render(4);
    o.octavesAfter = tunes();

    // 3. A take on track 5, count-in off: D#4, 3 above C4, lands as pad 3 with TUNE +3.
    r.long(B.REC);
    o.chooser = pageNow();
    turn(3, -2);
    tap(B.REC);
    o.recording = info(29, 0, 0);
    render(100);
    press(10); render(30); lift(10); render(20);
    tap(B.PLAY); tap(B.PLAY);                       // PLAY ends the take, PLAY again stops
    render(12);
    o.take = notes(T5).filter(n => n.at < 3840);
    o.takeAfter = tunes();

    // 4. Chromatic off, step entry on track 5: F3 enters pad 1 at the pointer,
    // and back on it SHIFT+K4 turns its TUNE to +3, the footer saying TUNE +3.
    tap(B.GLO); turn(1, 4); turn(2, -1); tap(B.GLO);
    o.chromaticOff = info(61, T5) & 255;
    r.step();
    o.stepPage = pageNow();
    const pointer = info(17);
    press(0); render(2); lift(0); render(2);
    turn(6, -1);
    render(2);
    r.held(B.FX, () => { turn(5, 3); render(4); o.tuneKnobs = r.knobs(); });
    o.stepped = notes(T5).filter(n => n.at === pointer).map(n => [n.pitch, n.tune]);
    render(4);
    o.stepPng = png();
    o.plainKnob = r.knobs()[3];
    r.held(B.FX, () => { turn(5, -40); render(2); o.clamped = notes(T5).filter(n => n.at === pointer).map(n => n.tune); turn(5, 27); render(2); });
    o.backTo = notes(T5).filter(n => n.at === pointer).map(n => n.tune);
    render(12);
    o.stepAfter = tunes();
    r.step();

    // 5. Chromatic on, step entry: D4, 2 above C4, enters pad 3 with TUNE +2,
    // and the entry shows it.
    tap(B.GLO); turn(1, 4); turn(2, 1); tap(B.GLO);
    r.step();
    turn(6, 2);
    const there = info(17);
    press(9); render(2); lift(9); render(2);
    o.chromaticStep = { entered: notes(T5).filter(n => n.at === there).map(n => [n.pitch, n.tune]), entry: tuneOf(info(62)),
      moved: info(17) - there };
    render(12);
    r.step();

    // 6. Playing from the top: each note on track 5 finds its pad at the
    // setting plus its TUNE as it goes, untuned rims (pad 3) at the setting.
    turn(6, -20);
    o.start = info(59);
    c.sim_song_capture(1);
    tap(B.PLAY);
    const after = new Map();
    for (let i = 0; i < 2000; i++) { render(1); after.set(info(50) - 1, tunes()); }
    const ons = r.heard().filter(h => !h.kind && h.velocity && h.track === T5);
    const want = notes(T5).sort((a, b) => a.at - b.at);
    const padOf = { 36: 0, 38: 1, 37: 2, 41: 3, 47: 4, 48: 5, 42: 6, 46: 7 };
    o.played = ons.slice(0, 6).map((h, k) => ({ pitch: h.pitch, got: (after.get(h.block) || [])[padOf[h.pitch]], want: 24 + (want[k] ? want[k].tune : 99),
      note: want[k] && want[k].pitch }));
    o.playingTunes = tunes();
    tap(B.PLAY);
    o.stopSoon = tunes();
    render(12);
    o.stopped = tunes();

    // 7. Saved, then played: the song still looks saved, the sampler's sound
    // saved clean though the engine marks it edited, and chromatic mode is in
    // the saved song (version 5).
    const link = FM1SongWorkbench.link.simulator(async frames => r.core.command({ type: 'protocol', frames }));
    const id = await link.saveWorking(true);
    render(4);
    o.savedClean = info(38, 0, 17);
    tap(B.PLAY); render(400); tap(B.PLAY); render(12);
    o.afterPlay = { unsaved: info(38, 0, 17), engineEdited: info(60, T5, 0) >> 8 & 1, tunes: tunes() };
    const saved = FM1SongWorkbench.songFile.decode((await link.readSong(id)).bytes);
    o.saved = { version: saved.version, chromatic: Array.from(saved.chromatic), modified: saved.sounds[1 + T5 * 223 + 3],
      tuned: saved.tracks[T5].filter(e => (e.at >>> 26 & 31) && tuneOf(saved.glitch.find(g => g.index === (e.at >>> 26 & 31)).word)).length };

    // 8. Pad 1 turned by the user while a note has it tuned: theirs stays,
    // and the next tuned note goes from it.
    tap(B.PLAY);
    render(40);
    o.pad1Playing = padTune(0);
    c.sim_song_tone(T5, 0, 2);
    tap(B.PLAY); render(12);
    o.pad1Kept = padTune(0);
    tap(B.PLAY); render(40);
    o.pad1Again = padTune(0);
    tap(B.PLAY); render(12);
    o.pad1Back = padTune(0);
    c.sim_song_tone(T5, 0, -5);
    render(4);
    o.pad1Setting = padTune(0);

    o.userEdit = info(38, 0, 17);                   // a tune the user turned is a change
    // 9. Track 1 (DX7) and track 8 (808) with TUNE on notes play the same as
    // without, and on track 8 SHIFT+K4 is still PAD. Page 5 there says DRUMS.
    const play = () => { turn(6, -20); c.sim_song_capture(1); tap(B.PLAY); const from = info(9); render(700); tap(B.PLAY); render(4);
      return r.heard().filter(h => !h.kind && (h.track === T1 || h.track === T8)).map(h => [h.block - from, h.track, h.pitch, h.velocity]); };
    const plain = play();
    const firsts = t => notes(t).slice(0, 2).map(n => n.i);
    o.applied = [...firsts(T1).map(i => c.sim_song_glitch(T1, i, 7 << 20)), ...firsts(T8).map(i => c.sim_song_glitch(T8, i, 59 << 20))];
    o.wordsSet = [...firsts(T1).map(i => tuneOf(info(35, T1, i))), ...firsts(T8).map(i => tuneOf(info(35, T8, i)))];
    const tunesBefore = tunes();
    const withTune = play();
    o.others = { plain: plain.length, same: JSON.stringify(plain) === JSON.stringify(withTune), padsStill: JSON.stringify(tunes()) === JSON.stringify(tunesBefore) };
    turn(0, 3);                                     // track 8
    tap(B.GLO); turn(1, 4); render(4);
    o.drumKnobs = r.knobs();
    tap(B.GLO);
    r.step();
    r.held(B.FX, () => { render(4); o.drumK4 = r.knobs()[3]; });
    r.step();
    return o;
  });
  for (const [name, key] of [['chromatic', 'setupPng'], ['step', 'stepPng']]) savePng(root, 'tune-' + name, out, key);
  await page.close();
  const t = checker('tune'), q = out;
  t.eq('the song view', q.songPage, 19);
  t.eq('track 5 is the sampler', q.engine, 5);
  t.eq('every pad starts at the middle of its tune', q.base, [24, 24, 24, 24, 24, 24, 24, 24]);
  t.eq('GLO, then ALGO four times: page 5', q.setupPage, 21);
  t.match('on the sampler it is chromatic mode, off, pad 1', q.offKnobs.join(' | '), /MODE OFF \| PAD 1 \|/);
  t.eq('K1 on, K2 to pad 3', q.chromatic, 8 | 2);
  t.match('the knobs show it', q.onKnobs.join(' | '), /MODE ON \| PAD 3 \|/);
  t.eq('K2 stops at pad 8', q.padTop, 8 | 7);
  t.eq('and comes back to pad 3', q.padBack, 8 | 2);
  t.yes('E4 tunes pad 3 four up as it plays', q.e4.includes(28), q.e4);
  t.eq('then back at its setting two blocks on, the key still down', q.e4.slice(-1)[0], 24);
  t.over('and it sounds', q.e4Level, 1e-4);
  t.eq('every pad at its setting after', q.e4After, q.base);
  t.yes('G5 two octaves up: 24 up at most', q.g5Up2.includes(48) && Math.max(...q.g5Up2) === 48, q.g5Up2);
  t.yes('F3 two octaves down: 24 down at most', q.f3Down2.includes(0), q.f3Down2);
  t.eq('the pads back at their settings', q.octavesAfter, q.base);
  t.eq('REC held: the record settings', q.chooser, 23);
  t.eq('recording', q.recording, 1);
  t.eq('the take: one note, pad 3, TUNE +3', q.take.map(n => [n.pitch, n.tune]), [[37, 3]]);
  t.eq('the pads back after the take', q.takeAfter, q.base);
  t.eq('chromatic off', q.chromaticOff, 2);
  t.eq('SHIFT+REC: step entry', q.stepPage, 20);
  t.eq('F3 entered pad 1, and SHIFT+K4 three steps: TUNE +3', q.stepped, [[36, 3]]);
  t.match('the footer says TUNE +3 with SHIFT held', q.tuneKnobs[3], /TUNE \+3/);
  t.match('and PAD without', q.plainKnob, /PAD/);
  t.eq('SHIFT+K4 stops at -24', q.clamped, [-24]);
  t.eq('and turns back to +3', q.backTo, [3]);
  t.eq('the pads back after the previews', q.stepAfter, q.base);
  t.eq('chromatic on: D4 enters pad 3 with TUNE +2', q.chromaticStep.entered, [[37, 2]]);
  t.eq('the entry shows +2', q.chromaticStep.entry, 2);
  t.over('and the pointer moves on', q.chromaticStep.moved, 0);
  t.eq('playing starts at the top', q.start, 0);
  t.yes('each note on track 5 finds its pad at the setting plus its TUNE', q.played.length === 6 &&
    q.played.every(p => p.pitch === p.note && p.got === p.want), q.played);
  t.yes('tuned and untuned notes both played', q.played.some(p => p.want !== 24) && q.played.some(p => p.want === 24), q.played);
  t.eq('two blocks after the stop, every pad is back', q.stopped, q.base);
  t.eq('pad 1 tuned +3 while its note plays', q.pad1Playing, 27);
  t.eq('turned by the user meanwhile, it stays theirs after the stop', q.pad1Kept, 29);
  t.eq('the next tuned note goes from theirs', q.pad1Again, 32);
  t.eq('and back at theirs after', q.pad1Back, 29);
  t.eq('a tune the user turns is a change', q.userEdit, 1);
  t.eq('saved: nothing unsaved', q.savedClean, 0);
  t.eq('played: still nothing unsaved', q.afterPlay.unsaved, 0);
  t.eq('the pads back', q.afterPlay.tunes, q.base);
  t.eq('the engine marks the sampler edited', q.afterPlay.engineEdited, 1);
  t.eq('but its sound was saved clean', q.saved.modified, 0);
  t.eq('the saved song is version 5', q.saved.version, 5);
  t.eq('with track 5 chromatic, pad 3', q.saved.chromatic, [0, 0, 0, 0, 8 | 2, 0, 0, 0]);
  t.eq('and its tuned notes', q.saved.tuned, 3);
  t.eq('TUNE set on two notes each of tracks 1 and 8', q.applied, [0, 0, 0, 0]);
  t.eq('the words hold it', q.wordsSet, [7, 7, -5, -5]);
  t.over('tracks 1 and 8 played', q.others.plain, 10);
  t.yes('and played the same with TUNE on them', q.others.same);
  t.yes('no pad moved for them', q.others.padsStill);
  t.match('page 5 on the 808 says DRUMS', q.drumKnobs[0], /MODE DRUMS/);
  t.match('SHIFT+K4 on the 808 is still PAD', q.drumK4, /PAD/);
  return { phase: 'tune', checks: t.list, out };
}
