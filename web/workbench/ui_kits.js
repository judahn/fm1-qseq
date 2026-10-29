/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* The Kits part of the "Songs and kits" section (phase 5b): a kit built
 * from up to 8 WAVs with prep per pad, kit files, the device's kit library,
 * FM1 Open's built-in sounds sent as a kit, and the factory DX7 voices sent
 * to the DX7 library (voices.js). Encoding to SEA5 runs in a private copy
 * of the simulator. */
(function (root) {
  'use strict';
  /* The page declares the simulator's bytes with const, so not on globalThis. */
  const simulator = () => (typeof FM1_SIMULATOR !== 'undefined' ? FM1_SIMULATOR : root.FM1_SIMULATOR);
  if (typeof document === 'undefined') return;
  const ns = root.FM1SongWorkbench, ui = ns.ui, $ = ui.$, KF = ns.kitFile, W = ns.wav;
  const FIELDS = [['start', 'Start s', 0.001], ['end', 'End s', 0.001], ['fadeIn', 'Fade in', 1], ['fadeOut', 'Fade out', 1],
    ['gain', 'Gain dB', 0.5], ['bits', 'Bits', 1], ['hold', 'Rate Hz', 100]];
  const html = `<h3>Kits</h3>
<label>On the device <select id="sw-kits" size="5"></select></label>
<div><button id="sw-kit-use">Use on the device</button><button id="sw-kit-download">Download kit file</button>
<button id="sw-kit-delete">Delete</button><button id="sw-kit-builtin">Send the built-in kit</button>
<button id="sw-voices-factory">Send the factory DX7 voices</button></div>
<h4>Build a kit</h4>
<div class="field-row"><label>Kit name <input id="sw-kit-name" type="text" maxlength="15" value="NEW KIT"></label>
<label>Open a kit file <input id="sw-kit-file" type="file" accept=".fm1kit"></label></div>
<p class="quiet-note">Fades are in milliseconds. Bits below 16 and a rate below 32000 Hz are the lo-fi settings.</p>
<div class="sw-wide"><table class="sw-pads"><thead><tr><th>Pad</th><th>WAV</th><th>Name</th>${FIELDS.map(f => '<th>' + f[1] + '</th>').join('')}
<th>Norm.</th><th>Rev.</th><th>Size</th><th></th></tr></thead><tbody id="sw-pads"></tbody></table></div>
<div><button id="sw-kit-build">Prepare pads</button><button id="sw-kit-save">Save kit file</button>
<button id="sw-kit-write" class="primary">Write kit to the device</button></div><p id="sw-kit-summary" class="quiet-note"></p>`;
  const pads = Array.from({ length: 8 }, () => ({ source: null, encoded: null, prepared: null }));
  let params = null, kits = [], encoder = null, built = null;
  const row = pad => $('sw-pads').rows[pad];
  const input = (pad, key) => row(pad).querySelector('[data-k="' + key + '"]');
  const selectedKit = () => { const k = kits.find(k => String(k.id) === $('sw-kits').value); if (!k) throw Error('Select a kit on the device'); return k; };

  async function encode(data) {
    if (!encoder) {
      const wasm = await root.FM1Web.assetBytes(simulator(), 2097152, 'The simulator failed its check');
      encoder = new WebAssembly.Instance(new WebAssembly.Module(wasm), {}).exports;
      encoder._initialize();
    }
    const pcm = W.toInt16(data), at = encoder.sim_sample_alloc(pcm.length);
    if (!at) throw Error('A pad is too long for the sampler');
    new Int16Array(encoder.memory.buffer, at, pcm.length).set(pcm);
    const out = encoder.sim_sample_encode();
    if (!out) throw Error('Encoding failed');
    return new Uint8Array(encoder.memory.buffer, out, encoder.sim_sample_bytes()).slice();
  }
  /* FM1 Open's 8 built-in sounds, made by a fresh copy of the simulator as
   * the stock sample library makes them (sdk/Samples.js), as a kit. */
  async function builtin() {
    const wasm = await root.FM1Web.assetBytes(simulator(), 2097152, 'The simulator failed its check');
    const c = (await WebAssembly.instantiate(wasm, {})).instance.exports;
    c._initialize();
    const at = c.sim_factory(), bytes = c.sim_factory_size();
    if (!at || !bytes) throw Error('The built-in sounds failed to load');
    return KF.builtinKit(new Uint8Array(c.memory.buffer, at, bytes).slice());
  }
  function options(pad) {
    const o = {};
    for (const [key] of FIELDS) o[key] = Number(input(pad, key).value) || 0;
    o.normalize = input(pad, 'normalize').checked; o.reverse = input(pad, 'reverse').checked;
    if (!o.bits) o.bits = 16;
    if (!o.hold) o.hold = W.RATE;
    return o;
  }
  /* Every pad prepared and encoded, then the bank. */
  async function build() {
    const entries = [];
    for (let pad = 0; pad < 8; pad++) {
      const p = pads[pad], name = input(pad, 'name').value;
      if (p.source) {
        p.prepared = W.prepare(p.source, options(pad));
        p.encoded = p.prepared.length ? { name, frames: p.prepared.length, data: await encode(p.prepared) } : null;
      }
      if (p.encoded) p.encoded.name = name;
      entries.push(p.encoded || { name: '', frames: 0, data: new Uint8Array(0) });
      size(pad, p.encoded);
    }
    const used = entries.filter(e => e.frames).length;
    if (!used) throw Error('Add a WAV to at least one pad');
    built = { name: $('sw-kit-name').value, params, bank: KF.buildBank(entries) };
    $('sw-kit-summary').textContent = used + ' pads, ' + built.bank.length + ' bytes, ' +
      entries.reduce((n, e) => n + (e.frames ? KF.sectors(e.data.length) : 0), 0) * 4 + ' KB on the device before sharing.';
    return built;
  }
  function size(pad, sample) {
    const cell = input(pad, 'size');
    cell.textContent = sample ? (sample.frames / W.RATE).toFixed(2) + ' s' : '';
    cell.title = sample ? sample.data.length + ' bytes encoded' : '';
  }
  function load(kit) {
    $('sw-kit-name').value = kit.name;
    params = kit.params;
    KF.readBank(kit.bank).forEach((p, pad) => {
      pads[pad] = { source: null, prepared: null, encoded: p.frames ? p : null };
      input(pad, 'name').value = p.name;
      input(pad, 'wav').value = '';
      size(pad, p.frames ? p : null);
    });
    built = null;
  }
  function rows() {
    for (let pad = 0; pad < 8; pad++) {
      const tr = $('sw-pads').insertRow();
      tr.innerHTML = '<td>' + (pad + 1) + '</td><td><input data-k="wav" type="file" accept=".wav,audio/wav"></td>' +
        '<td><input data-k="name" type="text" maxlength="15"></td>' +
        FIELDS.map(([key, , step]) => '<td><input data-k="' + key + '" type="number" step="' + step + '" value="' + W.DEFAULTS[key] + '"></td>').join('') +
        '<td><input data-k="normalize" type="checkbox"></td><td><input data-k="reverse" type="checkbox"></td>' +
        '<td data-k="size"></td><td><button data-k="play">Play</button></td>';
      input(pad, 'wav').onchange = ui.act(async () => {
        const file = await ui.file(input(pad, 'wav'));
        pads[pad] = { source: W.decode(file.bytes), encoded: null, prepared: null };
        if (!input(pad, 'name').value) input(pad, 'name').value = file.name.replace(/\.wav$/i, '').slice(0, 15).toUpperCase();
        input(pad, 'end').value = (pads[pad].source.data.length / pads[pad].source.rate).toFixed(3);
        built = null;
      });
      input(pad, 'play').onclick = ui.act(async () => {
        const p = pads[pad];
        if (!p.source) throw Error('Pad ' + (pad + 1) + ' has no WAV to play here');
        const data = W.prepare(p.source, options(pad)), context = new AudioContext();
        const buffer = context.createBuffer(1, Math.max(1, data.length), W.RATE), node = context.createBufferSource();
        buffer.copyToChannel(data, 0); node.buffer = buffer; node.connect(context.destination); node.start();
        node.onended = () => context.close();
      });
    }
  }
  const part = {
    mount() {
      $('sw-kits-part').innerHTML = html;
      rows();
      $('sw-kit-build').onclick = ui.act(async () => { await build(); ui.status('Pads prepared.'); });
      $('sw-kit-save').onclick = ui.act(async () => {
        const kit = await build();
        ui.save(KF.writeKit(kit), ui.slug(kit.name) + '.fm1kit');
        ui.status('Kit file saved.');
      });
      $('sw-kit-file').onchange = ui.act(async () => { load(KF.readKit((await ui.file($('sw-kit-file'))).bytes)); ui.status('Kit file opened, CRC checked.'); });
      $('sw-kit-write').onclick = ui.act(async () => {
        const kit = await build();
        const result = await ui.run(async link => { const r = await link.writeKit(kit, ui.progress); await ui.refresh(link); return r; });
        $('sw-kits').value = String(result.id);
        const used = KF.readBank(kit.bank).filter(p => p.frames).length;
        ui.status('Kit ' + result.id + ' written: ' + result.fresh + ' new samples, ' + (used - result.fresh) + ' already on the device.');
      });
      $('sw-kit-builtin').onclick = ui.act(async () => {
        const kit = await builtin();
        const r = await ui.run(async link => {
          const have = await link.findKit(kit.pads);
          if (have) return { have };
          const written = await link.writeKit(kit, ui.progress);
          await ui.refresh(link);
          return written;
        });
        const at = r.have ? r.have.id : r.id;
        $('sw-kits').value = String(at);
        ui.status(r.have ? 'The built-in sounds are already on the device as kit ' + at + ', ' + r.have.name + '. Nothing sent.'
          : 'Kit ' + at + ' written: BUILT-IN, the 8 built-in sounds, ' + r.fresh + ' new samples. Its samples take ' +
            kit.pads.reduce((n, p) => n + KF.sectors(p.data.length), 0) * 4 + ' KB on the device.');
      });
      $('sw-voices-factory').onclick = ui.act(async () => {
        const V = ns.voices, banks = await V.factory(simulator());
        const ask = n => confirm('Replace ' + n + (n > 1 ? ' DX7 banks that hold' : ' DX7 bank that holds') +
          ' saved voices with the factory voices?');
        const r = await ui.run(link => V.sendFactory(link, banks, ask, ui.progress));
        ui.status(r.cancelled ? 'Nothing sent.' : 'The 128 factory DX7 voices are in the DX7 library, all 4 banks read back and checked.');
      });
      $('sw-kit-use').onclick = ui.act(async () => {
        const k = selectedKit();
        await ui.run(async link => { await link.useKit(k.id); await ui.refresh(link); });
        ui.status(k.name + ' is on the pads.');
      });
      $('sw-kit-download').onclick = ui.act(async () => {
        const k = selectedKit(), kit = await ui.run(link => link.readKit(k.id, ui.progress));
        ui.save(KF.writeKit(kit), ui.slug(kit.name) + '.fm1kit');
        load(kit);
        ui.status('Kit file saved, CRC checked.');
      });
      $('sw-kit-delete').onclick = ui.act(async () => {
        const k = selectedKit();
        if (!confirm('Delete ' + k.name + '? Samples no other kit uses go too.')) return;
        await ui.run(async link => { await link.deleteKit(k.id); await ui.refresh(link); });
        ui.status('Deleted.');
      });
    },
    async refresh(link, info) {
      const keep = $('sw-kits').value;
      kits = await link.kits();
      $('sw-kits').replaceChildren(...kits.map(k => ui.option(k.id, String(k.id).padStart(2, '0') + '  ' + k.name + '  (' +
        [...Array(8).keys()].filter(i => k.pads >> i & 1).length + ' pads, ' + k.bytes + ' bytes)' + (k.id === info.kit ? '  in use' : ''))));
      if (kits.some(k => String(k.id) === keep)) $('sw-kits').value = keep;
    },
    enable(busy) {
      for (const id of ['sw-kit-use', 'sw-kit-download', 'sw-kit-delete', 'sw-kit-write', 'sw-kit-builtin', 'sw-voices-factory']) $(id).disabled = busy;
    },
    state: () => ({ pads, built })
  };
  ui.parts.push(part);
  ns.kitsPart = part;
  ui.mount();
})(typeof globalThis !== 'undefined' ? globalThis : this);
