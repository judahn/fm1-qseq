# FM-1 hardware test plan

One sitting, about an hour. Nothing here is flashed without your OK. Write the numbers into the table at
the end as you go.

## Read first: flash space

The FM-1's firmware area holds at most 453,921 bytes of app. Stock Open V14 already uses 453,576, which
leaves 345 bytes. Song mode adds about 28 KB, so the full song mode can't go on the device as it is.

This test build fits (453,832 bytes) because it leaves out:
- the song view, step entry and settings pages (the browser preview still has them),
- the 16 factory preset projects (the PRESETS factory bank is empty),
- the workbench's project, sample and voice transfers. Firmware updates and the V15 restore don't use
  them, so flashing and recovery still work.

So during the test the workbench can't read or write projects or samples, and the sampler track may be
silent if its samples don't carry over from V15. Everything else plays normally.

## What you need

- A computer with desktop Chrome or Edge, and a USB data cable.
- The workbench file `build/official-web/FM1-Official-Open-Source.html`. It carries this test firmware
  and M-VAVE's original V15. Keep this exact file until the device is back on V15.

## 1. Device facts (30 seconds, on stock V15)

- [ ] The firmware version the device shows. If it isn't V15, stop here: the recovery image in the page is V15.
- [ ] Which function button (FX, SCL, ENV, LFO) sits nearest the keys. That decides SHIFT.
- [ ] The labels printed by the seven encoders.
- [ ] Whether the seventh encoder (the stock PRESETS knob) is the one next to the screen.
- [ ] Velocity: play one key softly, then hard, on a bright sound. Does it get louder?

## 2. Back up

- [ ] A new unit only has factory content. "Restore original V15" brings back the firmware and the 128
  factory voices.
- [ ] If you've made your own projects or samples, back them up with M-VAVE's own tools first. I couldn't
  confirm that the Open workbench can read them from stock V15.

## 3. Flash the test build

1. Open the HTML file in Chrome or Edge, plug in the FM-1 and switch it on. Close other MIDI apps.
2. Click **Connect device** and allow MIDI and SysEx.
3. The page sees different firmware (FM-1_015). Click **Enable force flash and reconnect**.
4. Tick the box saying you've backed up what you want to keep.
5. Click **Force flash**, accept the warning, and leave the cable in and the power on until it reboots.
6. If it stops partway, or the device sits on the update screen, keep the same page open, reconnect and
   continue as the page says.

## 4. Getting back to stock V15 (any time)

1. In the same file, click **Restore original V15**, connect, tick the backup box, then
   **Install original V15** and accept.
2. It installs V15 and restores the 128 factory voices. If it's interrupted, reconnect and retry.
3. If the device won't answer over USB at all, use M-VAVE's PC firmware tool (www.m-vave.com/download,
   "PC Firmware / FM-1 V15").

## 5. The diagnostics

Press **SEQ** to open the diagnostics page. In this build SEQ goes straight there. With the full song
pages it's the last of the device settings: SHIFT+GLO, then ALGO past the sync and MIDI pages. On the page:
- **PLAY** starts and stops the test song (12 bars, about 23 seconds, stopping by itself).
- **K1** picks the delivery method.
- **SCL** clears the numbers.
- **ENV** runs the memory probe.
- **HOME** leaves.

The three delivery methods:
- **A** (the default): the control loop hands each note over one block (2.9 ms) before it's due.
- **B**: the same, and the audio interrupt also wakes the control loop every block. The SDK doesn't say
  that wake-up call is safe from an interrupt. Its code suggests it was built for it, but it's unconfirmed.
- **C** (risky, separate on purpose): the audio interrupt posts every note itself. The SDK says that call
  is for the control task only. C needs SHIFT (FX) held while turning K1, and it's never saved, so the
  device always starts on A.

Run these in order:

1. **Control loop, stopped.** On A, wait 10 seconds. Note LOOP MS (min / average / max), BLOCKS, RUN MAX,
   and TIMER CHECK. TIMER CHECK should read about 24.0 MHz. If it doesn't, the ms figures are off by that
   ratio.
2. **Control loop, playing.** Press PLAY, wait 20 seconds and note the same rows.
3. **Lateness on A.** Stop, SCL, PLAY, and let the whole song run. Note LATE 0 / 1 / 2 / 3+, NOTES and
   MAX LATE. Listen while it plays.
4. **Lateness on B.** Stopped, turn K1 to B, then SCL, PLAY, and the whole song again. Note the same rows,
   plus OVERRUN and UNDER.
5. **Only if A and B both fall short** (notes 2 or more blocks late, or a loose feel): C. Hold FX and turn
   K1 right to C while stopped, then SCL and PLAY. Note the same rows. If the device freezes or the audio
   breaks up, switch it off and on (it comes back on A). If it won't start, go to section 4.
6. **Memory.** Stopped, press ENV. Note LARGEST FREE and the six numbers below it: the KB left with each
   engine type on track 6. Track 6 gets its usual DX7 back afterwards.
7. **Audio health.** During the A and B runs, note OVERRUN, SHORT, PEAK, RENDER / PEAK / OF, ADAPT,
   OVERLOAD, UNDER and MIDI DROP.
8. **Listening.** Play the song on the best method. Tracks 6 and 7 are muted in the default project (the
   song still plays them, and GLO's mixer unmutes them). Listen for steady time, doubled or stuck notes
   after stop, the tempo step at bar 9 (120 to 133 BPM), and the fast 64th run at the end of bar 8.

## 6. What the results mean

- **Control loop:** an average near 1 block (2.9 ms) with a max of 2 means the control loop can keep up
  on its own.
- **Keep A** if nearly every note is at LATE 0 and none is 2 or more late. It's the simplest method and the
  one the SDK supports. One block late is 2.9 ms of jitter.
- **Keep B** if A shows notes 2 or more late, B gets them to 0, and OVERRUN and UNDER stay at 0. B's
  interrupt wake-up isn't documented as safe, so if B ever glitches, it's out.
- **C** only counts if A and B both fall short. If C is tight and stable, it still relies on behavior
  M-VAVE doesn't promise.
- **Ask M-VAVE** for an interrupt-safe or timestamped note call if C is the only tight method, or if none
  are. Until then, timing-critical playback falls back to the stock step sequencer, as the design says.
- **Memory:** LARGEST FREE sets how big the song note pool can be. The engine numbers show what the
  heaviest mix of 8 sounds would cost.
- **Audio health:** any OVERRUN, UNDER or OVERLOAD while playing means the engine ran short of time with
  this load. That points at the CPU budget rather than at note delivery.

## Results

| Check | A | B | C (if run) |
|---|---|---|---|
| Loop ms min / avg / max, stopped |  |  |  |
| Loop ms min / avg / max, playing |  |  |  |
| Blocks min / avg / max, playing |  |  |  |
| Run max (ms, blocks) |  |  |  |
| Timer check (MHz) |  |  |  |
| Late 0 / 1 / 2 / 3+ |  |  |  |
| Notes, max late |  |  |  |
| Overrun, short, peak |  |  |  |
| Render, peak, of |  |  |  |
| Adapt, overload, under, MIDI drop |  |  |  |
| Sounds tight? |  |  |  |

| Memory | |
|---|---|
| Largest free (B) | |
| KB free with DX7 / 303 / 808 on track 6 | |
| KB free with PSG / pluck / sampler on track 6 | |

| Device facts | |
|---|---|
| Firmware version on arrival | |
| Function button nearest the keys | |
| Encoder labels | |
| Seventh encoder next to the screen? | |
| Keys sense velocity? | |
