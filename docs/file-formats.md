# Song mode file formats and transfer commands (phase 5b)

Songs and kits move between the FM-1 and a computer through the browser workbench, over the stock SysEx
transfer channel (`app/transfer.c`). The workbench saves them as two files of our own, and converts songs to
and from Standard MIDI Files. Everything is little endian. CRC-32 is the common one (IEEE, reflected,
start `0xFFFFFFFF`, final inversion), the same as the device's `pool_crc` and the stock code's `groove_crc32`.

Code: `app/song/song_file.c` (the payload on the device), `app/song/song_transfer.c` (the commands),
`web/workbench/` (the codecs, MIDI and the workbench sections), with tests in `tests/workbench_test.mjs` and
`tests/preview_5b.mjs`.

## Song file (`.fm1song`)

A 16-byte header, then the song payload exactly as the device stores it in its pool. Downloading a song and
uploading the file gives the device the same bytes back, so device to file to device is byte for byte.

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | Magic `FMSG` (`46 4D 53 47`) |
| 4 | 2 | File version, 1 |
| 6 | 2 | Flags, 0 |
| 8 | 4 | Payload length |
| 12 | 4 | CRC-32 of the payload |
| 16 | n | Payload |

### Song payload, version 1

| Size | Field |
|---|---|
| 1 | Payload version, 1 |
| 2 | Sounds block length, 1999 |
| 1999 | Sounds block: version byte 1, the 8 sound setups (223 bytes each, the stock preset: engine type at +0, program at +2), mute, solo, the 4 shared effect bytes, the arpeggiators (8 x 2), the tuning tables (8 x 24) |
| 12 | Name, printable ASCII, space padded |
| 2 | Kit id. 0 means no kit: the device keeps the kit in use |
| 8 | Each track's program as saved, plus 1 (0 when not known) |
| 2 | Bars, 1 to 999 |
| 1 + 6n | Tempo changes, 1 to 32: clock (4), BPM x 100 (2, 20.00 to 300.00). The first at clock 0, then rising |
| 1 + 4n | Time signatures, 1 to 32: bar (2, from 0), beats (1, 1 to 16), unit (1: 4, 8 or 16). The first at bar 0, then rising |
| 1 + 12n | Markers, 0 to 16: clock (4), name (8, zero padded). In clock order |
| 9 | Loop start (4), loop end (4), loop on (1). Start no later than end |
| 10 | Settings: swing (0 to 50), swing grid, swing tracks, key root (0 to 11), scale (0 to 12), recording mode (0 to 4), count-in (0 to 2), metronome (0 to 2), input quantize (0 to 14), strength (0 to 100) |
| 32 | Knob controllers, 4 per track (0 is the engine's default) |
| 1 + 5n | Glitch table entries in use: index (1, 1 to 31), attribute word (4) |
| 16 | Humanize: timing per track (8), velocity per track (8) |
| 4 | Random seed |
| per track | Event count (2), then each event: `at` (4), a (1), b (1), length (2). 4,096 events at most in all |

An event's `at` holds its clock in bits 0 to 23, its kind in bits 24 and 25 (0 note, 1 controller, 2 program
change, 3 FX lane stutter), and a note's glitch table index in bits 26 to 30. Bit 31 is always 0 in a file.
A note keeps pitch in a, velocity in b and its length in clocks (1 to 65535). A controller keeps its number in
a and value in b, a program change its program in a, both with length 0. The clock is 480 per quarter note.
Events on each track are in clock order. The device checks all of this before it stores a song, and refuses
the whole upload if anything is out of range.

Size: 2,127 bytes with one tempo, one time signature and nothing else, plus 8 per event, 6 per extra tempo,
4 per extra time signature, 12 per marker and 5 per glitch entry. On the device a song takes
ceil((64 + payload) / 4096) sectors of the pool: the 681-event test song is 7,641 bytes and 2 sectors.

## Kit file (`.fm1kit`)

An 80-byte header, then the kit as a stock sample bank. The bank is what travels to the device (stock
commands 12 to 14), so a kit file is also readable by anything that knows the stock bank.

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | Magic `FMKT` (`46 4D 4B 54`) |
| 4 | 2 | File version, 1 |
| 6 | 2 | Flags: bit 0 set when the pad settings are given |
| 8 | 16 | Name, printable ASCII, zero padded, 15 characters at most |
| 24 | 48 | Pad settings, 6 bytes per pad: level, tune, length, choke, variation, filter (zero when not given) |
| 72 | 4 | Bank length |
| 76 | 4 | CRC-32 of bytes 0 to 75, continued over the bank |
| 80 | n | Stock sample bank |

### Stock sample bank

A 512-byte header: magic `FM1S` (`0x53314D46`), version 1, total length, CRC, commit word (0), sample rate
32000, two reserved words (0), then 16 entries of 28 bytes (offset, frames, bytes, name of 16 bytes with a zero
at the end) and 32 bytes of zeros. Pads 1 to 8 are the first 8 entries. Entries 9 to 16 are empty. The audio
follows, pad after pad, each in SEA5 chunks of 228 bytes per 320 frames (starting `01 45 14 5A`). The bank CRC
is CRC-32 over the whole bank with bytes 12 to 19 (the CRC and commit words) taken as zeros.

Size: 512 bytes plus 228 per 320 frames of audio, about 22.8 KB per second at 32 kHz. On the device each
sample is its own pool extent of ceil((64 + bytes) / 4096) sectors, and kits that share a sample (same bytes,
same length) point at one copy. The workbench counts that before writing, so a kit that doesn't fit is
refused before anything is sent.

## Transfer commands 32 to 50

The stock framing carries them: `F0 7D 'F' 'M' 02 cmd token-lo token-hi`, the payload packed LSB first into
7-bit bytes, a checksum, `F7`. Requests carry up to 132 bytes, replies up to 64 after the status byte. The
status is 0 for success, 1 for nothing there, and 64 minus the error otherwise: 71 invalid, 72 out of memory,
84 the pool is full, 85 stop playback first, 86 a kit is being written. Ids are 16 bits. Writes happen only
while the device is stopped, and every write is the 5a two-copy write: the new copy goes to free space and
is committed last, and only then is the old one erased.

| Cmd | Request | Reply |
|---|---|---|
| 32 info | | version, pool sectors (2), free (2), largest free run (2), songs, kits, samples, working song id (2), kit in use (2), flags (1 playing, 2 kit being written, 4 recovery copy waiting), song sectors (2), sample sectors (2), most kits, most events (2), largest song (4) |
| 33 song list | index | id, length (4), CRC (4), name (16), or status 1 past the end |
| 34 song read begin | id | length (4), CRC (4), name (16), then zeros |
| 35 read | offset (4), count (1 to 64) | the bytes. Refused if the song or kit changed since its read began |
| 36 song write begin | id (0 for a new song), length (4) | Refused at once when the pool lacks the room |
| 37 song write | offset (4), up to 128 bytes | in order |
| 38 song write finish | CRC (4) | the id written. The payload is checked whole first |
| 39 song delete | id | |
| 40 song load | id | loads it as the working song |
| 41 kit list | index | id, name (16), pads used (bit mask), has settings, bank length (4) |
| 42 kit read begin | id | bank length (4), bank CRC (4), name (16), has settings, the 8 sample ids (2 each). Then 35 reads the bank |
| 43 kit pad settings | id | 48 bytes |
| 44 kit set | id, name (16), has settings, settings (48) | |
| 45 kit delete | id | deletes the kit, and any sample no other kit uses |
| 46 kit use | id | binds the pads, nothing is written |
| 47 kit finish | name (16), has settings, settings (48) | the new kit's id. Ends a bank sent with stock commands 12 and 13 as a library kit, without changing the kit in use |
| 48 sample list | index | id, bytes (4), frames (4), CRC (4), kits using it, name (16) |
| 49 cancel | | ends any read or write in progress |
| 50 song save | 1 to save as new | the id. Saves the working song |

The stock commands keep working. Stock 12 to 14 still write a bank, which now becomes a kit in the pool and
the kit in use, so the stock sample loader's read-back check still passes. The page's own "send to the
simulator" and "clear" go through the pool as well.

## Standard MIDI Files

Export is format 1 at 480 ticks per quarter note, so clocks carry over unchanged. Track 1 is the conductor:
the song name, a time signature at the start of each signature's bar, the tempo changes and the markers.
Tracks 2 to 9 are the FM-1 tracks 1 to 8 on MIDI channels 1 to 8, named after their engines, with the notes,
controllers and program changes, and a program change at the start when the track's saved program is known
and nothing else sits there. Every track ends at the end of the last bar. Glitch attributes, the FX lane and
humanize are left out (the workbench says how many), and stay in the song file.

Import takes format 0 and 1 files with any tick rate. Each part is a track and channel pair. Parts on
channels 1 to 8, one per channel, map to the FM-1 track of that number, as an export does. Otherwise the
first 8 parts with notes map in file order, and any track can be remapped or left empty. Tempo, time
signatures (moved to the next bar line if they fall inside one, x/2 written as 2x/4), markers (8 characters,
upper case) and the song name come across, and so do the controllers each track's engine takes and the
program changes in its range. Repeated controller values are dropped, since they change nothing. The sounds,
kit and settings come from the song file open in the workbench. Before anything is imported, the workbench
reports the events, tempo, signature and marker changes, bars and clocks against the song's limits, and
refuses the import if any is over.
