/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: REC's mode chooser. REC in the song view opens it, and REC or
 * PLAY goes: step entry, or a real-time take with its count-in. K1 picks
 * the mode, K2 the count-in, K3 input quantize and K4 its strength. Turning
 * K3 or K4 after a take requantizes that take from its raw timing. */

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
    uint32_t sig = signature(track | (uint32_t)r->taken << 8, &s->rec_mode, 5);
    if (!paint_rendering || paint_top < 192) {
        if (sig != cache->middle_cache) {
            cache->middle_cache = sig;
            paint_fill(0, 0, 240, 192, BACK);
            if (paint_rendering) {
                text(8, 7, "RECORD", 2, ACCENT, BACK);
                text(184, 7, "T", 2, colors[track], BACK);
                number(198, 7, (int)track + 1, 1, 2, colors[track], BACK);
                for (unsigned m = 0; m < SONG_REC_MODES; ++m) {
                    unsigned y = 36 + m * 20, on = m == s->rec_mode;
                    paint_fill(6, y, 228, 17, on ? PANEL : BACK);
                    text(12, y + 5, song_rec_modes[m], 1, on ? (m ? 0xf800 : AMBER) : DIM, on ? PANEL : BACK);
                }
                static const char *const explain[SONG_REC_MODES] = {
                    "ENTER NOTES ONE STEP AT A TIME", "PLAYED NOTES REPLACE WHAT'S THERE",
                    "PLAYED NOTES ARE ADDED", "RECORDS ONLY INSIDE THE LOOP", "THE LOOP REPEATS, EACH PASS ADDS"};
                text(8, 140, explain[s->rec_mode], 1, TEXT, BACK);
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
    knob(0, "MODE", song_rec_modes[s->rec_mode], colors[0]);
    knob(1, "COUNT IN", count, colors[1]);
    knob(2, "QUANTIZE", quantize, colors[2]);
    knob(3, "STRENGTH", strength, colors[3]);
    if (cache->hint_cache != 0x7ecU) {
        cache->hint_cache = 0x7ecU;
        paint_fill(6, 231, 228, 7, BACK);
        text(6, 231, "REC OR PLAY TO GO  HOME TO LEAVE", 1, DIM, BACK);
    }
}
static int song_rec_input(const struct fm1_controls *c, uint32_t *pressed, uint32_t key_down, uint32_t now)
{
    (void)key_down;
    struct song *s = song_port_song();
    struct song_recorder *r = song_port_recorder();
    if (*pressed & (BUTTON(PLAY) | BUTTON(REC))) {
        *pressed &= ~(BUTTON(PLAY) | BUTTON(REC));
        if (s->rec_mode == SONG_REC_STEP) { song_step_enter(); return 1; }
        page = music_page = SONG;
        cache->shown_page = ~0U;
        if (song_port_record(s->rec_mode, track)) { notice = "STOP FIRST"; notice_until = now + 1500U; }
        return 1;
    }
    *pressed &= ~(BUTTON(FX) | BUTTON(ENV) | BUTTON(SCL) | BUTTON(LFO) | BUTTON(ARP));
    if (*pressed & (BUTTON(HOME) | BUTTON(EDIT) | BUTTON(GLO) | BUTTON(SAVE))) return 0;
    select_track(c->encoders[0]);
    s->rec_mode = (uint8_t)clamp(s->rec_mode + c->encoders[2], 0, SONG_REC_MODES - 1);
    s->count_in = (uint8_t)clamp(s->count_in + c->encoders[3], 0, 2);
    unsigned quantize = s->quantize, strength = s->strength;
    s->quantize = (uint8_t)clamp(s->quantize + c->encoders[4], 0, 14);
    s->strength = (uint8_t)clamp(s->strength + c->encoders[5] * 5, 0, 100);
    if ((quantize != s->quantize || strength != s->strength) && r->taken && !r->active) song_rec_requantize(r);
    return 1;
}
