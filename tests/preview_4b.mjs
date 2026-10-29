// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// Phase 4b of the preview check: glitch. Repeats entered in step entry on a
// synth track and played on exact blocks, ARP in the song view as the
// arpeggiator again (0.3.0, the stutter and the FX lane gone), and mangle
// with undo. Saves docs/phase4b-*.png.
import { simPage, savePng, checker } from './preview_lib.mjs';

export async function phase4b(browser, root) {
  const page = await simPage(browser, root);
  const out = await page.evaluate(() => {
    const r = previewRig(), { B, c, same } = r, info = r.info;
    const heard = () => r.heard().filter(h => !h.kind);
    const notes = t => Array.from({ length: info(0, t) }, (_, i) => i).filter(i => info(30, t, i) === 0)
      .map(i => ({ clock: info(1, t, i), pitch: info(2, t, i), length: info(3, t, i), glitch: info(35, t, i) }));
    const o = { step: {}, arp: {} };
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

    // 2. ARP in the song view is the arpeggiator's (0.3.0): a tap turns it on
    // for the track and another off, the song playing on as it was. A hold
    // opens its settings, and HOME there comes back to the song view.
    const ar = o.arp, track = 1;
    r.turn(0, track);                              // track 2
    ar.before = info(56, track);
    r.tap(B.ARP);
    ar.on = info(56, track);
    ar.page = r.page();
    r.tap(B.ARP);
    ar.off = info(56, track);
    c.sim_control(0, B.ARP, 1);
    r.render(250);                                 // past the 600 ms hold
    ar.held = r.page();
    c.sim_control(0, B.ARP, 0);
    r.render(4);
    ar.png = r.png();
    r.tap(B.HOME);
    ar.back = r.page();
    ar.unchanged = info(56, track);
    r.turn(0, -track);
    // 3. Mangle from the song view: SHIFT+F#3 scrambles bars 3 to 6 of track
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
  savePng(root, 'phase4b-arp', out.arp, 'png');
  await page.close();
  const t = checker('4b'), { step: st, arp: ar } = out;
  t.eq('two notes entered with glitch', st.entered.length, 2);
  t.eq('their repeats: 4 and 3', st.repeats, [4, 3]);
  t.eq('the repeats play on their exact blocks', st.got, st.want);
  t.eq('ARP tapped in the song view: the track\'s arpeggiator on', [ar.before, ar.on], [0, 1]);
  t.eq('and the song view stays', ar.page, 19);
  t.eq('tapped again: off', ar.off, 0);
  t.eq('ARP held: the arpeggiator\'s settings', ar.held, 17);
  t.eq('HOME there: back to the song view', ar.back, 19);
  t.eq('the hold left the arpeggiator as it was', ar.unchanged, 0);
  t.yes('SHIFT+F#3 scrambles the selection', out.mangle.changed);
  t.yes('undo puts it back', out.mangle.undone);
  return { phase: '4b', checks: t.list, out };
}
