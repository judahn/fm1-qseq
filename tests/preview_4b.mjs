// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// Phase 4b of the preview check: glitch. Repeats entered in step entry on a
// synth track and played on exact blocks, a held stutter that repeats the
// slice with its drift and rejoins in time, the same stutter recorded on the
// FX lane and played back the same, and mangle with undo. Saves
// docs/phase4b-*.png.
import { simPage, savePng, checker } from './preview_lib.mjs';

export async function phase4b(browser, root) {
  const page = await simPage(browser, root);
  const out = await page.evaluate(() => {
    const r = previewRig(), { B, c, same } = r, info = r.info;
    const heard = () => r.heard().filter(h => !h.kind);
    const notes = t => Array.from({ length: info(0, t) }, (_, i) => i).filter(i => info(30, t, i) === 0)
      .map(i => ({ clock: info(1, t, i), pitch: info(2, t, i), length: info(3, t, i), glitch: info(35, t, i) }));
    const o = { step: {}, stutter: {}, lane: {} };
    r.render(32);
    r.tap(B.SEQ);

    // 1. Step entry on track 1 (DX7) at the song end: the glitch layer, 4
    // repeats on a quarter C4, then 3 repeats (a triplet ratchet) on D4.
    const st = o.step;
    r.turn(2, 12);
    const at = info(8);
    r.tap(B.REC); r.tap(B.REC);
    r.tap(B.ARP);
    r.held(B.FX, () => r.key(4));                  // SHIFT+A3: quarter notes
    r.turn(2, 3);                                  // REPEATS x4
    r.key(7);
    r.turn(2, -1);                                 // x3
    r.key(9);
    r.render(4);
    st.png = r.png();
    r.tap(B.ARP);
    r.tap(B.REC);
    st.entered = notes(0).filter(n => n.clock >= at);
    st.repeats = st.entered.map(n => [3, 2].indexOf(n.glitch & 7) >= 0 ? (n.glitch & 7) === 3 ? 4 : 3 : 0);
    r.turn(2, -1);                                 // back to the bar line where they start
    c.sim_song_capture(1);
    r.tap(B.PLAY);
    r.render(700);
    const ons = heard().filter(h => h.velocity && h.track === 0), block = r.tempo().block, start = info(9);
    if (info(7)) r.tap(B.PLAY);                    // it may have reached the song's end
    st.want = [];
    st.entered.forEach((n, i) => { const k = st.repeats[i]; for (let j = 0; j < k; j++) st.want.push(block(start, at, n.clock + Math.floor((j * n.length + Math.floor(k / 2)) / k)) + ':' + n.pitch); });
    st.got = ons.map(h => h.block + ':' + h.pitch);

    // 2. A held stutter from bar 3 (marker 2): GLITCH down about 0.8 s in,
    // K1 drifts each repeat up 2 semitones, released 1.1 s later.
    const su = o.stutter;
    r.held(B.FX, () => r.key(14));                 // SHIFT+G4: marker 2
    const origin = info(8);
    c.sim_song_capture(1);
    r.tap(B.PLAY);
    r.render(300);
    c.sim_control(0, B.ARP, 1);
    r.turn(2, 2);
    su.at = info(36, 0, 0);
    r.render(400);
    su.png = r.png();
    c.sim_control(0, B.ARP, 0);
    su.to = info(36, 0, 1);
    r.render(600);
    const list = heard();
    r.tap(B.PLAY);
    const start2 = info(9), map = r.tempo().block, block2 = clock => map(start2, origin, clock), slice = 240, from = su.at - slice;
    const engines = [0, 1, 2, 3, 4, 5, 6, 7].map(t => info(34, t));
    const lo = block2(su.at), hi = block2(su.to), endBlock = start2 + 1290;
    const wantRep = [], wantAfter = [];
    for (let t = 0; t < 8; t++) {
      const src = notes(t).filter(n => n.clock >= from && n.clock < su.at);
      for (let k = 0; src.length; k++) {
        const first = su.at + k * slice;
        if (first >= su.to) break;
        for (const n of src) {
          const when = first + n.clock - from;
          if (when < su.to) wantRep.push(block2(when) + ':' + t + ':' + (engines[t] === 2 || engines[t] === 5 ? n.pitch : n.pitch + 2 * k));
        }
      }
      for (const n of notes(t)) if (n.clock >= su.to && block2(n.clock) < endBlock) wantAfter.push(block2(n.clock) + ':' + t + ':' + n.pitch);
    }
    const gotRep = list.filter(h => h.velocity && h.block >= lo && h.block < hi).map(h => h.block + ':' + h.track + ':' + h.pitch);
    const gotAfter = list.filter(h => h.velocity && h.block >= hi && h.block < endBlock).map(h => h.block + ':' + h.track + ':' + h.pitch);
    su.replays = { expected: wantRep.length, got: gotRep.length, ok: same([...wantRep].sort(), [...gotRep].sort()) };
    su.rejoined = { expected: wantAfter.length, got: gotAfter.length, ok: same([...wantAfter].sort(), [...gotAfter].sort()) };
    su.balanced = r.balanced(heard());

    // 3. The same during an overdub take, no count-in: it lands on the FX
    // lane, and playing the song back sounds the same as the take did.
    const la = o.lane;
    r.held(B.FX, () => r.key(14));
    r.tap(B.REC); r.turn(2, 2); r.turn(3, -1);
    c.sim_song_capture(1);
    r.tap(B.REC);
    r.render(300);
    c.sim_control(0, B.ARP, 1);                    // the drift stays at +2
    r.render(400);
    c.sim_control(0, B.ARP, 0);
    r.render(600);
    const take = heard().filter(h => !(h.track === 7 && h.pitch === 37)).map(h => [h.block - info(9), h.track, h.pitch, h.velocity]);
    r.tap(B.PLAY);
    la.fx = Array.from({ length: info(0, 0) }, (_, i) => i).filter(i => info(30, 0, i) === 3)
      .map(i => ({ clock: info(1, 0, i), slice: info(2, 0, i), drift: info(18, 0, i) - 64, length: info(3, 0, i) }));
    r.held(B.FX, () => r.key(14));
    c.sim_song_capture(1);
    r.tap(B.PLAY);
    r.render(1300);
    const back = heard().map(h => [h.block - info(9), h.track, h.pitch, h.velocity]);
    r.tap(B.PLAY);
    const window = take.filter(h => h[0] < 1290), again = back.filter(h => h[0] < 1290);
    la.compared = window.length;
    la.same = same(window, again);
    // The FX lane in the automation view: the conductor row, PRESETS to FX.
    r.held(B.FX, () => r.tap(B.LFO));
    r.turn(0, -1); r.turn(6, 2);
    r.turn(5, 2);
    r.render(4);
    o.lanePng = r.png();
    r.turn(5, -2);
    r.held(B.FX, () => r.tap(B.LFO));
    // 4. Mangle from the song view: SHIFT+F#3 scrambles bars 3 to 6 of track
    // 1, and undo puts them back.
    const before = notes(0).map(n => n.clock + ':' + n.pitch).join();
    r.held(B.FX, () => r.key(1));
    const after = notes(0).map(n => n.clock + ':' + n.pitch).join();
    r.held(B.HOME, () => r.tap(B.SAVE));
    o.mangle = { changed: after !== before, undone: notes(0).map(n => n.clock + ':' + n.pitch).join() === before };
    o.page = r.page();
    return o;
  });
  savePng(root, 'phase4b-step-glitch', out.step, 'png');
  savePng(root, 'phase4b-stutter', out.stutter, 'png');
  savePng(root, 'phase4b-fx-lane', out, 'lanePng');
  await page.close();
  const t = checker('4b'), { step: st, stutter: su, lane: la } = out;
  t.eq('two notes entered with glitch', st.entered.length, 2);
  t.eq('their repeats: 4 and 3', st.repeats, [4, 3]);
  t.eq('the repeats play on their exact blocks', st.got, st.want);
  t.eq('the stutter starts on a slice grid line', su.at % 240, 0);
  t.over('the stutter ends after it starts', su.to, su.at);
  t.yes('every track repeats the slice, drifting', su.replays.ok, su.replays);
  t.over('repeats checked', su.replays.expected, 20);
  t.yes('playing rejoins the song in time', su.rejoined.ok, su.rejoined);
  t.over('notes checked after the rejoin', su.rejoined.expected, 20);
  t.yes('balanced through the stutter', su.balanced);
  t.eq('one stutter recorded on the FX lane', la.fx.length, 1);
  t.eq('its slice is a 1/8', la.fx[0]?.slice, 1);
  t.eq('its drift is +2', la.fx[0]?.drift, 2);
  t.eq('it starts where the held one did', la.fx[0]?.clock, su.at);
  t.yes('the song plays back as the take sounded', la.same);
  t.over('events compared', la.compared, 100);
  t.yes('SHIFT+F#3 scrambles the selection', out.mangle.changed);
  t.yes('undo puts it back', out.mangle.undone);
  return { phase: '4b', checks: t.list, out };
}
