/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* The Songs part of the "Songs and kits" section (phase 5b): the device's
 * songs, the open song file, factory templates, and MIDI export and import. */
(function (root) {
  'use strict';
  if (typeof document === 'undefined') return;
  const ns = root.FM1SongWorkbench, ui = ns.ui, $ = ui.$, S = ns.songFile, SM = ns.songMidi;
  const html = `<h3>Songs</h3>
<label>On the device <select id="sw-songs" size="6"></select></label>
<div><button id="sw-song-download">Download song file</button><button id="sw-song-midi">Export MIDI</button>
<button id="sw-song-load">Load on the device</button><button id="sw-song-delete">Delete</button>
<button id="sw-song-save">Save the working song as new</button></div>
<div class="field-row"><label>Open a song file or MIDI file <input id="sw-song-file" type="file" accept=".fm1song,.mid,.midi"></label>
<label>New from a factory sound set <select id="sw-templates"><option value="">Choose...</option></select></label></div>
<p id="sw-current">No song file open.</p>
<div><button id="sw-current-upload">Upload as a new song</button><button id="sw-current-replace">Replace the selected song</button>
<button id="sw-current-save">Save song file</button><button id="sw-current-midi">Export MIDI</button></div>
<div id="sw-import" hidden><h4>MIDI import</h4>
<p class="quiet-note">Pick a part of the file for each FM-1 track. Sounds, kit and settings come from the open song file
(or the first factory sound set). Controllers a track's engine doesn't take are left out.</p>
<table class="sw-map" id="sw-map"></table><pre id="sw-import-report"></pre>
<button id="sw-import-go">Import into a new song file</button></div>`;
  let current = null, midi = null, songs = [];
  const selected = () => songs.find(s => String(s.id) === $('sw-songs').value);
  const need = () => { const s = selected(); if (!s) throw Error('Select a song on the device'); return s; };

  function open(payload, from) {
    const song = S.decode(payload);
    current = { payload, song };
    const notes = song.tracks.flat().filter(e => S.kindOf(e) === S.KIND.NOTE).length;
    $('sw-current').textContent = 'Open: ' + S.name(song) + ' (' + from + '), ' + payload.length + ' bytes, ' + S.eventCount(song) +
      ' events (' + notes + ' notes), ' + song.bars + ' bars.';
    ui.enable();
  }
  function midiFile(song, name) {
    const out = SM.exportMidi(song), s = out.skipped, left = [];
    if (s.glitched) left.push(s.glitched + ' glitch attributes');
    if (s.fx) left.push(s.fx + ' FX lane events');
    if (s.humanize) left.push('humanize');
    if (s.clips) left.push(s.clips + (s.clips > 1 ? ' clips' : ' clip'));
    ui.save(out.bytes, ui.slug(name) + '.mid');
    ui.status('MIDI file saved.' + (left.length ? ' Left out (they stay in the song file): ' + left.join(', ') + '.' : ''));
  }
  async function base() {
    return current ? current.song : S.decode(await ns.templates.build(0));
  }
  async function plan() {
    const mapping = [...$('sw-map').querySelectorAll('select')].map(s => (s.value === '' ? null : Number(s.value)));
    const result = SM.importMidi(midi.parsed, await base(), mapping, midi.name);
    const r = result.report, d = r.dropped;
    $('sw-import-report').textContent = [
      (r.ok ? 'Fits: ' : 'Does not fit: ') + r.counts.events + ' of ' + r.limits.events + ' events, ' + r.counts.tempos + ' tempo, ' +
        r.counts.meters + ' time signature and ' + r.counts.markers + ' marker changes, ' + r.counts.bars + ' bars.',
      ...r.tracks.filter(t => t.source !== null).map(t => 'Track ' + t.track + ': ' + t.notes + ' notes, ' + (t.events - t.notes) + ' other events'),
      ...r.problems,
      d.controllers ? d.controllers + ' controllers the engines do not take, left out' : '',
      d.programs ? d.programs + ' program changes out of range, left out' : '',
      d.clamped ? d.clamped + ' notes or tempos clamped to range' : '',
      d.moved ? d.moved + ' time signatures moved to the next bar line' : ''].filter(Boolean).join('\n');
    $('sw-import-go').disabled = !r.ok || ui.busy();
    return result;
  }
  function showImport() {
    const list = SM.sources(midi.parsed), mapping = SM.defaultMapping(list), table = $('sw-map');
    table.replaceChildren();
    for (let t = 0; t < 8; t++) {
      const row = table.insertRow(), select = document.createElement('select');
      row.insertCell().textContent = 'Track ' + (t + 1);
      select.append(ui.option('', 'Nothing'));
      for (const s of list)
        select.append(ui.option(s.id, 'File track ' + (s.track + 1) + (s.name ? ' "' + s.name + '"' : '') + ', channel ' + (s.channel + 1) + ', ' + s.notes + ' notes'));
      select.value = mapping[t] === null ? '' : String(mapping[t]);
      select.onchange = ui.act(plan);
      row.insertCell().append(select);
    }
    $('sw-import').hidden = false;
    return plan();
  }

  const part = {
    mount() {
      $('sw-songs-part').innerHTML = html;
      $('sw-song-download').onclick = ui.act(async () => {
        const s = need(), read = await ui.run(link => link.readSong(s.id, ui.progress));
        ui.save(S.wrap(read.bytes), ui.slug(s.name) + '.fm1song');
        open(read.bytes, 'downloaded');
        ui.status('Song file saved, CRC checked.');
      });
      $('sw-song-midi').onclick = ui.act(async () => {
        const s = need(), read = await ui.run(link => link.readSong(s.id, ui.progress));
        midiFile(S.decode(read.bytes), s.name);
      });
      $('sw-song-load').onclick = ui.act(async () => {
        const s = need();
        await ui.run(async link => { await link.loadSong(s.id); await ui.refresh(link); });
        ui.status('Loaded ' + s.name + ' on the device.');
      });
      $('sw-song-delete').onclick = ui.act(async () => {
        const s = need();
        if (!confirm('Delete ' + s.name + ' from the device?')) return;
        await ui.run(async link => { await link.deleteSong(s.id); await ui.refresh(link); });
        ui.status('Deleted.');
      });
      $('sw-song-save').onclick = ui.act(async () => {
        const id = await ui.run(async link => { const id = await link.saveWorking(true); await ui.refresh(link); return id; });
        $('sw-songs').value = String(id);
        ui.status('The working song is saved as song ' + id + '.');
      });
      $('sw-song-file').onchange = ui.act(async () => {
        const file = await ui.file($('sw-song-file'));
        if (/\.midi?$/i.test(file.name)) {
          midi = { name: file.name.replace(/\.midi?$/i, ''), parsed: ns.midiFile.read(file.bytes) };
          await showImport();
          ui.status('MIDI file read. Check the track mapping, then import.');
        } else { open(S.unwrap(file.bytes), file.name); ui.status('Song file opened, CRC checked.'); }
      });
      $('sw-templates').onfocus = ui.act(async () => {
        if ($('sw-templates').options.length > 1) return;
        (await ns.templates.names()).forEach((name, i) => $('sw-templates').append(ui.option(i, name)));
      });
      $('sw-templates').onchange = ui.act(async () => {
        if ($('sw-templates').value === '') return;
        const index = Number($('sw-templates').value);
        open(await ns.templates.build(index), 'factory sound set');
        ui.status('Template ready: upload it, or save it as a file.');
      });
      $('sw-current-upload').onclick = ui.act(async () => {
        const id = await ui.run(async link => { const id = await link.writeSong(0, current.payload, ui.progress); await ui.refresh(link); return id; });
        $('sw-songs').value = String(id);
        ui.status('Uploaded as song ' + id + '.');
      });
      $('sw-current-replace').onclick = ui.act(async () => {
        const s = need();
        if (!confirm('Replace ' + s.name + ' on the device with ' + S.name(current.song) + '?')) return;
        await ui.run(async link => { await link.writeSong(s.id, current.payload, ui.progress); await ui.refresh(link); });
        $('sw-songs').value = String(s.id);
        ui.status('Song ' + s.id + ' replaced.');
      });
      $('sw-current-save').onclick = ui.act(async () => {
        ui.save(S.wrap(current.payload), ui.slug(S.name(current.song)) + '.fm1song');
        ui.status('Song file saved.');
      });
      $('sw-current-midi').onclick = ui.act(async () => midiFile(current.song, S.name(current.song)));
      $('sw-import-go').onclick = ui.act(async () => {
        const { song } = await plan();
        if (!song) return;
        open(S.encode(song), 'imported from MIDI');
        ui.status('Imported. Upload it, or save it as a song file.');
      });
    },
    async refresh(link) {
      const keep = $('sw-songs').value;
      songs = await link.songs();
      $('sw-songs').replaceChildren(...songs.map(s => ui.option(s.id, String(s.id).padStart(2, '0') + '  ' + s.name + '  (' + s.bytes + ' bytes)')));
      if (songs.some(s => String(s.id) === keep)) $('sw-songs').value = keep;
    },
    enable(busy) {
      for (const id of ['sw-song-download', 'sw-song-midi', 'sw-song-load', 'sw-song-delete', 'sw-song-save']) $(id).disabled = busy;
      for (const id of ['sw-current-upload', 'sw-current-replace', 'sw-current-save', 'sw-current-midi']) $(id).disabled = busy || !current;
      $('sw-import-go').disabled = busy || !midi;
    },
    state: () => ({ current, midi })
  };
  ui.parts.push(part);
  ns.songsPart = part;
})(typeof globalThis !== 'undefined' ? globalThis : this);
