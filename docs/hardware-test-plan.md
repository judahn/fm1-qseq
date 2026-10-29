# FM-1 hardware test plan

One sitting, about an hour. Nothing here is flashed without your OK. Write the numbers into the table at
the end as you go.

**Run once (10/28), on the test build.** It chose delivery method B. The numbers are in the results at the
end. The steps below are kept for running it again: the test build still has the diagnostics page, with A
and B. Method C, which the first run didn't need, is gone.

## Read first: flash space

The FM-1's firmware area holds at most 453,921 bytes of app. Stock Open V14 already uses 453,576, which
leaves 345 bytes. Song mode adds about 28 KB, so the full song mode can't go on the device as it is.

This test build fits (453,480 bytes) because it leaves out:
- the song view, step entry and settings pages (the browser preview still has them),
- the 16 factory preset projects (the PRESETS factory bank is empty),
- the workbench's project, sample and voice transfers. Firmware updates and the V15 restore don't use
  them, so flashing and recovery still work.

So during the test the workbench can't read or write projects or samples, and the sampler track may be
silent if its samples don't carry over from V15. Everything else plays normally.

### The opt-in full build

The full song mode build (`FM1_SONG_DEVICE_FULL=1`) is a separate test that needs your OK. It has every song
page, and it makes room by giving 23 of the 131 storage sectors (92 KB) to code: the 15 stock project slots
(sectors 0 to 14) and the first 8 sectors of the sample area (17 to 24). Its app limit is 548,129 bytes, and it
uses 520,680 (0.2.0). It delivers by method B and has no diagnostics page.

What it means for the data on the device:
- Sector 25 on keeps its place: the song and kit pool (25 to 125, about 404 KB), the DX7 voice banks (126 to
  129) and the tuning and session sector (130).
- The settings sector (15) and the staging sector (16) move, so the MIDI, clock and Bluetooth settings start
  from their defaults.
- Stock projects are overwritten, and the stock sample bank can't stay, because it starts at sector 17. Kits
  come from the workbench, which keeps its sample transfers in this build. Its "Send the built-in kit" puts
  FM1 Open's 8 built-in sounds back as a kit (88 KB).
- The SDK's settings area (VM) takes the space between the code and the Bluetooth sector, 60 KB at this size
  instead of 32 KB, and the Bluetooth sector moves. Both are untested on hardware.

## What you need

- A computer with desktop Chrome or Edge, and a USB data cable.
- The workbench file `build/official-web/FM1-Official-Open-Source.html`. It carries this test firmware
  and M-VAVE's original V15. Keep this exact file until the device is back on V15.

## 1. Device facts (30 seconds, on stock V15)

- [ ] The firmware version the device shows. If it isn't V15, stop here: the recovery image in the page is V15.
- [ ] Which function button (FX, SEL, ENV, LFO) sits nearest the keys. That decides SHIFT.
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

Press **SEQ** to open the diagnostics page. In this build SEQ goes straight there, and SEQ again goes to the
stock step page. On the page:
- **PLAY** starts and stops the test song (12 bars, about 23 seconds, stopping by itself).
- **K1** picks the delivery method, A or B, while stopped. The page starts on B.
- **SEL** clears the numbers.
- **ENV** runs the memory probe, **LFO** just the largest free block.
- **HOME** goes back to the stock overview.

The two delivery methods:
- **A**: the control loop hands each note over one block (2.9 ms) before it's due, at its own pace.
- **B** (the one the first run chose): the same, and the audio interrupt also wakes the control loop after
  every block while the song plays. The SDK doesn't say that wake-up call is safe from an interrupt. Its
  code suggests it was built for it, and the first run had no trouble with it.

Run these in order:

1. **Control loop, stopped.** Wait 10 seconds. Note LOOP MS (min / average / max), BLOCKS, RUN MAX,
   and TIMER CHECK. TIMER CHECK should read about 24.0 MHz. If it doesn't, the ms figures are off by that
   ratio.
2. **Control loop, playing.** Press PLAY, wait 20 seconds and note the same rows.
3. **Lateness on A.** Stopped, turn K1 to A, then SEL, PLAY, and let the whole song run. Note LATE
   0 / 1 / 2 / 3+, NOTES and MAX LATE. Listen while it plays.
4. **Lateness on B.** Stopped, turn K1 to B, then SEL, PLAY, and the whole song again. Note the same rows,
   plus OVERRUN and UNDER.
5. **Memory.** Stopped, press ENV. Note LARGEST FREE, the heap line under it and the six numbers below: the
   KB left with each engine type on track 6. Track 6 gets its usual DX7 back afterwards. A number with "+"
   (or "OR MORE") reached the top of the probe's search, so it's that much or more. If the page says MALLOC
   GAVE A BLOCK OUTSIDE THE HEAP, malloc handed out memory past the heap the link map gives, and there's no
   result. The probe never writes to a block it gets.
6. **Audio health.** During the A and B runs, note OVERRUN, SHORT, PEAK, RENDER / PEAK / OF, ADAPT,
   OVERLOAD, UNDER and MIDI DROP.
7. **Listening.** Play the song on B. Tracks 6 and 7 are muted in the default project (the song still plays
   them, and GLO's mixer unmutes them). Listen for steady time, doubled or stuck notes after stop, the tempo
   step at bar 9 (120 to 133 BPM), and the fast 64th run at the end of bar 8.

## 6. What the results mean

- **Control loop:** an average near 1 block (2.9 ms) with a max of 2 would mean the control loop keeps up
  on its own. The first run measured about 3.7 blocks, so it doesn't.
- **A** would do if nearly every note were at LATE 0 and none 2 or more late. One block late is 2.9 ms of
  jitter.
- **B** is right if A shows notes 2 or more late, B gets them to 0 or 1, and OVERRUN and UNDER stay at 0.
  That's what the first run found. If B ever glitches, it's out, and the question for M-VAVE is an
  interrupt-safe or timestamped note call.
- **Memory:** LARGEST FREE sets how big the song note pool can be. The engine numbers show what the
  heaviest mix of 8 sounds would cost.
- **Audio health:** any OVERRUN, UNDER or OVERLOAD while playing means the engine ran short of time with
  this load. That points at the CPU budget rather than at note delivery.

## Results

First run, 10/28, on the test build, from the photos of the page. B was chosen.

Control loop (one reading):

| Check | Result |
|---|---|
| Loop ms min / avg / max | 9.9 / 10.8 / 12.1 |
| Blocks min / avg / max | 3 / 3.7 / 5 |
| Run max | 13 ms |
| Timer check | 24.0 MHz |

The test song, a whole run on each method:

| Check | A | B |
|---|---|---|
| Late 0 / 1 / 2 / 3+ | 184 / 200 / 230 / 162 | 1,312 / 49 / 0 / 0 |
| Notes, max late | 774, 4 blocks | 1,361, 1 block |
| Overrun, short | 0, 0 | 0, 0 |
| Render, peak, of | 1,508, 36,722, 69,659 | 1,508, 36,722, 69,659 |
| Adapt, overload, under, MIDI drop | up 23 (VOICES ADAPTED), 0, 0, 0 | up 23 (VOICES ADAPTED), 0, 0, 0 |
| Sounds tight? | | Yes, clean |

A's four LATE counts add to 776, two more than its NOTES, as read from the photo.

| Memory | |
|---|---|
| Largest free | Inconclusive: 524,272 bytes, the top of the probe's search, while the link map gives the heap about 213 KB. malloc didn't fail when it ran out. The probe now counts only blocks inside the heap and reads "or more" at the top |
| KB free with each engine on track 6 | Inconclusive: 513.9 for every engine, the same ceiling |

| Device facts | |
|---|---|
| Button names | As the code has them, except the one the hints called SCL is printed SEL. The hints now say SEL |
