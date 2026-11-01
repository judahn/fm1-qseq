#!/usr/bin/env python3
"""synth.py: QSEQ's four synthesized drum kits (JUNGLE, DNB, BOOMBAP, LOFI) as one-shots.

Pure synthesis only: oscillators, noise, filters, envelopes and effects in code.
No samples, no recordings, no network.

Output (next to this script):
    <kit>/<N>-<NAME>.wav   32000 Hz, mono, 16-bit PCM, N = pad 1..8
    <kit>.png              contact sheet (waveform + spectrogram per pad), git ignores it
Then node tools/make-kit.mjs kits/<kit> makes each kit file (kits/README.md).
Kits: JUNGLE, DNB, BOOMBAP, LOFI. Pad order in every kit:
    1 kick, 2 snare, 3 closed hat, 4 open hat, 5 snare 2 / clap / ghost,
    6 rim / perc, 7 ride / shaker, 8 crash / stab / fx

Rerun:  python3 kits/synth.py      (numpy, scipy and matplotlib; fixed seeds, identical output every run)
"""
import math
import os
import re
import zlib

import numpy as np
from scipy import signal
from scipy.io import wavfile
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
FS = 32000            # device rate
OS = 3                # synthesize at 96 kHz, decimate to 32 kHz (keeps saws/FM/saturation clean)
FSI = FS * OS
PEAK = 10 ** (-1 / 20)  # -1 dBFS
SECTOR_BUDGET = 16
TWO_PI = 2 * np.pi


# ----------------------------------------------------------------------------- basics
def rng_for(name):
    return np.random.default_rng(zlib.crc32(name.encode()))


def nsamp(dur):
    return int(round(dur * FS)) * OS


def tvec(dur):
    return np.arange(nsamp(dur)) / FSI


def farr(f, n):
    return np.full(n, float(f))


def norm(x):
    m = np.max(np.abs(x))
    return x / m if m > 0 else x


def sat(x, drive):
    """Symmetric tanh saturation, unity at full scale."""
    return np.tanh(drive * x) / np.tanh(drive)


def tape(x, drive=1.8, bias=0.08):
    """Asymmetric soft clip (tape-ish): adds even harmonics. DC removed later."""
    y = np.tanh(drive * (x + bias)) - np.tanh(drive * bias)
    return norm(y)


def bitcrush(x, bits):
    q = 2 ** (bits - 1)
    return np.round(x * q) / q


def exp_env(t, tau, hold=0.0):
    return np.where(t < hold, 1.0, np.exp(-(t - hold) / tau))


def decimate(x):
    return signal.resample_poly(x, 1, OS)


# ----------------------------------------------------------------------------- filters
def _sos(x, kind, f, order, fs):
    sos = signal.butter(order, f, btype=kind, fs=fs, output="sos")
    return signal.sosfilt(sos, x)


def lp(x, f, order=2, fs=FSI):
    return _sos(x, "lowpass", f, order, fs)


def hp(x, f, order=2, fs=FSI):
    return _sos(x, "highpass", f, order, fs)


def bp(x, lo, hi, order=2, fs=FSI):
    return _sos(x, "bandpass", [lo, hi], order, fs)


def peaking(x, f0, gain_db, q, fs=FSI):
    A = 10 ** (gain_db / 40)
    w = TWO_PI * f0 / fs
    alpha = np.sin(w) / (2 * q)
    b = [1 + alpha * A, -2 * np.cos(w), 1 - alpha * A]
    a = [1 + alpha / A, -2 * np.cos(w), 1 - alpha / A]
    return signal.lfilter(b, a, x)


def high_shelf(x, f0, gain_db, fs=FSI):
    A = 10 ** (gain_db / 40)
    w = TWO_PI * f0 / fs
    c, s = np.cos(w), np.sin(w)
    alpha = s / 2 * np.sqrt(2)
    sa = 2 * np.sqrt(A) * alpha
    b = [A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa)]
    a = [(A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa]
    return signal.lfilter(b, a, x)


def svf_lp(x, fc, q):
    """TPT state-variable low-pass with per-sample cutoff (Zavalishin)."""
    x = np.asarray(x, float)
    fc = np.broadcast_to(np.asarray(fc, float), x.shape)
    g = np.tan(np.pi * np.clip(fc, 10.0, 0.45 * FSI) / FSI)
    k = 1.0 / q
    a1 = 1.0 / (1.0 + g * (g + k))
    a2 = g * a1
    a3 = g * a2
    xs, A1, A2, A3 = x.tolist(), a1.tolist(), a2.tolist(), a3.tolist()
    out = [0.0] * len(xs)
    ic1 = ic2 = 0.0
    for i in range(len(xs)):
        v3 = xs[i] - ic2
        v1 = A1[i] * ic1 + A2[i] * v3
        v2 = ic2 + A2[i] * ic1 + A3[i] * v3
        ic1 = 2.0 * v1 - ic1
        ic2 = 2.0 * v2 - ic2
        out[i] = v2
    return np.array(out)


def phaser(x, f_lo, f_hi, rate, stages=4, mix=0.5, ph0=0.0):
    """Swept first-order all-pass chain mixed with the dry signal (moving notches)."""
    n = len(x)
    t = np.arange(n) / FSI
    fc = f_lo * (f_hi / f_lo) ** (0.5 - 0.5 * np.cos(TWO_PI * rate * t + ph0))
    tn = np.tan(np.pi * fc / FSI)
    a = ((tn - 1) / (tn + 1)).tolist()
    xs = np.asarray(x, float).tolist()
    xp = [0.0] * stages
    yp = [0.0] * stages
    out = [0.0] * n
    for i in range(n):
        v = xs[i]
        ai = a[i]
        for s in range(stages):
            y = ai * v + xp[s] - ai * yp[s]
            xp[s] = v
            yp[s] = y
            v = y
        out[i] = v
    return np.asarray(x) + mix * np.array(out)


def room(x, name, tau, wet, bright=7000.0, predelay=0.002):
    """Short synthetic room: a few early reflections + exponentially decaying filtered noise."""
    r = rng_for(name + "/room")
    L = int(min(6 * tau + predelay + 0.01, 0.6) * FSI)
    t = np.arange(L) / FSI
    nz = r.standard_normal(L)
    diffuse = lp(nz, bright, 2) * np.exp(-t / tau) * np.clip((t - predelay) / 0.005, 0, 1)
    late_dark = lp(nz, bright / 3, 2) * np.exp(-t / (tau * 1.5)) * np.clip((t - predelay) / 0.01, 0, 1)
    er = np.zeros(L)
    for ms, a in [(2.3, 0.7), (3.9, -0.55), (6.1, 0.45), (8.7, -0.38), (12.2, 0.3), (16.8, -0.22)]:
        i = int(ms * 1e-3 * FSI)
        if i < L:
            er[i] += a
    er = lp(er, bright * 0.8, 2)
    ir = (0.6 * er / np.sqrt(np.sum(er ** 2)) + diffuse / np.sqrt(np.sum(diffuse ** 2))
          + 0.5 * late_dark / np.sqrt(np.sum(late_dark ** 2)))
    ir /= np.sqrt(np.sum(ir ** 2))
    return x + wet * signal.fftconvolve(x, ir)[: len(x)]


# ----------------------------------------------------------------------------- oscillators
def phase(freq):
    """Integrated phase in cycles, starting at 0."""
    return np.concatenate(([0.0], np.cumsum(freq[:-1]))) / FSI


def osc_sine(freq, ph0=0.0):
    return np.sin(TWO_PI * (phase(freq) + ph0))


def _polyblep(p, dt):
    out = np.zeros_like(p)
    m = p < dt
    x = p[m] / dt[m]
    out[m] = x + x - x * x - 1.0
    m = p > 1.0 - dt
    x = (p[m] - 1.0) / dt[m]
    out[m] = x * x + x + x + 1.0
    return out


def osc_saw(freq, ph0=0.0):
    p = (phase(freq) + ph0) % 1.0
    return 2.0 * p - 1.0 - _polyblep(p, freq / FSI)


def osc_square(freq, ph0=0.0):
    return 0.5 * (osc_saw(freq, ph0) - osc_saw(freq, ph0 + 0.5))


METAL_808 = [205.3, 304.4, 369.6, 522.7, 540.0, 800.0]   # classic analog six-square hat recipe


def metal(n, scale, r):
    return sum(osc_square(farr(f * scale, n), r.random()) for f in METAL_808)


# ----------------------------------------------------------------------------- instruments
def kick(name, dur, f_start, f_body, ptau, atau, hold=0.0, click=0.3, click_band=(1500, 7000),
         click_tau=0.002, thump=0.2, thump_cut=250, drive=1.5, tone_lp=None, sub2=0.0):
    """Pitch-swept sine body + band-passed noise click + low noise thump, then saturation."""
    r = rng_for(name)
    t = tvec(dur)
    n = len(t)
    f = f_body + (f_start - f_body) * np.exp(-t / ptau)
    env = exp_env(t, atau, hold)
    body = osc_sine(f) * env
    if sub2:
        body += sub2 * osc_sine(2 * f) * env * np.exp(-t / 0.03)
    cl = norm(bp(r.standard_normal(n), *click_band)) * np.exp(-t / click_tau)
    th = norm(lp(r.standard_normal(n), thump_cut, 2)) * np.exp(-t / 0.012)
    x = body + click * cl + thump * th
    x = sat(norm(x), drive)
    if tone_lp:
        x = lp(x, tone_lp, 2)
    return decimate(x)


def snare(name, dur, modes, drop=1.2, drop_tau=0.008, wires_band=(2000, 8000), wires_amp=1.0,
          wires_tau=0.07, wires_hold=0.0, low_band=(700, 2000), low_amp=0.3, low_tau=0.04,
          crack=0.5, crack_tau=0.0012, swish=0.0, swish_band=(1500, 7000), swish_peak=0.02,
          swish_tau=0.07, room_tau=0.03, room_wet=0.25, shelf_db=0.0, drive=1.5, tone_lp=None,
          body_mix=0.9):
    """Modal drum-head body + wire noise + crack transient (+ optional brush swish) + room."""
    r = rng_for(name)
    t = tvec(dur)
    n = len(t)
    pd = 1 + (drop - 1) * np.exp(-t / drop_tau)
    body = sum(a * osc_sine(f * pd) * np.exp(-t / tau) for f, a, tau in modes)
    wires = norm(bp(r.standard_normal(n), *wires_band)) * exp_env(t, wires_tau, wires_hold)
    low = norm(bp(r.standard_normal(n), *low_band)) * np.exp(-t / low_tau)
    cr = norm(hp(r.standard_normal(n), 3000, 2)) * np.exp(-t / crack_tau)
    x = body_mix * norm(body) + wires_amp * wires + low_amp * low + crack * cr
    if swish:
        s = t / swish_peak
        env = s * np.exp(1 - s)                      # rises to 1 at swish_peak
        env = np.where(t > swish_peak, np.exp(-(t - swish_peak) / swish_tau), env)
        x += swish * norm(bp(r.standard_normal(n), *swish_band)) * env
    x = room(x, name, room_tau, room_wet)
    if shelf_db:
        x = high_shelf(x, 6000, shelf_db)
    x = sat(norm(x), drive)
    if tone_lp:
        x = lp(x, tone_lp, 2)
    return decimate(x)


def hat(name, dur, env_fn, hp_f=7000, hp_order=4, boost=None, metal_amp=0.6, noise_amp=0.6,
        scale=1.0, lp_f=None):
    """Six detuned squares (inharmonic cluster) + white noise, high-passed, enveloped."""
    r = rng_for(name)
    t = tvec(dur)
    n = len(t)
    src = metal_amp * norm(metal(n, scale, r)) + noise_amp * norm(r.standard_normal(n))
    src = hp(src, hp_f, hp_order)
    if boost:
        src = peaking(src, boost[0], boost[1], boost[2])
    if lp_f:
        src = lp(src, lp_f, 2)
    x = src * env_fn(t)
    return decimate(x)


def ride(name, dur, f_lo, f_hi, n_part, tau_lo, wash_band, wash_tau, fm_c, fm_ratio, fm_index,
         ping=0.4, mix=(0.55, 0.3, 0.4), hp_f=350, low_gain=0.35):
    """Additive inharmonic partials with shimmer pairs + FM cluster + noise wash + stick ping."""
    r = rng_for(name)
    t = tvec(dur)
    n = len(t)
    bell = np.zeros(n)
    u = np.sort(r.random(n_part))
    for k in range(n_part):
        f = f_lo * (f_hi / f_lo) ** (u[k] ** 0.75)   # denser toward the top, like a cymbal
        amp = r.uniform(0.45, 1.0) * (1.0 if f > 2000 else low_gain)
        tau = tau_lo * (1500 / (f + 1500)) ** 0.4 + 0.03
        df = r.uniform(2.0, 7.0)                      # slow beating partner = shimmer
        e = np.exp(-t / tau)
        bell += amp * e * (np.sin(TWO_PI * (f * t + r.random()))
                           + 0.45 * np.sin(TWO_PI * ((f + df) * t + r.random())))
    idx = fm_index * np.exp(-t / 0.05) + 0.8
    fm = np.sin(TWO_PI * fm_c * t + idx * np.sin(TWO_PI * fm_c * fm_ratio * t)) * np.exp(-t / (tau_lo * 1.1))
    wash = norm(bp(r.standard_normal(n), *wash_band)) * np.exp(-t / wash_tau)
    pg = norm(hp(r.standard_normal(n), 2500, 2)) * np.exp(-t / 0.0015)
    x = mix[0] * norm(bell) + mix[1] * norm(fm) + mix[2] * wash + ping * pg
    x = hp(x, hp_f, 2) * (1 - np.exp(-t / 0.0003))
    return decimate(x)


def modal(name, dur, modes, drop=0.0, drop_tau=0.01, click=0.3, click_band=(2000, 12000),
          click_tau=0.001, noise=None, room_tau=None, room_wet=0.0, drive=1.0, tone_lp=None):
    """Sum of decaying sine modes (rim, snap, woodblock, conga) + click + optional noise."""
    r = rng_for(name)
    t = tvec(dur)
    n = len(t)
    pd = 1 + drop * np.exp(-t / drop_tau)
    x = sum(a * osc_sine(f * pd) * np.exp(-t / tau) for f, a, tau in modes)
    x = norm(x)
    if click:
        x += click * norm(bp(r.standard_normal(n), *click_band)) * np.exp(-t / click_tau)
    if noise:
        band, amp, tau = noise
        x += amp * norm(bp(r.standard_normal(n), *band)) * np.exp(-t / tau)
    if room_tau:
        x = room(x, name, room_tau, room_wet)
    x = sat(norm(x), drive)
    if tone_lp:
        x = lp(x, tone_lp, 2)
    return decimate(x)


def clap(name, dur, offsets, amps, burst_tau, tail_tau, band, center, room_tau, room_wet, drive):
    """Several fast noise bursts (hands not quite together) then a longer tail, plus room."""
    r = rng_for(name)
    t = tvec(dur)
    n = len(t)
    nz = peaking(bp(r.standard_normal(n), *band), center, 6, 1.4)
    env = np.zeros(n)
    for i, (o, a) in enumerate(zip(offsets, amps)):
        tt = np.clip(t - o, 0, None)
        tau = tail_tau if i == len(offsets) - 1 else burst_tau
        env += a * (t >= o) * np.exp(-tt / tau) * (1 - np.exp(-tt / 0.00025))
    x = room(norm(nz) * env, name, room_tau, room_wet)
    x = sat(norm(x), drive)
    return decimate(x)


def shaker(name, dur, band, rise, tau, start=0.3, bead_rate=3000):
    """Band-passed noise modulated by a random bead train, quick swell then decay."""
    r = rng_for(name)
    t = tvec(dur)
    n = len(t)
    nz = norm(bp(r.standard_normal(n), *band))
    beads = np.zeros(n)
    idx = r.random(n) < bead_rate / FSI
    beads[idx] = r.uniform(0.3, 1.0, idx.sum())
    beads = np.convolve(beads, np.hanning(int(0.0004 * FSI) + 2), "same")
    tex = nz * (0.45 + norm(beads))
    s = np.clip(t / rise, 0, 1)
    env = (start + (1 - start) * (0.5 - 0.5 * np.cos(np.pi * s))) * exp_env(t, tau, rise)
    return decimate(tex * env)


def tamb(name, dur, hits, n_jingles=10, jingle_band=(5200, 12500), wash_tau=0.035, skin=0.25):
    """Tambourine: clusters of inharmonic jingle partials over a few micro-hits + noise wash + skin."""
    r = rng_for(name)
    t = tvec(dur)
    n = len(t)
    jf = r.uniform(*jingle_band, n_jingles)
    jt = r.uniform(0.03, 0.08, n_jingles)
    jingles = np.zeros(n)
    wenv = np.zeros(n)
    for o, a in hits:
        tt = np.clip(t - o, 0, None)
        on = (t >= o) * (1 - np.exp(-tt / 0.0002))
        for f, tau in zip(jf, jt):
            f2 = f * (1 + r.uniform(-0.01, 0.01))
            jingles += a * on * np.sin(TWO_PI * f2 * tt + TWO_PI * r.random()) * np.exp(-tt / tau)
        wenv += a * on * np.exp(-tt / wash_tau)
    wash = norm(hp(r.standard_normal(n), 6000, 2)) * wenv
    sk = np.sin(TWO_PI * 280 * t) * np.exp(-t / 0.012) + 0.6 * norm(lp(r.standard_normal(n), 1500)) * np.exp(-t / 0.004)
    x = 0.65 * norm(jingles) + 0.5 * norm(wash) + skin * sk
    return decimate(x)


def stab(name, dur, notes, note_amps, detune=(-10, 0, 10), fc_hi=6500, fc_lo=500, fc_tau=0.07,
         q=1.5, amp_tau=0.14, room_tau=0.05, room_wet=0.2, drive=1.4):
    """Detuned saw chord through a 4-pole state-variable LP with a decaying filter envelope."""
    r = rng_for(name)
    t = tvec(dur)
    n = len(t)
    src = np.zeros(n)
    for f, a in zip(notes, note_amps):
        for c in detune:
            src += a * osc_saw(farr(f * 2 ** (c / 1200), n), r.random())
    src = norm(src)
    fc = fc_lo + (fc_hi - fc_lo) * np.exp(-t / fc_tau)
    y = svf_lp(svf_lp(src, fc, 0.6), fc, q)
    y *= (1 - np.exp(-t / 0.0015)) * np.exp(-t / amp_tau)
    y = room(y, name, room_tau, room_wet)
    y = sat(norm(y), drive)
    return decimate(y)


def horn(name, dur, notes, detune=(-7, 7), scoop_cents=-40, scoop_tau=0.025, start=0.4,
         swell_tau=0.02, hold=0.14, rel_tau=0.1, fc_base=300, fc_amt=3600, bright_rise=0.03,
         bright_tau=0.25, q=0.9, breath=0.06, room_tau=0.06, room_wet=0.25, drive=1.5):
    """Brass-ish stab: saws with a pitch scoop, amplitude swell, brightness that follows the swell."""
    r = rng_for(name)
    t = tvec(dur)
    n = len(t)
    src = np.zeros(n)
    for f in notes:
        for c in detune:
            cents = c + scoop_cents * r.uniform(0.8, 1.2) * np.exp(-t / scoop_tau)
            src += osc_saw(f * 2 ** (cents / 1200), r.random())
    src = norm(src)
    amp = (start + (1 - start) * (1 - np.exp(-t / swell_tau))) * exp_env(t, rel_tau, hold)
    amp *= 1 - np.exp(-t / 0.001)
    bright = (1 - np.exp(-t / bright_rise)) * np.exp(-t / bright_tau)
    fc = fc_base + fc_amt * bright
    y = svf_lp(svf_lp(src, fc, 0.6), fc, q) * amp
    y += breath * norm(bp(r.standard_normal(n), 1000, 4000)) * amp * bright
    y = room(y, name, room_tau, room_wet)
    y = sat(norm(y), drive)
    return decimate(y)


def reese(name, dur, f0=55.0, cents=16, fc0=1100, fc1=420, fc_tau=0.15, q=1.3, hold=0.36,
          rel_tau=0.05, drive=1.4, phaser_mix=0.5):
    """Two detuned saws near 55 Hz, saturated, 4-pole LP, swept phaser, gated envelope."""
    t = tvec(dur)
    n = len(t)
    s1 = osc_saw(farr(f0 * 2 ** (-cents / 1200), n), 0.0)
    s2 = osc_saw(farr(f0 * 2 ** (cents / 1200), n), 0.0)
    src = sat(0.5 * (s1 + s2), drive)
    fc = fc1 + (fc0 - fc1) * np.exp(-t / fc_tau)
    y = svf_lp(svf_lp(src, fc, 0.6), fc, q)
    y = phaser(y, 250, 1500, 1.7, 4, phaser_mix)
    y *= (1 - np.exp(-t / 0.003)) * exp_env(t, rel_tau, hold)
    return decimate(y)


def epiano(name, dur, notes, strum=0.004, tone_lp=5500):
    """Rhodes-like FM: 1:1 body with decaying bark + 14:1 tine ping, pickup asymmetry."""
    r = rng_for(name)
    t = tvec(dur)
    n = len(t)
    out = np.zeros(n)
    for i, f in enumerate(notes):
        o = i * strum * r.uniform(0.7, 1.3) if i else 0.0
        tt = np.clip(t - o, 0, None)
        on = t >= o
        vel = r.uniform(0.8, 1.0) * (220 / f) ** 0.25
        ph = TWO_PI * f * tt
        ib = 1.5 * np.exp(-tt / 0.09) + 0.3
        body = np.sin(ph + ib * np.sin(ph))
        tine = np.sin(ph + 1.2 * np.exp(-tt / 0.015) * np.sin(14 * ph))
        amp = (1 - np.exp(-tt / 0.0012)) * (0.6 * np.exp(-tt / 0.3) + 0.4 * np.exp(-tt / 0.09))
        note = (0.75 * body + 0.35 * tine) * amp * vel * on
        out += note + 0.18 * note ** 2        # asymmetric pickup -> warm 2nd harmonic
    out = lp(out, tone_lp, 2)
    return decimate(out)


def crackle(name, dur, tick_rate=260, pop_rate=9, hiss=0.05):
    """Vinyl crackle: dense Poisson ticks (heavy-tailed sizes) + a few soft pops + hiss, opening pop."""
    r = rng_for(name)
    n = nsamp(dur)
    lo, hi = int(0.003 * FSI), n - int(0.02 * FSI)
    nt, npop = int(r.poisson(tick_rate * dur)), int(r.poisson(pop_rate * dur))
    sharp = np.zeros(n)
    soft = np.zeros(n)
    np.add.at(sharp, r.integers(lo, hi, nt),
              np.minimum(r.pareto(2.2, nt) * 0.07 + 0.03, 0.6) * r.choice([-1.0, 1.0], nt))
    np.add.at(soft, r.integers(lo, hi, npop), r.uniform(0.25, 0.55, npop) * r.choice([-1.0, 1.0], npop))
    sharp[0] = 1.0
    soft[0] = 0.7
    k1 = r.standard_normal(int(0.0004 * FSI)) * np.exp(-np.arange(int(0.0004 * FSI)) / (0.00008 * FSI))
    k2 = lp(r.standard_normal(int(0.002 * FSI)), 2500) * np.exp(-np.arange(int(0.002 * FSI)) / (0.0004 * FSI))
    clicks = lp(hp(signal.fftconvolve(sharp, k1)[:n], 1200, 2), 9000, 2)
    pops = bp(signal.fftconvolve(soft, k2)[:n], 200, 3000, 2)
    hs = lp(hp(r.standard_normal(n), 3000, 2), 10000, 2)
    x = norm(clicks) + 0.6 * norm(pops) + hiss * norm(hs)
    return decimate(x)


# ----------------------------------------------------------------------------- kit processing
def wobble(x, depth_cents, rate, flutter_cents, flutter_rate, seed):
    """Tape wow/flutter: variable-rate read of the signal (at FS)."""
    r = np.random.default_rng(seed)
    n = len(x)
    t = np.arange(n) / FS
    cents = (depth_cents * np.sin(TWO_PI * rate * t + TWO_PI * r.random())
             + flutter_cents * np.sin(TWO_PI * flutter_rate * t + TWO_PI * r.random()))
    ratio = 2 ** (cents / 1200)
    pos = np.minimum(np.cumsum(ratio) - ratio[0], n - 1)
    return np.interp(pos, np.arange(n), x)


def process_boombap(x, name):
    x = sat(norm(x) * 0.95, 1.6)          # gentle saturation
    x = bitcrush(x * 0.95, 12)            # 12-bit
    return lp(x, 10000, 4, FS)            # sampler-ish output filter


def process_lofi(x, name, tonal=0.0):
    x = norm(x) * 0.95
    if tonal:
        x = wobble(x, 9 * tonal, 0.9, 3 * tonal, 6.5, zlib.crc32(name.encode()))
    x = tape(x, 1.8, 0.08)                # tape-style saturation
    x = bitcrush(x * 0.95, 11)            # 11-bit
    return lp(x, 7000, 3, FS)


# ----------------------------------------------------------------------------- finalize
def finalize(x, fade_ms=10.0, fade_in=6, lead_db=-50.0):
    x = hp(np.asarray(x, float), 12, 1, FS)                 # DC blocker
    thr = np.max(np.abs(x)) * 10 ** (lead_db / 20)
    x = x[int(np.argmax(np.abs(x) > thr)):]                # start at the transient
    n = len(x)
    w = np.ones(n)
    w[:fade_in] = np.sin(0.5 * np.pi * (np.arange(fade_in) + 1) / (fade_in + 1)) ** 2
    nf = max(int(round(fade_ms * FS / 1000)), int(0.005 * FS) + 1)
    w[-nf:] = 0.5 + 0.5 * np.cos(np.pi * np.arange(1, nf + 1) / nf)   # ends at exactly 0
    c = np.sum(w * x) / np.sum(w)                           # weighted mean -> exact zero DC
    y = w * (x - c)
    y *= PEAK / np.max(np.abs(y))
    q = np.round(y * 32767).astype(np.int16)
    q[-1] = 0
    return q, nf


def sectors(frames):
    byts = math.ceil(frames / 320) * 228
    return math.ceil((64 + byts) / 4096)


# ----------------------------------------------------------------------------- the kits
def oh_env(chick_tau, sus_tau, chick=0.45):
    return lambda t: chick * np.exp(-t / chick_tau) + (1 - chick) * np.exp(-t / sus_tau)


def ch_env(tau, hold=0.0):
    return lambda t: exp_env(t, tau, hold)


def kit_jungle():
    K = "JUNGLE/"
    return [
        ("KICK", kick(K + "KICK", 0.30, f_start=230, f_body=57, ptau=0.014, atau=0.075, hold=0.02,
                      click=0.35, click_band=(1200, 6000), click_tau=0.0025, thump=0.25, drive=1.8), 20, 0),
        ("SNARE", snare(K + "SNARE", 0.33, modes=[(200, 1.0, 0.04), (335, 0.5, 0.025), (480, 0.2, 0.018)],
                        drop=1.3, drop_tau=0.006, wires_band=(2000, 8000), wires_amp=1.4, wires_tau=0.075,
                        wires_hold=0.004, low_band=(700, 2000), low_amp=0.35, low_tau=0.04, crack=0.8,
                        crack_tau=0.0012, room_tau=0.03, room_wet=0.3, shelf_db=4, drive=2.0,
                        body_mix=0.65), 20, 0),
        ("HAT", hat(K + "HAT", 0.065, ch_env(0.013), hp_f=6500, boost=(9500, 4, 1.0),
                    metal_amp=0.55, noise_amp=0.7), 8, 0),
        ("OPEN HAT", hat(K + "OPEN HAT", 0.34, oh_env(0.012, 0.09), hp_f=6500, boost=(9500, 4, 1.0),
                         metal_amp=0.6, noise_amp=0.6), 30, 0),
        ("GHOST", snare(K + "GHOST", 0.12, modes=[(205, 1.0, 0.022), (345, 0.35, 0.015)], drop=1.15,
                        wires_band=(1500, 6000), wires_amp=0.9, wires_tau=0.025, low_amp=0.2, low_tau=0.02,
                        crack=0.08, room_tau=0.018, room_wet=0.12, drive=1.1, tone_lp=6500,
                        body_mix=0.6), 12, 0),
        ("RIM", modal(K + "RIM", 0.09, modes=[(540, 1.0, 0.012), (1680, 0.8, 0.009), (2950, 0.4, 0.006),
                                              (4500, 0.2, 0.004)], click=0.5, click_tau=0.0008, drive=1.6), 8, 0),
        ("RIDE", ride(K + "RIDE", 0.53, 600, 13000, 30, 0.16, (4000, 12000), 0.14, 3200, 1.414, 4.0,
                      ping=0.35), 90, 0),
        ("STAB", stab(K + "STAB", 0.50, notes=[110.0, 220.0, 261.63, 329.63, 392.0],
                      note_amps=[0.6, 1.0, 0.9, 0.9, 0.85], fc_hi=6500, fc_lo=500, fc_tau=0.07, q=1.5,
                      amp_tau=0.12, room_tau=0.05, room_wet=0.2, drive=1.4), 50, 0),
    ]


def kit_dnb():
    K = "DNB/"
    return [
        ("KICK", kick(K + "KICK", 0.26, f_start=320, f_body=54, ptau=0.009, atau=0.055, hold=0.035,
                      click=0.45, click_band=(2500, 9000), click_tau=0.0015, thump=0.15, drive=3.5), 20, 0),
        ("SNARE", snare(K + "SNARE", 0.26, modes=[(205, 1.0, 0.045), (370, 0.35, 0.02)], drop=1.8,
                        drop_tau=0.003, wires_band=(1800, 9000), wires_amp=1.5, wires_tau=0.035,
                        wires_hold=0.015, low_band=(900, 2500), low_amp=0.3, low_tau=0.03, crack=1.0,
                        crack_tau=0.0008, room_tau=0.02, room_wet=0.12, drive=3.0, body_mix=0.6), 15, 0),
        ("HAT", hat(K + "HAT", 0.05, ch_env(0.009), hp_f=8000, boost=(11000, 3, 1.0),
                    metal_amp=0.5, noise_amp=0.8, scale=1.15), 6, 0),
        ("OPEN HAT", hat(K + "OPEN HAT", 0.30, oh_env(0.01, 0.075), hp_f=7500, boost=(10500, 3, 1.0),
                         metal_amp=0.6, noise_amp=0.6, scale=1.15), 30, 0),
        ("CLAP", clap(K + "CLAP", 0.17, offsets=(0.0, 0.0085, 0.0178, 0.0295), amps=(0.9, 0.75, 0.85, 1.0),
                      burst_tau=0.0045, tail_tau=0.032, band=(900, 5000), center=1600, room_tau=0.02,
                      room_wet=0.15, drive=1.8), 15, 0),
        ("SHAKER", shaker(K + "SHAKER", 0.11, band=(5000, 13000), rise=0.006, tau=0.022, start=0.35), 10, 0),
        ("RIDE", ride(K + "RIDE", 0.50, 700, 13500, 26, 0.12, (5000, 13000), 0.1, 4100, 1.53, 5.0,
                      ping=0.5, mix=(0.6, 0.35, 0.3)), 80, 0),
        ("REESE", reese(K + "REESE", 0.53, f0=55.0, cents=11, fc0=1600, fc1=650, fc_tau=0.15, q=1.4,
                        hold=0.36, rel_tau=0.05, drive=1.4, phaser_mix=0.5), 50, 0),
    ]


def kit_boombap():
    K = "BOOMBAP/"
    return [
        ("KICK", kick(K + "KICK", 0.48, f_start=160, f_body=50, ptau=0.03, atau=0.12, hold=0.04,
                      click=0.2, click_band=(800, 4000), click_tau=0.004, thump=0.35, thump_cut=300,
                      drive=2.2), 45, 0),
        ("SNARE", snare(K + "SNARE", 0.34, modes=[(180, 1.0, 0.06), (290, 0.55, 0.04), (420, 0.2, 0.025)],
                        drop=1.2, drop_tau=0.01, wires_band=(1200, 7000), wires_amp=0.9, wires_tau=0.09,
                        wires_hold=0.003, low_band=(500, 1500), low_amp=0.45, low_tau=0.05, crack=0.5,
                        crack_tau=0.0015, room_tau=0.045, room_wet=0.4, drive=2.0), 30, 0),
        ("HAT", hat(K + "HAT", 0.10, ch_env(0.02), hp_f=5500, boost=(8000, 4, 1.2),
                    metal_amp=0.45, noise_amp=0.8, scale=0.95), 10, 0),
        ("OPEN HAT", hat(K + "OPEN HAT", 0.34, oh_env(0.015, 0.085), hp_f=5500, boost=(8000, 4, 1.2),
                         metal_amp=0.55, noise_amp=0.7, scale=0.95), 30, 0),
        ("SNAP", modal(K + "SNAP", 0.16, modes=[(1850, 0.6, 0.006), (2900, 0.4, 0.004), (1200, 0.3, 0.008)],
                       click=0.8, click_band=(3000, 12000), click_tau=0.0008,
                       noise=((1500, 6000), 1.3, 0.009), room_tau=0.04, room_wet=0.3, drive=1.4), 15, 0),
        ("RIM", modal(K + "RIM", 0.12, modes=[(450, 1.0, 0.016), (1050, 0.5, 0.009), (1900, 0.35, 0.006),
                                              (3300, 0.2, 0.004)], click=0.4, click_tau=0.001,
                      noise=((2000, 6000), 0.15, 0.02), room_tau=0.02, room_wet=0.12, drive=1.2), 12, 0),
        ("TAMB", tamb(K + "TAMB", 0.25, hits=[(0.0, 1.0), (0.006, 0.55), (0.013, 0.4), (0.021, 0.28),
                                             (0.032, 0.18)]), 25, 0),
        ("HORN", horn(K + "HORN", 0.53, notes=[146.83, 220.0, 293.66, 349.23], fc_amt=4800, q=1.0), 50, 0),
    ]


def kit_lofi():
    K = "LOFI/"
    return [
        ("KICK", kick(K + "KICK", 0.32, f_start=130, f_body=55, ptau=0.02, atau=0.08, hold=0.02,
                      click=0.12, click_band=(600, 2500), click_tau=0.003, thump=0.3, drive=1.4,
                      tone_lp=1800), 25, 0.6),
        ("SNARE", snare(K + "SNARE", 0.30, modes=[(190, 0.6, 0.03), (320, 0.25, 0.02)], drop=1.1,
                        wires_band=(900, 6000), wires_amp=1.0, wires_tau=0.08, wires_hold=0.005,
                        low_amp=0.25, crack=0.15, crack_tau=0.002, swish=0.7, swish_peak=0.02,
                        swish_tau=0.07, room_tau=0.04, room_wet=0.3, drive=1.3, tone_lp=6000,
                        body_mix=0.45), 25, 0),
        ("HAT", hat(K + "HAT", 0.10, ch_env(0.018), hp_f=3800, hp_order=2, boost=(6000, 3, 1.0),
                    metal_amp=0.5, noise_amp=0.7, scale=0.85, lp_f=7500), 10, 0),
        ("SHAKER", shaker(K + "SHAKER", 0.15, band=(3000, 9000), rise=0.012, tau=0.035, start=0.25), 12, 0),
        ("RIM", modal(K + "RIM", 0.12, modes=[(480, 1.0, 0.012), (1250, 0.5, 0.008), (2300, 0.2, 0.005)],
                      click=0.15, click_band=(1500, 6000), click_tau=0.0015, room_tau=0.02, room_wet=0.12,
                      drive=1.2, tone_lp=6000), 12, 0.4),
        ("PERC", modal(K + "PERC", 0.165, modes=[(245, 1.0, 0.045), (245 * 1.52, 0.3, 0.025), (245 * 2.3, 0.12, 0.012)],
                       drop=0.15, drop_tau=0.012, click=0.0, noise=((800, 3500), 0.25, 0.004),
                       drive=1.2, tone_lp=5000), 25, 0.6),
        ("CRACKLE", crackle(K + "CRACKLE", 0.53, tick_rate=180, hiss=0.06), 60, 0),
        ("KEYS", epiano(K + "KEYS", 0.70, notes=[146.83, 174.61, 220.0, 261.63, 329.63]), 90, 1.0),
    ]


KITS = [
    ("JUNGLE", kit_jungle, None),
    ("DNB", kit_dnb, None),
    ("BOOMBAP", kit_boombap, "boombap"),
    ("LOFI", kit_lofi, "lofi"),
]
NAME_RE = re.compile(r"^[A-Z0-9 -]{1,15}$")


# ----------------------------------------------------------------------------- analysis + sheets
def analyze(q, nf):
    x = q.astype(float) / 32768.0
    n = len(x)
    pk = np.max(np.abs(x))
    X = np.abs(np.fft.rfft(x))
    f = np.fft.rfftfreq(n, 1 / FS)
    P = X ** 2
    tot = P.sum()
    band = lambda lo, hi: P[(f >= lo) & (f < hi)].sum() / tot
    lead = int(np.argmax(np.abs(x) > pk * 10 ** (-40 / 20)))
    e5 = int(0.005 * FS)
    pre = x[max(0, n - nf - int(0.01 * FS)): n - nf]
    lo_mask = (f >= 20) & (f < 500)
    return dict(
        frames=n, dur=n / FS, sectors=sectors(n),
        peak_db=20 * np.log10(pk), rms_db=20 * np.log10(np.sqrt(np.mean(x ** 2))),
        t_peak_ms=1000 * np.argmax(np.abs(x)) / FS, lead_ms=1000 * lead / FS,
        centroid=float((f * X).sum() / X.sum()),
        b_sub=band(0, 150), b_low=band(150, 1000), b_mid=band(1000, 5000), b_hi=band(5000, FS / 2),
        f_low_peak=float(f[lo_mask][np.argmax(P[lo_mask])]),
        prefade_db=20 * np.log10(max(np.max(np.abs(pre)), 1e-9) / pk) if len(pre) else float("nan"),
        end5_db=20 * np.log10(max(np.max(np.abs(x[-e5:])), 1e-9) / pk),
        last=int(q[-1]), first=int(q[0]), dc=float(np.mean(x)),
        jump_end=float(np.max(np.abs(np.diff(x[-e5:]))) / pk),
        fade_ms=1000 * nf / FS,
    )


def contact_sheet(kit, rows, path):
    fig = plt.figure(figsize=(15, 21), dpi=72)
    gs = fig.add_gridspec(8, 2, width_ratios=[1, 1.25], hspace=0.55, wspace=0.12, top=0.965, bottom=0.02)
    fig.suptitle(f"{kit}  |  total {sum(r[2]['sectors'] for r in rows)} sectors (budget {SECTOR_BUDGET})",
                 fontsize=15, y=0.985)
    for i, (label, q, m) in enumerate(rows):
        x = q.astype(float) / 32768.0
        t = np.arange(len(x)) / FS * 1000
        ax = fig.add_subplot(gs[i, 0])
        ax.plot(t, x, lw=0.5, color="#1f3b57")
        ax.set_xlim(0, t[-1])
        ax.set_ylim(-1, 1)
        ax.axhline(PEAK, color="#c44", lw=0.4, ls=":")
        ax.axhline(-PEAK, color="#c44", lw=0.4, ls=":")
        ax.set_title(f"{label}   {m['dur']*1000:.0f} ms  {m['sectors']} sec   pk {m['peak_db']:.1f} dB  "
                     f"rms {m['rms_db']:.1f} dB   cent {m['centroid']:.0f} Hz", fontsize=9, loc="left")
        ax.tick_params(labelsize=7)
        ax2 = fig.add_subplot(gs[i, 1])
        pad = np.concatenate([np.zeros(256), x, np.zeros(256)])
        ff, tt, S = signal.spectrogram(pad, FS, window="hann", nperseg=512, noverlap=480, mode="magnitude")
        tt = (tt - 256 / FS) * 1000
        SdB = 20 * np.log10(S + 1e-9)
        sel = ff >= 30
        ax2.pcolormesh(tt, ff[sel], SdB[sel], shading="auto", cmap="magma", vmin=SdB.max() - 75, vmax=SdB.max())
        ax2.set_yscale("log")
        ax2.set_ylim(30, FS / 2)
        ax2.set_xlim(0, t[-1])
        ax2.set_title(f"<150 {m['b_sub']:.2f}  150-1k {m['b_low']:.2f}  1-5k {m['b_mid']:.2f}  "
                      f">5k {m['b_hi']:.2f}   lowpk {m['f_low_peak']:.0f} Hz   prefade {m['prefade_db']:.0f} dB",
                      fontsize=9, loc="left")
        ax2.tick_params(labelsize=7)
    fig.savefig(path, bbox_inches="tight")
    plt.close(fig)


def main():
    hdr = (f"{'pad':<16}{'ms':>5}{'frm':>7}{'sec':>4}{'pk':>6}{'rms':>6}{'tpk':>6}{'lead':>5}"
           f"{'cent':>7}{'<150':>6}{'-1k':>5}{'-5k':>5}{'>5k':>5}{'lowpk':>6}{'pre':>5}{'end5':>6}"
           f"{'fade':>5}{'last':>5}{'dc':>9}")
    for kit, builder, proc in KITS:
        outdir = os.path.join(HERE, kit.lower())
        os.makedirs(outdir, exist_ok=True)
        for fn in os.listdir(outdir):
            if fn.endswith(".wav"):
                os.remove(os.path.join(outdir, fn))
        pads = builder()
        assert len(pads) == 8
        rows = []
        print(f"\n== {kit}\n{hdr}")
        for i, (pname, x, fade_ms, tonal) in enumerate(pads, start=1):
            assert NAME_RE.match(pname), pname
            if proc == "boombap":
                x = process_boombap(x, kit + pname)
            elif proc == "lofi":
                x = process_lofi(x, kit + pname, tonal)
            q, nf = finalize(x, fade_ms)
            path = os.path.join(outdir, f"{i}-{pname.replace(' ', '-')}.wav")
            wavfile.write(path, FS, q)
            m = analyze(q, nf)
            rows.append((f"{i} {pname}", q, m))
            print(f"{i} {pname:<14}{m['dur']*1000:5.0f}{m['frames']:7d}{m['sectors']:4d}{m['peak_db']:6.1f}"
                  f"{m['rms_db']:6.1f}{m['t_peak_ms']:6.1f}{m['lead_ms']:5.2f}{m['centroid']:7.0f}"
                  f"{m['b_sub']:6.2f}{m['b_low']:5.2f}{m['b_mid']:5.2f}{m['b_hi']:5.2f}{m['f_low_peak']:6.0f}"
                  f"{m['prefade_db']:5.0f}{m['end5_db']:6.0f}{m['fade_ms']:5.0f}{m['last']:5d}{m['dc']:9.1e}")
        total = sum(r[2]["sectors"] for r in rows)
        print(f"   total sectors: {total} / {SECTOR_BUDGET}")
        assert total <= SECTOR_BUDGET, f"{kit} over budget"
        contact_sheet(kit, rows, os.path.join(HERE, f"{kit.lower()}.png"))


if __name__ == "__main__":
    main()
