// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// Phase 2 of the preview check: step entry and the settings page, on a
// fresh simulator. A bar of lengths, triplets, a dot, a tie, a rest and a
// chord built key by key, played back on exact blocks. Swing and the key
// on the settings page with their key lights, and an out-of-scale key
// snapping up in step entry. Saves docs/phase2-step-view.png and
// docs/phase2-settings.png.
import { simPage, savePng, checker } from './preview_lib.mjs';

export async function phase2(browser, root) {
  const page = await simPage(browser, root);
  const out = await page.evaluate(() => {
    const r = previewRig(), { B, c } = r, o = {};
    const shiftKey = k => r.held(B.FX, () => r.key(k));
    r.render(32);
    r.tap(B.SEQ);
    r.turn(2, 12);                                 // K1: to the song end, bar 13
    const p0 = r.info(8);
    r.tap(B.REC); r.tap(B.REC);                    // REC's chooser, then STEP: step entry on track 1
    o.stepPage = r.page();
    o.pointer = r.info(17) - p0;
    // One bar: an 8th, three 8th triplets, a dotted 8th, a 16th tied to an
    // 8th (the length changes before the tie), an 8th rest, and a chord built
    // key by key.
    shiftKey(6); r.key(7);                         // 8th, C4
    shiftKey(1); r.key(9); r.key(11); r.key(12);   // 8th triplets D4 E4 F4
    shiftKey(1); shiftKey(3); r.key(14);           // dotted 8th G4
    shiftKey(3); shiftKey(3); shiftKey(4); r.key(16); // plain 16th A4
    shiftKey(6); r.key(26);                        // an 8th, then tie
    r.key(24);                                     // 8th rest
    const beforeChord = r.info(0, 0);
    c.sim_control(1, 7, 1); c.sim_control(1, 11, 1); c.sim_control(1, 14, 1);
    const whileHeld = r.info(0, 0);
    c.sim_control(1, 7, 0); c.sim_control(1, 11, 0); c.sim_control(1, 14, 0);
    o.chord = [whileHeld - beforeChord, r.info(0, 0) - beforeChord];
    const want = [[0, 60, 216], [240, 62, 144], [400, 64, 144], [560, 65, 144], [720, 67, 324], [1080, 69, 336],
      [1680, 60, 216], [1680, 64, 216], [1680, 67, 216]].map(([at, pitch, length]) => ({ clock: p0 + at, pitch, length }));
    o.want = want;
    o.stored = [];
    for (let i = 0; i < r.info(0, 0); i++) if (r.info(1, 0, i) >= p0) o.stored.push({ clock: r.info(1, 0, i), pitch: r.info(2, 0, i), length: r.info(3, 0, i) });
    o.pointerAfter = r.info(17) - p0;
    // Back onto the chord: K4 by an 8th. The pointer selects and previews it.
    c.sim_song_capture(1);
    r.turn(5, -1);
    o.previewedChord = r.heard().map(h => h.pitch).sort().join();
    r.render(4);
    o.stepPng = r.png();
    r.render(200);                                 // the preview times out
    // REC leaves at the pointer. Back to the bar line and play the phrase.
    r.tap(B.REC);
    o.backInSong = r.page();
    r.turn(2, -1);
    o.playhead = r.info(8) - p0;
    c.sim_song_capture(1);
    r.tap(B.PLAY);
    r.render(1600);
    const start = r.info(9), heard = r.heard(), { block } = r.tempo(), expected = [];
    for (const w of want) {
      expected.push(block(start, p0, w.clock) + ':' + w.pitch + ':on');
      expected.push(block(start, p0, w.clock + w.length) + ':' + w.pitch + ':off');
    }
    const got = heard.filter(h => h.track === 0).map(h => h.block + ':' + h.pitch + ':' + (h.velocity ? 'on' : 'off'));
    o.playback = { expected: expected.length, got: got.length, ok: JSON.stringify([...expected].sort()) === JSON.stringify([...got].sort()),
      first: got.slice(0, 4), playing: r.info(7) };
    // Settings: GLO tap opens them. Key A minor on page 2, then swing 25% on
    // page 1 with track 2 left straight.
    r.tap(B.GLO);
    o.settingsPage = r.page();
    r.turn(1, 1); r.turn(2, 9); r.turn(3, 2); r.turn(1, -1);
    r.turn(4, 25); r.key(2);
    o.swing = r.info(19) & 255; o.swingTracks = r.info(19) >>> 16; o.key = r.info(20);
    const leds = () => Array.from(r.state().subarray(168 + 14, 168 + 41));
    const whites = [0, 2, 4, 6, 7, 9, 11, 12];
    o.swingLeds = leds().every((on, k) => !!on === (whites.includes(k) && whites.indexOf(k) !== 1));
    r.render(4);
    o.settingsPng = r.png();
    r.tap(B.GLO);
    o.backFromSettings = r.page();
    // The key lights show A minor on a pitched track: the white keys here.
    const major = [0, 2, 4, 5, 7, 9, 11];
    o.scaleLeds = leds().every((on, k) => !!on === major.includes((53 + k) % 12));
    // In step entry an out-of-scale key snaps up: C#4 enters D4. At the song
    // end, where the track is empty.
    r.turn(2, 20);
    r.tap(B.REC); r.tap(B.REC);
    const p1 = r.info(17), count = r.info(0, 0);
    r.key(8);
    o.snapped = { added: r.info(0, 0) - count, at: r.info(1, 0, count) - p1, pitch: r.info(2, 0, count) };
    r.tap(B.ENV);                                  // back-delete removes it again
    o.backDeleted = { added: r.info(0, 0) - count, pointer: r.info(17) - p1 };
    r.tap(B.REC);
    r.held(B.FX, () => r.tap(B.GLO));
    o.shiftGlo = r.page();
    return o;
  });
  savePng(root, 'phase2-step-view', out, 'stepPng');
  savePng(root, 'phase2-settings', out, 'settingsPng');
  await page.close();
  const t = checker('2'), q = out;
  t.eq('REC, REC opens step entry', q.stepPage, 20);
  t.eq('the pointer starts at the playhead', q.pointer, 0);
  t.eq('a chord goes in when every key is up (held, then released)', q.chord, [0, 3]);
  t.eq('the bar went in as entered', q.stored, q.want);
  t.eq('landing on the chord previews it', q.previewedChord, '60,64,67');
  t.eq('REC leaves step entry for the song view', q.backInSong, 19);
  t.eq('K1 puts the playhead back on the phrase', q.playhead, 0);
  t.yes('the phrase plays on exact blocks, ons and offs', q.playback.ok, q.playback);
  t.eq('playback stopped at the song end', q.playback.playing, 0);
  t.eq('GLO opens the settings', q.settingsPage, 21);
  t.eq('swing set to 25%', q.swing, 25);
  t.eq('swing tracks: all but track 2', q.swingTracks, 0xfd);
  t.eq('key A minor', q.key, 9 | 2 << 8);
  t.yes('the key lights show the swinging tracks', q.swingLeds);
  t.eq('GLO goes back to the song view', q.backFromSettings, 19);
  t.yes('the key lights show the scale', q.scaleLeds);
  t.eq('an out-of-scale key snaps up to D4 at the pointer', q.snapped, { added: 1, at: 0, pitch: 62 });
  t.eq('back-delete takes it out again', q.backDeleted, { added: 0, pointer: 0 });
  t.eq('SHIFT+GLO opens the device settings', q.shiftGlo, 7);
  return { phase: '2', checks: t.list, out };
}
