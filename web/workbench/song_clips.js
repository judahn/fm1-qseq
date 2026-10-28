/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Clips in song files (phase 7b, design 8f): a song's clips listed, a clip
 * copied into another song, and the song with every clip block expanded
 * into plain events, as the device's Expand job does (app/song/blocks.c),
 * for MIDI export. */
(function (root) {
  'use strict';
  const ns = root.FM1SongWorkbench = root.FM1SongWorkbench || {};
  const S = ns.songFile, { KIND, LIMITS } = S;
  const byClock = (a, b) => S.clockOf(a) - S.clockOf(b);
  const lane = e => (S.kindOf(e) === KIND.PROGRAM ? 128 : e.a);
  const isLane = e => S.kindOf(e) === KIND.CONTROL || S.kindOf(e) === KIND.PROGRAM;

  /* Every clip: its cell, track and row (from 0), length, events and row name. */
  function list(song) {
    const names = new Map((song.rows || []).map(r => [r.row, ns.bytes.text(r.name)]));
    return (song.clips || []).map(c => ({ cell: c.cell, track: c.cell >> 4, row: c.cell & 15, length: c.length,
      events: c.events.length, name: names.get(c.cell & 15) || '' }));
  }
  /* A copy of `to` with clip `cell` of `from` in its cell `into`, replacing
   * what was there. Glitch attributes move to the other song's table, and a
   * note whose attributes don't fit there plays plain. */
  function copy(from, cell, to, into) {
    const clip = (from.clips || []).find(c => c.cell === cell);
    if (!clip) throw Error('No clip in that cell');
    if (!(into >= 0 && into < LIMITS.cells)) throw Error('No such cell');
    const glitch = to.glitch.map(g => ({ ...g })), words = new Map(from.glitch.map(g => [g.index, g.word]));
    let dropped = 0;
    const events = clip.events.filter(e => S.kindOf(e) !== KIND.FX).map(e => {
      const g = S.glitchOf(e);
      if (!g || S.kindOf(e) !== KIND.NOTE) return { ...e };
      const word = words.get(g);
      let entry = glitch.find(x => x.word === word);
      if (!entry && word !== undefined && glitch.length < 31) {
        let index = 1;
        while (glitch.some(x => x.index === index)) index++;
        entry = { index, word };
        glitch.push(entry);
      }
      if (!entry) dropped++;
      return { ...e, at: ((e.at & ~(31 << 26)) | (entry ? entry.index << 26 : 0)) >>> 0 };
    });
    const clips = (to.clips || []).filter(c => c.cell !== into).concat([{ cell: into, length: clip.length, events }]);
    clips.sort((a, b) => a.cell - b.cell);
    const song = { ...to, version: Math.max(to.version || 0, 2), glitch: glitch.sort((a, b) => a.index - b.index), clips };
    if (S.eventCount(song) > LIMITS.events) throw Error('The other song has no room for the clip');
    return { song, dropped };
  }

  /* Lanes as of `clock`, as the device chases them: the last change before
   * it, or a controller's first change after it. */
  function chase(events, clock) {
    const value = new Map(), seen = new Set();
    for (const e of events) {
      if (!isLane(e)) continue;
      const l = lane(e), at = S.clockOf(e);
      if (at < clock) { value.set(l, S.kindOf(e) === KIND.PROGRAM ? e.a : e.b); seen.add(l); }
      else if (!seen.has(l)) { seen.add(l); if (at > clock && l < 128) value.set(l, e.b); }
    }
    return value;
  }
  /* The song with every block a track plays turned into the events it
   * plays: the clip pass after pass from the block's start, cut off at its
   * end, the track's own events inside it gone, notes sounding into it
   * ending where it starts, and the controllers set at its edges. */
  function expand(song) {
    const tracks = song.tracks.map((events, t) => {
      let list = events.slice();
      for (;;) {
        const i = list.findIndex(S.isBlock);
        if (i < 0) return list;
        const b = list[i], from = S.clockOf(b), next = list.slice(i + 1).find(S.isBlock);
        const to = Math.min(from + S.blockLength(b), next ? S.clockOf(next) : Infinity);
        const clip = b.a === 0xff ? null : (song.clips || []).find(c => c.cell === t * 16 + (b.a & 15));
        const length = clip ? clip.length : 0, parts = clip ? clip.events.filter(e => S.clockOf(e) < length) : [];
        const edges = [];
        if (clip) for (const [l, v] of chase(clip.events, 0)) if (l < 128) edges.push(S.event(from, KIND.CONTROL, l, v, 0));
        for (const [l, v] of chase(list, to))
          if (l < 128 && !list.some(e => isLane(e) && lane(e) === l && S.clockOf(e) === to)) edges.push(S.event(to, KIND.CONTROL, l, v, 0));
        list = list.filter((e, j) => j !== i && (S.kindOf(e) === KIND.FX || S.clockOf(e) < from || S.clockOf(e) >= to)).map(e =>
          S.kindOf(e) === KIND.NOTE && S.clockOf(e) < from && S.clockOf(e) + e.length > from ? { ...e, length: from - S.clockOf(e) } : e);
        for (let pass = from; length && pass < to; pass += length)
          for (const e of parts) {
            const at = pass + S.clockOf(e);
            if (at >= to) break;
            const x = { ...e, at: ((e.at & ~0xffffff) | at) >>> 0 };
            if (S.kindOf(x) === KIND.NOTE && at + x.length > to) x.length = to - at;
            list.push(x);
          }
        list = list.concat(edges).sort(byClock);
      }
    });
    return { ...song, tracks };
  }
  const blocks = song => song.tracks.reduce((n, events) => n + events.filter(S.isBlock).length, 0);

  ns.songClips = { list, copy, expand, blocks };
  if (typeof module === 'object' && module.exports) module.exports = ns.songClips;
})(typeof globalThis !== 'undefined' ? globalThis : this);
