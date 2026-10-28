/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Sample prep for kits (phase 5b, design section 8): WAV in, mono at the
 * sampler's 32 kHz, then trim, reverse, fades, normalize, gain and lo-fi,
 * all here in the browser, before the device's SEA5 encoder sees it. */
(function (root) {
  'use strict';
  const ns = root.FM1SongWorkbench = root.FM1SongWorkbench || {};
  const { check, le16, le32 } = ns.bytes;
  const RATE = 32000;

  /* PCM 8/16/24/32-bit or 32-bit float, plain or extensible, mixed to mono. */
  function decode(bytes) {
    check(bytes instanceof Uint8Array && bytes.length >= 44, 'Not a WAV file');
    const tag = at => String.fromCharCode(...bytes.subarray(at, at + 4));
    check(tag(0) === 'RIFF' && tag(8) === 'WAVE', 'Not a WAV file');
    let format = null, data = null;
    for (let at = 12; at + 8 <= bytes.length;) {
      const size = le32(bytes, at + 4), start = at + 8;
      check(start + size <= bytes.length, 'The WAV file is cut short');
      if (tag(at) === 'fmt ') {
        let kind = le16(bytes, start);
        if (kind === 0xfffe && size >= 40) kind = le16(bytes, start + 24);
        format = { kind, channels: le16(bytes, start + 2), rate: le32(bytes, start + 4), bits: le16(bytes, start + 14) };
      } else if (tag(at) === 'data') data = bytes.subarray(start, start + size);
      at = start + size + (size & 1);
    }
    check(format && data, 'The WAV file has no audio');
    const { kind, channels, rate, bits } = format, width = bits / 8;
    check(((kind === 1 && [8, 16, 24, 32].includes(bits)) || (kind === 3 && bits === 32)) && channels >= 1 && channels <= 8 &&
      rate >= 4000 && rate <= 192000, 'Use a PCM or 32-bit float WAV');
    const view = new DataView(data.buffer, data.byteOffset, data.byteLength);
    const frames = Math.floor(data.length / (width * channels)), out = new Float32Array(frames);
    const sample = at => kind === 3 ? view.getFloat32(at, true) : bits === 8 ? (data[at] - 128) / 128
      : bits === 16 ? view.getInt16(at, true) / 32768 : bits === 32 ? view.getInt32(at, true) / 2147483648
      : ((data[at] | data[at + 1] << 8 | data[at + 2] << 16) << 8 >> 8) / 8388608;
    for (let f = 0; f < frames; f++) {
      let sum = 0;
      for (let c = 0; c < channels; c++) sum += sample((f * channels + c) * width);
      out[f] = sum / channels;
    }
    return { rate, data: out };
  }
  /* 16-bit mono WAV, for tests and for saving a prepared pad. */
  function encode(data, rate = RATE) {
    const bytes = new Uint8Array(44 + data.length * 2), v = new DataView(bytes.buffer);
    const text = (at, s) => { for (let i = 0; i < 4; i++) bytes[at + i] = s.charCodeAt(i); };
    text(0, 'RIFF'); v.setUint32(4, 36 + data.length * 2, true); text(8, 'WAVE'); text(12, 'fmt ');
    v.setUint32(16, 16, true); v.setUint16(20, 1, true); v.setUint16(22, 1, true); v.setUint32(24, rate, true);
    v.setUint32(28, rate * 2, true); v.setUint16(32, 2, true); v.setUint16(34, 16, true); text(36, 'data');
    v.setUint32(40, data.length * 2, true);
    toInt16(data).forEach((s, i) => v.setInt16(44 + i * 2, s, true));
    return bytes;
  }
  const toInt16 = data => Int16Array.from(data, s => Math.max(-32768, Math.min(32767, Math.round(s * 32767))));

  /* Windowed-sinc resampling, low-passed at the lower of the two rates. */
  function resample(data, from, to = RATE) {
    if (from === to) return Float32Array.from(data);
    const ratio = from / to, cutoff = Math.min(1, to / from), taps = 16;
    const out = new Float32Array(Math.max(1, Math.round(data.length / ratio)));
    for (let i = 0; i < out.length; i++) {
      const center = i * ratio, first = Math.ceil(center - taps / cutoff), last = Math.floor(center + taps / cutoff);
      let sum = 0, weight = 0;
      for (let j = Math.max(0, first); j <= Math.min(data.length - 1, last); j++) {
        const x = (j - center) * cutoff, window = 0.5 + 0.5 * Math.cos(Math.PI * x / taps);
        const w = (x ? Math.sin(Math.PI * x) / (Math.PI * x) : 1) * window;
        sum += data[j] * w; weight += w;
      }
      out[i] = weight ? sum / weight : 0;
    }
    return out;
  }

  /* The prep chain, in this order. Times in seconds and milliseconds, gain in
   * dB, bits 1 to 16 (16 is off), hold rate in Hz (32000 is off). */
  const DEFAULTS = { start: 0, end: 0, reverse: false, fadeIn: 0, fadeOut: 0, normalize: false, gain: 0, bits: 16, hold: RATE };
  function prepare(source, options = {}) {
    const o = { ...DEFAULTS, ...options };
    const data = resample(source.data, source.rate, RATE);
    const first = Math.min(data.length, Math.max(0, Math.round(o.start * RATE)));
    const last = o.end > o.start ? Math.min(data.length, Math.round(o.end * RATE)) : data.length;
    let out = data.slice(first, Math.max(first, last));
    if (o.reverse) out.reverse();
    const fadeIn = Math.min(out.length, Math.round(o.fadeIn * RATE / 1000)), fadeOut = Math.min(out.length, Math.round(o.fadeOut * RATE / 1000));
    for (let i = 0; i < fadeIn; i++) out[i] *= i / fadeIn;
    for (let i = 0; i < fadeOut; i++) out[out.length - 1 - i] *= i / fadeOut;
    const peak = out.reduce((m, v) => Math.max(m, Math.abs(v)), 0);
    const scale = (o.normalize && peak ? 0.966 / peak : 1) * 10 ** (o.gain / 20); /* normalize to -0.3 dBFS */
    const step = o.bits < 16 ? 2 / 2 ** o.bits : 0, every = o.hold < RATE ? RATE / o.hold : 1;
    let held = 0;
    for (let i = 0; i < out.length; i++) {
      if (every === 1 || Math.floor(i / every) !== Math.floor((i - 1) / every)) held = out[i] * scale;
      let v = every === 1 ? out[i] * scale : held;
      if (step) v = Math.round(v / step) * step;
      out[i] = Math.max(-1, Math.min(1, v));
    }
    return out;
  }
  ns.wav = { RATE, DEFAULTS, decode, encode, resample, prepare, toInt16 };
  if (typeof module === 'object' && module.exports) module.exports = ns.wav;
})(typeof globalThis !== 'undefined' ? globalThis : this);
