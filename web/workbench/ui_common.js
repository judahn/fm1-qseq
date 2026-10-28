/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* The workbench's "Songs and kits" section (phase 5b): its frame in the
 * library tab, the target (the page's simulator or an FM-1 over USB), the
 * pool gauge, status and files. ui_songs.js and ui_kits.js fill it in. */
(function (root) {
  'use strict';
  if (typeof document === 'undefined') return;
  const ns = root.FM1SongWorkbench = root.FM1SongWorkbench || {};
  const $ = id => document.getElementById(id);
  const html = `
<details id="sw-section" class="library-group" open><summary>Songs and kits (song mode)</summary>
<div class="field-row"><label>Device <select id="sw-target"><option value="sim">Browser simulator</option>
<option value="device">FM-1 over USB</option></select></label></div><button id="sw-refresh">Refresh</button>
<div class="sw-pool" aria-label="Song and sample memory"><span id="sw-pool-songs"></span><span id="sw-pool-samples"></span></div>
<p id="sw-pool-text" class="quiet-note"></p>
<p id="sw-status" role="status" aria-live="polite">Pick where the songs and kits are, then Refresh.</p>
<progress id="sw-progress" value="0" max="1" aria-label="Transfer progress"></progress>
<div id="sw-songs-part"></div><div id="sw-kits-part"></div></details>`;
  const css = `.sw-pool{display:flex;height:10px;border-radius:5px;overflow:hidden;background:#33434b;margin:6px 0}
.sw-pool span{display:block;height:100%}#sw-pool-songs{background:#5fb3a1}#sw-pool-samples{background:#d9a441}
#sw-songs,#sw-kits{display:block;width:100%;min-height:96px}.sw-wide{overflow-x:auto}
.sw-pads{border-collapse:collapse;font-size:12px}.sw-pads td,.sw-pads th{padding:2px 3px;text-align:left;white-space:nowrap}
.sw-pads input{padding:3px 4px;min-width:0;margin:0}.sw-pads input[type=number]{width:58px}.sw-pads input[type=text]{width:84px}
.sw-pads input[type=file]{width:112px}.sw-pads button{padding:3px 8px;margin:0}.sw-map td{padding:2px 6px}
#sw-section pre{white-space:pre-wrap;font-size:12px;max-height:180px;overflow:auto}`;

  let busy = false;
  const ui = ns.ui = {
    $, busy: () => busy,
    status(text) { $('sw-status').textContent = text; },
    progress(value) { $('sw-progress').value = value; },
    /* One transfer at a time, against the chosen target. */
    async run(task) {
      if (busy) throw Error('A transfer is running');
      busy = true;
      ui.enable();
      try {
        if ($('sw-target').value === 'sim') {
          if (!root.FM1SimProtocol) throw Error('The simulator is still loading');
          return await task(ns.link.simulator(root.FM1SimProtocol));
        }
        return await root.FM1Workbench.session(library => task(ns.link.device(library)));
      } finally { busy = false; ui.enable(); }
    },
    act(task) {
      return async () => {
        try { await task(); } catch (error) { ui.status(error.message); } finally { ui.progress(0); }
      };
    },
    save(bytes, name) {
      const url = URL.createObjectURL(new Blob([bytes], { type: 'application/octet-stream' })), a = document.createElement('a');
      a.href = url; a.download = name; document.body.append(a); a.click(); a.remove();
      setTimeout(() => URL.revokeObjectURL(url), 1000);
    },
    async file(input) {
      const file = input.files && input.files[0];
      if (!file) throw Error('Choose a file first');
      return { name: file.name, bytes: new Uint8Array(await file.arrayBuffer()) };
    },
    slug: text => (text.trim() || 'untitled').replace(/[^A-Za-z0-9._-]+/g, '-'),
    option(value, label) { const o = document.createElement('option'); o.value = String(value); o.textContent = label; return o; },
    gauge(info) {
      const songs = info.songSectors / info.sectors, samples = info.sampleSectors / info.sectors;
      $('sw-pool-songs').style.width = (songs * 100).toFixed(1) + '%';
      $('sw-pool-samples').style.width = (samples * 100).toFixed(1) + '%';
      $('sw-pool-text').textContent = 'Memory: songs ' + info.songSectors * 4 + ' KB, kits and samples ' + info.sampleSectors * 4 +
        ' KB, free ' + info.free * 4 + ' KB of ' + info.sectors * 4 + ' KB. ' + info.songs + ' songs, ' + info.kits + ' kits, ' +
        info.samples + ' samples.' + (info.playing ? ' Playing: stop it to write.' : '');
    },
    parts: [],
    enable() { for (const part of ui.parts) part.enable(busy); $('sw-refresh').disabled = busy; },
    async refresh(link) {
      const info = await link.info();
      ui.gauge(info);
      for (const part of ui.parts) await part.refresh(link, info);
      return info;
    }
  };
  function mount() {
    const library = $('tab-library');
    if (!library || $('sw-section')) return;
    const style = document.createElement('style');
    style.textContent = css;
    document.head.append(style);
    library.insertAdjacentHTML('beforeend', html);
    $('sw-refresh').onclick = ui.act(async () => { await ui.run(ui.refresh); ui.status('Up to date.'); });
    $('sw-target').onchange = () => ui.status('Refresh to read ' + ($('sw-target').value === 'sim' ? 'the simulator.' : 'the FM-1.'));
    for (const part of ui.parts) part.mount();
    ui.enable();
  }
  ui.mount = mount;
  if (typeof module === 'object' && module.exports) module.exports = ui;
})(typeof globalThis !== 'undefined' ? globalThis : this);
