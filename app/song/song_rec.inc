/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: the record settings (0.4.0, REC's mode chooser before). A
 * hold of REC on the song view, the automation view or the grid opens it,
 * HOME goes back there, and REC or PLAY goes back and does what a tap of
 * REC does there, or SHIFT+REC with STEP picked. K1 picks from REPLACE,
 * OVERDUB, PUNCH, LOOP and STEP. A live mode becomes the song's, and STEP
 * never does, so picking STEP and leaving keeps the last live mode. K2
 * sets the count-in, K3 input quantize and K4 its strength, and turning K3
 * or K4 after a take requantizes that take from its raw timing. REC
 * itself is timed here too: a tap, let go within 600 ms, acts as it comes
 * up (song_buttons.inc), and SHIFT+SEL switches the live mode between
 * REPLACE and OVERDUB. */
static uint8_t song_rec_from, song_rec_back, song_rec_pick; /* REC down on a page, the page to go back to, K1's pick */
static uint32_t song_rec_ms;
static void song_rec_go(unsigned from, unsigned step, uint32_t now); /* song_buttons.inc */
/* The list's order: the live modes, then STEP (record.h counts STEP first). */
static unsigned song_rec_listed(unsigned pick) { return (pick + 1U) % SONG_REC_MODES; }
static void song_rec_down(uint32_t now) { song_rec_from = (uint8_t)page; song_rec_ms = now; }
static void song_rec_open(unsigned from)
{
    song_rec_back = (uint8_t)from;
    song_rec_pick = (uint8_t)((song_port_song()->rec_mode + SONG_REC_MODES - 1U) % SONG_REC_MODES);
    page = music_page = SONG_REC;
    cache->shown_page = ~0U;
}
static void song_rec_toggle(uint32_t now) /* SHIFT+SEL: REPLACE and OVERDUB, and PUNCH or LOOP to OVERDUB */
{
    struct song *s = song_port_song();
    s->rec_mode = s->rec_mode == SONG_REC_OVERDUB ? SONG_REC_REPLACE : SONG_REC_OVERDUB;
    notice = s->rec_mode == SONG_REC_REPLACE ? "REC: REPLACE" : "REC: OVERDUB";
    notice_until = now + 1500U;
}

/* The last take again from its raw timing, once input quantize or its
 * strength has changed: the record settings' K3 and K4, and K2 in the song
 * view and the clip editor (0.5.2). Returns how many notes moved. */
static int song_rec_retake(unsigned changed)
{
    struct song_recorder *r = song_port_recorder();
    return changed && r->taken && !r->active ? song_rec_requantize(r) : 0;
}
static void song_quantize_name(unsigned q, char out[12])
{
    static const char names[7][5] = {"1/64", "1/32", "1/16", "1/8", "1/4", "1/2", "1/1"};
    if (!q || q > 14) { strcpy(out, "OFF"); return; }
    strcpy(out, names[(q - 1U) / 2U]);
    if (!((q - 1U) & 1U)) return;
    strcat(out, "T");
}
static void song_rec_draw(void)
{
    const struct song *s = song_port_song();
    const struct song_recorder *r = song_port_recorder();
    char line[40], part[12];
    uint32_t sig = signature(track | (uint32_t)r->taken << 8 | (uint32_t)song_rec_pick << 16, &s->rec_mode, 5);
    if (!paint_rendering || paint_top < 192) {
        if (sig != cache->middle_cache) {
            cache->middle_cache = sig;
            paint_fill(0, 0, 240, 192, BACK);
            if (paint_rendering) {
                text(8, 7, "RECORD", 2, ACCENT, BACK);
                text(184, 7, "T", 2, colors[track], BACK);
                number(198, 7, (int)track + 1, 1, 2, colors[track], BACK);
                for (unsigned i = 0; i < SONG_REC_MODES; ++i) { /* the song's live mode stays red with STEP picked */
                    unsigned y = 36 + i * 20, on = i == song_rec_pick, m = song_rec_listed(i);
                    paint_fill(6, y, 228, 17, on ? PANEL : BACK);
                    text(12, y + 5, song_rec_modes[m], 1, m == s->rec_mode ? 0xf800 : on ? AMBER : DIM, on ? PANEL : BACK);
                }
                static const char *const explain[SONG_REC_MODES] = {
                    "PLAYED NOTES REPLACE WHAT'S THERE", "PLAYED NOTES ARE ADDED", "ONLY INSIDE THE LOOP, GRID: OVERDUB",
                    "THE LOOP REPEATS, EACH PASS ADDS", "NOTES ONE STEP AT A TIME (SHIFT+REC)"};
                text(8, 140, explain[song_rec_pick], 1, TEXT, BACK);
                strcpy(line, "COUNT IN ");
                if (s->count_in) { decimal(part, s->count_in, 1); strcat(line, part); strcat(line, s->count_in > 1 ? " BARS" : " BAR"); }
                else strcat(line, "OFF");
                text(8, 156, line, 1, DIM, BACK);
                strcpy(line, "QUANTIZE ");
                song_quantize_name(s->quantize, part);
                strcat(line, part);
                if (s->quantize) { strcat(line, " AT "); decimal(part, s->strength, 1); strcat(line, part); strcat(line, "%"); }
                text(8, 168, line, 1, DIM, BACK);
                if (r->taken && !r->active) text(8, 180, "K3 K4 REQUANTIZE THE LAST TAKE", 1, AMBER, BACK);
            }
        }
    }
    if (paint_rendering && paint_top + PAINT_ROWS <= 195) return;
    char count[12], quantize[12], strength[12];
    if (s->count_in) { decimal(count, s->count_in, 1); strcat(count, " BAR"); } else strcpy(count, "OFF");
    song_quantize_name(s->quantize, quantize);
    decimal(strength, s->strength, 1);
    strcat(strength, "%");
    knob(0, "MODE", song_rec_modes[song_rec_listed(song_rec_pick)], colors[0]);
    knob(1, "COUNT IN", count, colors[1]);
    knob(2, "QUANTIZE", quantize, colors[2]);
    knob(3, "STRENGTH", strength, colors[3]);
    song_hint("REC OR PLAY TO GO  HOME GOES BACK");
}
static int song_rec_input(const struct fm1_controls *c, uint32_t *pressed, uint32_t key_down, uint32_t now)
{
    (void)key_down;
    struct song *s = song_port_song();
    if (*pressed & (BUTTON(PLAY) | BUTTON(REC) | BUTTON(HOME))) { /* back, and REC or PLAY goes */
        unsigned go = (*pressed & (BUTTON(PLAY) | BUTTON(REC))) != 0;
        *pressed &= ~(BUTTON(PLAY) | BUTTON(REC) | BUTTON(HOME));
        page = music_page = song_rec_back;
        cache->shown_page = ~0U;
        if (go) song_rec_go(song_rec_back, song_rec_listed(song_rec_pick) == SONG_REC_STEP, now);
        return 1;
    }
    *pressed &= ~(BUTTON(FX) | BUTTON(ENV) | BUTTON(SCL) | BUTTON(LFO) | BUTTON(ARP));
    if (*pressed & (BUTTON(EDIT) | BUTTON(GLO) | BUTTON(SAVE))) return 0;
    select_track(c->encoders[0]);
    song_rec_pick = (uint8_t)clamp(song_rec_pick + c->encoders[2], 0, SONG_REC_MODES - 1);
    if (song_rec_listed(song_rec_pick) != SONG_REC_STEP) s->rec_mode = (uint8_t)song_rec_listed(song_rec_pick);
    s->count_in = (uint8_t)clamp(s->count_in + c->encoders[3], 0, 2);
    unsigned quantize = s->quantize, strength = s->strength;
    s->quantize = (uint8_t)clamp(s->quantize + c->encoders[4], 0, 14);
    s->strength = (uint8_t)clamp(s->strength + c->encoders[5] * 5, 0, 100);
    (void)song_rec_retake(quantize != s->quantize || strength != s->strength);
    return 1;
}
