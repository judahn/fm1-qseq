// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
// Phase 7b of the preview check: clips in the song. Three clips made in the
// grid, then REC on its SONG row records a performance (0.4.0): launches at
// the bar quantize and stops become blocks on tracks 1 and 2, and the tracks
// silent all along get stop blocks, one undo step, and playing the song back
// plays what was played, note for note in exact blocks, the silent tracks
// silent. The Expand job turns the blocks into notes that play the same
// again. A clip copied in the grid pastes into the song view as a block,
// Duplicate extends it. With a clip open, the grid's launched clips play on
// and the song stays quiet, and REC while playing records into the clip from
// its next pass. In the workbench,
// a song's clips are listed and one is copied into another song, and MIDI
// export expands the blocks. Saves docs/phase7b-*.png.
import { simPage, savePng, checker, shot } from './preview_lib.mjs';
import { pathToFileURL } from 'node:url';
import { createRequire } from 'node:module';
import fs from 'node:fs';
import path from 'node:path';

const require = createRequire(import.meta.url);
export async function phase7b(browser, root) {
  const page = await simPage(browser, root);
  const out = await page.evaluate(async () => {
    const r = previewRig(), { B, c } = r;
    const { tap, key, held, turn, render, info, png } = r, pageNow = r.page;
    const cell = (t, row) => t * 16 + row, part = (t, row) => 8 + cell(t, row);
    const events = p => Array.from({ length: info(0, p) }, (_, i) =>
      ({ clock: info(1, p, i), a: info(2, p, i), length: info(3, p, i) + info(18, p, i) * 65536, kind: info(30, p, i) }));
    const blocks = t => events(t).filter(e => e.kind === 3 && e.a & 0x80).map(e => [e.clock, e.length, e.a === 0xff ? 'stop' : e.a & 15]);
    const perf = () => info(47) & 255, armed = () => (info(47) >> 8) & 255, taking = () => info(47) >> 16;
    const undos = () => info(29, 0, 9);
    const on = (list, tracks) => list.filter(h => !h.kind && h.velocity && tracks.includes(h.track));
    const o = {};
    render(32);
    o.loopWasOn = info(16) & 1;

    // 1. Three clips in the grid: track 1 row 1 quarters, track 2 row 1
    // halves, track 1 row 2 quarters higher up.
    tap(B.SEQ); tap(B.SEQ);
    o.gridPage = pageNow();
    const fill = (length, keys) => { tap(B.SCL); r.step(); held(B.FX, () => key(length)); for (const k of keys) key(k); tap(B.SEQ); };
    fill(4, [0, 4, 7, 11]);                         // SHIFT+A3: quarters
    turn(0, 1); fill(2, [0, 7]);                    // SHIFT+G3: halves
    turn(0, -1); turn(2, 1); fill(4, [12, 14, 16, 17]);
    o.clips = [info(39, 0, cell(0, 0)), info(39, 0, cell(1, 0)), info(39, 0, cell(0, 1))];
    turn(2, -1);

    // 2. A performance: REC on the SONG row arms it, PLAY starts it, then
    // launches from row 1, and the SONG row's F3 stops track 1.
    const before = undos();
    turn(2, -1);                                    // the SONG row
    tap(B.REC);
    o.armed = perf();
    turn(2, 1);
    c.sim_song_capture(1);
    tap(B.PLAY);
    o.taking = perf();
    const start = info(9), at0 = info(10), launches = [];
    const launch = (k, t) => { key(k); launches.push(info(45, t) - (info(43) - info(8))); render(900); };
    render(300);
    launch(0, 0);                                   // track 1, row 1
    launch(2, 1);                                   // track 2, row 1
    turn(2, 1); launch(0, 0);                       // track 1, row 2
    launch(2, 1);                                   // track 2, row 2: empty, a stop
    o.takePng = png();
    turn(2, -2); launch(0, 0);                      // the SONG row's F3: track 1 stops
    tap(B.PLAY);                                    // PLAY ends the take, and playing goes on
    o.afterStop = [perf(), info(7)];
    tap(B.PLAY);
    const live = on(r.heard(), [0, 1]).map(h => [h.block - start, h.track, h.pitch]);
    o.launches = launches;
    o.blocks = [blocks(0), blocks(1)];
    o.others = [2, 3, 4, 5, 6, 7].map(blocks);
    o.undoSteps = undos() - before;
    o.at0 = at0;

    // 3. Played back from the same place, the loop off: the same notes.
    tap(B.SEQ);
    o.songPage = pageNow();
    if (info(16) & 1) tap(B.ENV);
    o.blocksPng = png();
    const replay = () => {
      held(B.FX, () => tap(B.PLAY));                // back to the top
      c.sim_song_capture(1);
      tap(B.PLAY);
      const from = info(9);
      render(live.length ? live[live.length - 1][0] + 40 : 4000);
      tap(B.PLAY);
      const got = on(r.heard(), [0, 1]).map(h => [h.block - from, h.track, h.pitch]);
      const quiet = on(r.heard(), [2, 3, 4, 5, 6, 7]).filter(h => h.block - from <= live[live.length - 1][0]).length;
      return { start: info(10), got: got.filter(g => g[0] <= live[live.length - 1][0]), quiet };
    };
    const back = replay();
    o.replay = { start: back.start, n: back.got.length, want: live.length, same: r.same(back.got, live), quiet: back.quiet };

    // 4. Expand, from the edit jobs: every block on every track, the whole song.
    held(B.FX, () => tap(B.EDIT));
    o.jobPage = pageNow();
    turn(2, 40);                                    // the last job
    held(B.LFO, () => key(14));                     // all tracks
    turn(3, 1);                                     // the whole song
    o.jobKnobs = r.knobs();
    tap(B.YES); render(2); tap(B.YES);
    o.expanded = [blocks(0).length, blocks(1).length];
    o.expandUndo = undos();
    tap(B.HOME);
    const again = replay();
    o.afterExpand = { n: again.got.length, same: r.same(again.got, live) };
    o.expandedPng = png();
    c.sim_control(0, B.HOME, 1); tap(B.SAVE); c.sim_control(0, B.HOME, 0); // undo: the blocks back
    o.undone = [blocks(0).length, blocks(1).length];

    // 5. By hand: track 1 row 1's clip copied in the grid, pasted over in the
    // song view at bar 9 as a block, then Duplicate extends it.
    tap(B.SEQ);
    turn(2, 1);                                     // row 1
    held(B.FX, () => key(0));                       // copy, at row 1 of track 1
    tap(B.SEQ);
    held(B.FX, () => tap(B.PLAY));
    turn(2, 8);                                     // the playhead to bar 9
    o.pasteAt = info(8);
    held(B.FX, () => key(6));                       // paste over
    o.placed = blocks(0).filter(b => b[0] >= o.pasteAt);
    held(B.FX, () => key(7));                       // duplicate
    o.extended = blocks(0).filter(b => b[0] >= o.pasteAt);
    o.bars = info(29, 0, 13);

    // 6. A clip open while playing: track 1's clip, launched, plays on
    // around it, and the song stays quiet. REC records into it from its
    // next pass, the rest playing on, and REC again stops recording while
    // playing goes on.
    held(B.FX, () => tap(B.PLAY));
    tap(B.SEQ);
    c.sim_song_capture(1);
    key(0);                                         // track 1's row 1 clip: playing starts
    turn(0, 1);                                     // track 2, row 1
    render(200);
    tap(B.SCL);
    o.open = info(42) & 255;
    render(400);
    const clipLength = info(39, 0, cell(1, 0));
    o.clipClock = info(8) < clipLength;
    const opened = r.heard().length;
    tap(B.REC);
    o.clipArmed = armed();
    let guard = 0;
    while (!taking() && guard++ < 2000) render(1);
    o.recording = taking();
    o.passTop = info(8) < 200;
    const notesBefore = info(0, 1);
    for (const k of [4, 9]) { key(k); render(100); }
    render(400);
    o.clipRecPng = png();
    tap(B.REC);
    o.stillPlaying = info(7);
    o.recordedTake = info(0, 1) - notesBefore;
    tap(B.REC);                                     // again, and PLAY ends that one, playing on
    guard = 0;
    while (!taking() && guard++ < 2000) render(1);
    o.clipAgain = taking();
    tap(B.PLAY);
    o.playEnds = [taking(), armed(), info(7)];
    render(300);
    const around = r.heard().slice(opened);
    o.launchedPlayed = on(around, [0]).length;
    o.songQuiet = on(around, [2, 3, 4, 5, 6, 7]).filter(h => h.track !== 7 || h.pitch !== 37).length; /* the metronome's rim aside */
    tap(B.PLAY);
    tap(B.SEQ);
    o.closed = info(42) & 255;
    o.clipNotes = info(40, 0, cell(1, 0));

    // 7. The song as the workbench would read it: saved, then its payload.
    const link = FM1SongWorkbench.link.simulator(async frames => r.core.command({ type: 'protocol', frames }));
    const id = await link.saveWorking(true);
    o.payload = Array.from((await link.readSong(id)).bytes);
    return o;
  });
  for (const [name, key] of [['take', 'takePng'], ['blocks', 'blocksPng'], ['expanded', 'expandedPng'], ['clip-rec', 'clipRecPng']])
    savePng(root, 'phase7b-' + name, out, key);
  await page.close();

  // 8. The workbench on the page's own simulator: the song uploaded twice,
  // its clips listed, one copied into the other song, and MIDI export.
  for (const name of ['bytes', 'song_file', 'song_clips', 'midi_file']) require(path.join(root, 'web/workbench', name + '.js'));
  const { songFile: S, midiFile: M } = globalThis.FM1SongWorkbench;
  const context = await browser.newContext({ acceptDownloads: true, viewport: { width: 1180, height: 1000 } });
  const wb = await context.newPage();
  wb.on('dialog', dialog => dialog.accept());
  await wb.goto(pathToFileURL(path.join(root, 'build/preview/index.html')).href);
  await wb.waitForFunction(() => globalThis.FM1SimProtocol && document.getElementById('sw-section'));
  await wb.click('#mode-library');
  const ids = await wb.evaluate(async payload => {
    const link = FM1SongWorkbench.link.simulator(FM1SimProtocol), bytes = Uint8Array.from(payload);
    return [await link.writeSong(0, bytes), await link.writeSong(0, bytes)];
  }, out.payload);
  const status = () => wb.textContent('#sw-status');
  const act = async selector => {
    await wb.evaluate(() => { document.getElementById('sw-status').textContent = ''; });
    await wb.click(selector);
    await wb.waitForFunction(() => document.getElementById('sw-status').textContent !== '');
    return status();
  };
  for (let i = 0; i < 40 && !/Up to date/.test(await act('#sw-refresh')); i++) await wb.waitForTimeout(250);
  await wb.selectOption('#sw-songs', String(ids[0]));
  const [file] = await Promise.all([wb.waitForEvent('download'), act('#sw-song-download')]);
  const song = S.decode(S.unwrap(new Uint8Array(fs.readFileSync(await file.path()))));
  const w = {};
  w.version = song.version;
  w.list = await wb.$$eval('#sw-clips option', options => options.map(o => o.textContent));
  await wb.selectOption('#sw-clips', { value: '1' });      // track 1, row 2
  w.target = [await wb.inputValue('#sw-clip-track'), await wb.inputValue('#sw-clip-row')];
  await wb.selectOption('#sw-clip-track', { value: '3' });
  await wb.selectOption('#sw-clip-row', { value: '5' });
  await wb.selectOption('#sw-songs', String(ids[1]));
  w.copied = await act('#sw-clip-copy');
  w.other = await wb.evaluate(async id => {
    const ns = FM1SongWorkbench, link = ns.link.simulator(FM1SimProtocol), s = ns.songFile.decode((await link.readSong(id)).bytes);
    const clip = s.clips.find(c => c.cell === 3 * 16 + 5);
    return clip ? { length: clip.length, pitches: clip.events.map(e => e.a) } : null;
  }, ids[1]);
  await wb.selectOption('#sw-clips', { value: '1' });
  await shot(wb.locator('#sw-songs-part'), path.join(root, 'docs/phase7b-workbench.png'));
  const [mid] = await Promise.all([wb.waitForEvent('download'), act('#sw-current-midi')]);
  w.midiStatus = await status();
  const midi = M.read(new Uint8Array(fs.readFileSync(await mid.path())));
  const blocksIn = song.tracks[0].filter(S.isBlock);
  const first = blocksIn.find(b => (b.a & 15) === 1);
  w.midiInBlock = first ? midi.tracks[1].filter(e => e.status === 0x90 && e.data[1] && e.tick >= S.clockOf(first) &&
    e.tick < S.clockOf(first) + S.blockLength(first)).map(e => e.data[0]) : [];
  await context.close();

  const t = checker('7b'), q = out, clip3 = (song.clips.find(c => c.cell === 1) || { events: [] }).events.map(e => e.a);
  t.eq('the grid', q.gridPage, 27);
  t.eq('three one-bar clips made', q.clips, [1920, 1920, 1920]);
  t.eq('REC on the grid\'s SONG row arms a performance', q.armed, 1);
  t.eq('PLAY starts the take', q.taking, 2);
  t.yes('each launch waits for a bar line', q.launches.every(l => l % 1920 === 0), q.launches);
  t.eq('PLAY again ends the take, and playing goes on', q.afterStop, [0, 1]);
  const [l1, l2, l3, l4, l5] = q.launches;
  const ended = q.blocks[1].length ? q.blocks[1][q.blocks[1].length - 1][0] + q.blocks[1][q.blocks[1].length - 1][1] : 0;
  t.eq('track 1\'s blocks: silent from the take\'s start, row 1, row 2, then the stop to the end', q.blocks[0],
    [[q.at0, l1 - q.at0, 'stop'], [l1, l3 - l1, 0], [l3, l5 - l3, 1], [l5, ended - l5, 'stop']]);
  t.eq('track 2\'s blocks: silent, row 1, then the stop until the take ended', q.blocks[1].map(b => [b[0], b[2]]),
    [[q.at0, 'stop'], [l2, 0], [l4, 'stop']]);
  t.over('the take ended after the last launch', ended, l5);
  t.eq('the tracks silent all along: a stop block each, over the whole take', q.others,
    [2, 3, 4, 5, 6, 7].map(() => [[q.at0, ended - q.at0, 'stop']]));
  t.eq('the take is one undo step', q.undoSteps, 1);
  t.eq('SEQ: the song view', q.songPage, 19);
  t.eq('played back from the same place', q.replay.start, q.at0);
  t.atLeast('notes on tracks 1 and 2 in the take', q.replay.want, 20);
  t.yes('played back, tracks 1 and 2 play the take note for note, in exact blocks', q.replay.same, q.replay);
  t.eq('and the tracks silent in the take are silent', q.replay.quiet, 0);
  t.eq('SHIFT+EDIT: the edit jobs', q.jobPage, 26);
  t.match('the last job is Expand, over the whole song', q.jobKnobs.join(' | '), /EXPAND CLIPS.*WHOLE SONG/);
  t.eq('Expand leaves no blocks', q.expanded, [0, 0]);
  t.yes('expanded, the same notes in the same blocks', q.afterExpand.same, q.afterExpand);
  t.eq('undo puts the blocks back', q.undone, [4, 3]);
  t.eq('a clip copied in the grid pastes as a block at the playhead, its length long', q.placed, [[q.pasteAt, 1920, 0]]);
  t.eq('Duplicate extends it', q.extended, [[q.pasteAt, 3840, 0]]);
  t.eq('opening a clip while playing', q.open, 8 + 16);
  t.yes('the editor\'s clock is the clip\'s', q.clipClock);
  t.eq('REC while playing in the clip: armed for its next pass', q.clipArmed, 1);
  t.eq('recording from the next pass', q.recording, 1);
  t.yes('from the top of the pass', q.passTop);
  t.eq('two notes recorded into the clip', q.recordedTake, 2);
  t.eq('REC again: still playing', q.stillPlaying, 1);
  t.eq('REC once more records into the clip again', q.clipAgain, 1);
  t.eq('PLAY ends that take, and playing goes on', q.playEnds, [0, 0, 1]);
  t.atLeast('track 1\'s launched clip played on throughout', q.launchedPlayed, 4);
  t.eq('the song stayed quiet with the clip open: the editor plays with the grid', q.songQuiet, 0);
  t.eq('SEQ closes the clip', q.closed, 0);
  t.eq('the clip keeps its notes and the new ones', q.clipNotes, 4);
  t.eq('the workbench reads it at the current version', w.version, S.VERSION);
  t.match('the clips listed', w.list.join(' | '), /T1 row 1, 1 bar, 4 events \| T1 row 2, 1 bar, 4 events \| T2 row 1, 1 bar, 4 events/);
  t.eq('picking a clip picks its own cell to copy into', w.target, ['0', '1']);
  t.match('copied into the other song', w.copied, /Clip copied into .*track 4 row 6/);
  t.eq('the other song has it there', w.other, { length: 1920, pitches: clip3 });
  t.match('MIDI export expands the blocks', w.midiStatus, /clip blocks expanded/);
  t.eq('the second clip\'s notes inside its block in the MIDI file', w.midiInBlock.slice(0, 4), clip3);
  return { phase: '7b', checks: t.list, out: { ...out, payload: out.payload.length, workbench: w } };
}
