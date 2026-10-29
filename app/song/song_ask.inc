/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* song-mode: YES and NO, after the QY70's +1/YES and -1/NO keys. OCT+ is
 * YES and OCT- is NO on the song list, the edit jobs and the grid, where
 * they answer what is open there: a question, drawn as a box over the
 * page's middle, or a naming. What is open belongs to the page that opened
 * it. It stays until YES, NO, a change that cancels it, or leaving the page
 * (song_input). It never times out. Included by song_view.inc. */

#define SONG_NAMING_HINT "K1 K2 NAME  OCT+ DONE  OCT- CANCEL"
enum { SONG_ASK_X = 4, SONG_ASK_Y = 90, SONG_ASK_W = 232, SONG_ASK_H = 64, SONG_ASK_CHARS = 36 };
static char song_ask_line[2][SONG_ASK_CHARS + 1];
static unsigned song_open_page, song_open_naming, song_ask_warn;

/* YES (1), NO (-1) or neither (0). The presses are taken either way, so they
 * never reach the stock octave keys. Both at once is NO. */
static int song_yes_no(uint32_t *pressed)
{
    int answer = (*pressed & BUTTON(OCT_DOWN)) ? -1 : (*pressed & BUTTON(OCT_UP)) ? 1 : 0;
    *pressed &= ~(BUTTON(OCT_UP) | BUTTON(OCT_DOWN));
    return answer;
}
static unsigned song_asking(void) { return song_open_page == page && !song_open_naming; }
static unsigned song_naming(void) { return song_open_page == page && song_open_naming; }
static void song_ask_close(void) { song_open_page = song_open_naming = 0; }
static void song_name_open(void) { song_open_page = page; song_open_naming = 1; }
/* A question on this page, on one or two lines. `warn` draws it in amber. */
static void song_ask(const char *first, const char *second, unsigned warn)
{
    memset(song_ask_line, 0, sizeof song_ask_line);
    for (unsigned i = 0; i < SONG_ASK_CHARS && first[i]; ++i) song_ask_line[0][i] = first[i];
    for (unsigned i = 0; i < SONG_ASK_CHARS && second && second[i]; ++i) song_ask_line[1][i] = second[i];
    song_ask_warn = warn;
    song_open_page = page;
    song_open_naming = 0;
}
/* For the page's middle signature: the box comes and goes with it. */
static uint32_t song_ask_signature(void)
{
    return song_asking() ? signature(song_ask_warn + 1U, song_ask_line, sizeof song_ask_line) : 0;
}
static void song_ask_centered(unsigned y, const char *line, uint16_t color)
{
    text(120U - 3U * (unsigned)strlen(line), y, line, 1, color, PANEL);
}
/* Drawn last in the page's middle, over what is there. */
static void song_ask_draw(void)
{
    if (!song_asking()) return;
    unsigned two = song_ask_line[1][0] != 0, y = SONG_ASK_Y;
    paint_fill(SONG_ASK_X, y, SONG_ASK_W, SONG_ASK_H, PANEL);
    frame(SONG_ASK_X, y, SONG_ASK_W, SONG_ASK_H, song_ask_warn ? AMBER : ACCENT);
    song_ask_centered(two ? y + 12U : y + 18U, song_ask_line[0], TEXT);
    if (two) song_ask_centered(y + 24U, song_ask_line[1], song_ask_warn ? AMBER : DIM);
    text(66, y + SONG_ASK_H - 18U, "OCT+ YES", 1, ACCENT, PANEL);
    text(132, y + SONG_ASK_H - 18U, "OCT- NO", 1, TEXT, PANEL);
}
