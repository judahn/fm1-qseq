/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: the edit job page (phase 6), after the QY70's song jobs. SHIFT+
 * EDIT in the song view opens it. K1 picks the job, K2 to K4 set what the
 * screen lists (ALGO turns to the next three, SHIFT moves by ten), and SAVE
 * runs it, asking once (the QY's ENTER). The range is the 6a selection:
 * the loop region on the picked tracks (hold LFO and tap F3 to G4), or the
 * whole song, and it is always on screen. PLAY plays, HOME or EDIT goes back,
 * HOME+SAVE undoes. The jobs themselves are in jobs_*.c. */
#include "jobs.h"

enum { JOB_QUANTIZE, JOB_VELOCITY, JOB_CRESCENDO, JOB_GATE, JOB_TRANSPOSE, JOB_FIT, JOB_SHIFT, JOB_SORT, JOB_STRUM,
       JOB_PASTE, JOB_ERASE, JOB_EXTRACT, JOB_RAMP, JOB_THIN, JOB_STRETCH, JOB_COPY, JOB_MERGE, JOB_CLEAR, JOB_NORMALIZE,
       SONG_JOBS };
enum { V_NUM, V_SIGNED, V_PCT, V_GRID, V_OFFON, V_RANGE, V_VMODE, V_GMODE, V_STEPS, V_NOTE, V_ORDER, V_TYPE, V_TRACK,
       V_LANE, V_SPACE, V_WHAT };
struct song_job_param { char name[9]; uint8_t kind; int16_t low, high, init; };
struct song_job_def { char name[14]; uint8_t count; char about[36]; struct song_job_param p[6]; };
#define RANGE_P {"RANGE", V_RANGE, 0, 1, 0}
static const struct song_job_def song_jobs[SONG_JOBS] = {
    {"QUANTIZE", 5, "NOTES TOWARD THE GRID, WITH SWING", {RANGE_P, {"GRID", V_GRID, 1, 14, 5},
        {"STRENGTH", V_PCT, 0, 100, 100}, {"SWING", V_PCT, 50, 83, 50}, {"GATE", V_OFFON, 0, 1, 0}}},
    {"VELOCITY", 4, "SET OR SCALE HOW HARD NOTES PLAY", {RANGE_P, {"MODE", V_VMODE, 0, 1, 1},
        {"VALUE", V_NUM, 0, 200, 100}, {"OFFSET", V_SIGNED, -99, 99, 0}}},
    {"CRESCENDO", 2, "VELOCITY RAMPS ACROSS THE RANGE", {RANGE_P, {"AMOUNT", V_SIGNED, -99, 99, 30}}},
    {"GATE TIME", 4, "NOTE LENGTHS, OR LEGATO", {RANGE_P, {"MODE", V_GMODE, 0, 2, 1},
        {"VALUE", V_NUM, 0, 1920, 100}, {"OFFSET", V_SIGNED, -999, 999, 0}}},
    {"TRANSPOSE", 5, "NOTES UP OR DOWN, DRUMS STAY", {RANGE_P, {"AMOUNT", V_SIGNED, -99, 99, 12},
        {"BY", V_STEPS, 0, 1, 0}, {"LOWEST", V_NOTE, 0, 127, 0}, {"HIGHEST", V_NOTE, 0, 127, 127}}},
    {"FIT TO SCALE", 1, "NOTES UP INTO THE SONG'S KEY", {RANGE_P}},
    {"SHIFT CLOCK", 2, "EVENTS LATER OR EARLIER, IN RANGE", {RANGE_P, {"CLOCKS", V_SIGNED, -9999, 9999, 0}}},
    {"CHORD SORT", 2, "A CHORD'S NOTES BY PITCH", {RANGE_P, {"ORDER", V_ORDER, 0, 1, 0}}},
    {"STRUM", 3, "SPREADS EACH CHORD, ENDS STAY", {RANGE_P, {"CLOCKS", V_NUM, 1, 999, 20},
        {"ORDER", V_ORDER, 0, 1, 0}}},
    {"PASTE N TIMES", 2, "CLIPBOARD AT THE PLAYHEAD, OVER", {{"TIMES", V_NUM, 1, 99, 2},
        {"TRANSPOSE", V_SIGNED, -24, 24, 0}}},
    {"ERASE", 2, "ONE KIND OF EVENT GOES", {RANGE_P, {"EVENTS", V_TYPE, 0, 3, 3}}},
    {"EXTRACT", 3, "ONE KIND MOVES TO ANOTHER TRACK", {RANGE_P, {"EVENTS", V_TYPE, 0, 2, 1},
        {"TO", V_TRACK, 0, 7, 7}}},
    {"RAMP", 6, "A CONTROLLER OR TEMPO SWEEP", {RANGE_P, {"LANE", V_LANE, 0, 9, 0}, {"START", V_NUM, 0, 300, 0},
        {"END", V_NUM, 0, 300, 127}, {"EVERY", V_SPACE, 0, 6, 2}, {"CURVE", V_SIGNED, -16, 16, 0}}},
    {"THIN OUT", 2, "EVERY OTHER CHANGE IN A LANE GOES", {RANGE_P, {"LANE", V_LANE, 0, 9, 0}}},
    {"TIME STRETCH", 2, "LONGER OR SHORTER, LATER BARS MOVE", {RANGE_P, {"RATE", V_PCT, 50, 200, 200}}},
    {"COPY TRACK", 3, "EVENTS, SOUND OR BOTH, REPLACING", {{"FROM", V_TRACK, 0, 7, 0}, {"TO", V_TRACK, 0, 7, 1},
        {"COPY", V_WHAT, 0, 2, 0}}},
    {"MERGE TRACKS", 2, "ONE TRACK INTO ANOTHER", {{"FROM", V_TRACK, 0, 7, 1}, {"INTO", V_TRACK, 0, 7, 0}}},
    {"CLEAR TRACK", 0, "EVERY EVENT OF THE TRACKS GOES", {{"", 0, 0, 0, 0}}},
    {"NORMALIZE", 0, "SWING, HUMANIZE, GLITCH INTO NOTES", {{"", 0, 0, 0, 0}}},
};
static unsigned song_job, song_job_page, song_job_ready;
static uint32_t song_job_armed;
static int16_t song_job_value[SONG_JOBS][6];
static const char *const song_job_grids[14] = {"1/64", "1/64T", "1/32", "1/32T", "1/16", "1/16T", "1/8", "1/8T",
                                               "1/4", "1/4T", "1/2", "1/2T", "1/1", "1/1T"};
static unsigned song_job_grid(int v) { static const uint16_t base[7] = {30, 60, 120, 240, 480, 960, 1920}; return v & 1 ? base[(v - 1) / 2] : base[(v - 1) / 2] * 2U / 3U; }
static int song_job_ranged(void) { return song_jobs[song_job].count && song_jobs[song_job].p[0].kind == V_RANGE; }
static unsigned song_drums(void)
{
    unsigned m = 0;
    for (unsigned t = 0; t < SONG_TRACKS; ++t) m |= (song_port_engine(t) == SONG_808 || song_port_engine(t) == SONG_SAMPLER) << t;
    return m;
}
/* A lane: the current track's engine parameters, then the tempo. */
static unsigned song_job_lanes(const uint8_t **index) { return song_params(song_port_engine(track), index) + 1U; }
static unsigned song_job_lane(int v)
{
    const uint8_t *index;
    unsigned n = song_job_lanes(&index), i = (unsigned)v < n ? (unsigned)v : n - 1U;
    return i == n - 1U ? (unsigned)SONG_JOB_TEMPO : song_param(index[i])->cc;
}
/* The end of everything: the last bar, or a later event. */
static uint32_t song_job_end(const struct song *s)
{
    uint32_t end = song_end(s);
    for (unsigned t = 0; t < SONG_TRACKS; ++t)
        if (s->count[t] && song_event_start(&s->event[s->first[t] + s->count[t] - 1U]) >= end)
            end = song_event_start(&s->event[s->first[t] + s->count[t] - 1U]) + 1U;
    return end;
}
static struct song_range song_job_range(void)
{
    const struct song *s = song_port_song();
    unsigned whole = !song_job_ranged() || song_job_value[song_job][0];
    return (struct song_range){whole ? 0 : s->loop_start, whole ? song_job_end(s) : s->loop_end, (uint8_t)song_selected(),
                               (uint8_t)song_drums()};
}
static void song_job_text(unsigned p, char out[16])
{
    const struct song_job_param *d = &song_jobs[song_job].p[p];
    int v = song_job_value[song_job][p];
    static const char *const words[][4] = {{"OFF", "ON"}, {"SELECTION", "WHOLE SONG"}, {"SET", "RATE"}, {"SET", "RATE", "LEGATO"},
        {"SEMITONES", "SCALE STEPS"}, {"LOW-HIGH", "HIGH-LOW"}, {"NOTES", "CCS", "PROGRAMS", "ALL"}, {"EVENTS", "SOUND", "BOTH"}};
    const uint8_t which[] = {[V_OFFON] = 1, [V_RANGE] = 2, [V_VMODE] = 3, [V_GMODE] = 4, [V_STEPS] = 5, [V_ORDER] = 6,
                             [V_TYPE] = 7, [V_WHAT] = 8, [V_WHAT + 1] = 0};
    out[0] = 0;
    if (d->kind < sizeof which && which[d->kind]) { strcpy(out, words[which[d->kind] - 1U][v]); return; }
    if (d->kind == V_GRID) strcpy(out, song_job_grids[v - 1]);
    else if (d->kind == V_SPACE) strcpy(out, song_job_grids[2 * v]);
    else if (d->kind == V_TRACK) { out[0] = 'T'; decimal(out + 1, v + 1, 1); }
    else if (d->kind == V_NOTE) { strcpy(out, song_key_names[v % 12]); decimal(out + strlen(out), v / 12 - 1, 1); }
    else if (d->kind == V_LANE) {
        unsigned lane = song_job_lane(v);
        const struct song_param *q = lane == SONG_JOB_TEMPO ? NULL : song_param_for(song_port_engine(track), lane);
        strcpy(out, q ? q->name : "TEMPO");
    } else {
        if (d->kind == V_SIGNED && v > 0) strcpy(out, "+");
        decimal(out + strlen(out), v, 1);
        if (d->kind == V_PCT || (song_job == JOB_VELOCITY && p == 2 && song_job_value[song_job][1]) ||
            (song_job == JOB_GATE && p == 2 && song_job_value[song_job][1] == SONG_JOB_RATE)) strcat(out, "%");
    }
}
/* Where it works, for the line under the name. */
static void song_job_where(char out[40])
{
    const struct song *s = song_port_song();
    const struct song_range r = song_job_range();
    char part[12];
    unsigned at[3];
    out[0] = 0;
    if (song_job == JOB_PASTE) {
        if (!song_port_clip()->length) { strcpy(out, "COPY SOMETHING FIRST (SHIFT+F3)"); return; }
        song_bbc(s, song_port_player()->playhead, at);
        strcpy(out, "AT BAR "); decimal(part, (int)at[0], 1); strcat(out, part);
        strcat(out, "."); decimal(part, (int)at[1], 1); strcat(out, part);
        return;
    }
    if (song_job == JOB_COPY || song_job == JOB_MERGE) { strcpy(out, "WHOLE TRACKS, AS BELOW"); return; }
    if (r.from == 0 && r.to == song_job_end(s)) strcpy(out, song_job_ranged() ? "WHOLE SONG" : "WHOLE TRACKS");
    else {
        song_bbc(s, r.from, at);
        strcpy(out, "BARS "); decimal(part, (int)at[0], 1); strcat(out, part); strcat(out, "."); decimal(part, (int)at[1], 1); strcat(out, part);
        song_bbc(s, r.to, at);
        strcat(out, " TO "); decimal(part, (int)at[0], 1); strcat(out, part); strcat(out, "."); decimal(part, (int)at[1], 1); strcat(out, part);
    }
    if (r.tracks == 0xff) strcat(out, "  ALL");
    else for (unsigned t = 0; t < SONG_TRACKS && strlen(out) < 34; ++t)
        if (r.tracks >> t & 1U) { strcat(out, " T"); decimal(part, (int)t + 1, 1); strcat(out, part); }
}
#include "song_jobs_page.inc"
