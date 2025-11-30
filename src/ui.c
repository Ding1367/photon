#include "ui.h"
#include "photon_debug.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "photon.h"
#include <termios.h>
#include <sys/ioctl.h>
#include <math.h>
#include <ctype.h>

typedef struct ansi_seq {
    unsigned int P[32];
    unsigned char num_params;
    char ch;
    char opt_flags;
} ansi_seq_t;

#define OPT_REMOVE_TRAILING_1 1
#define OPT_REMOVE_TRAILING_0 2

static int _ui_buf_eval(const ansi_seq_t *seq, char *buf, int n);
static int __ui_buf_put(const ansi_seq_t *seq
    PHOTON_DEBUG_OPT(, const char *fname)
);

#if PHOTON_DEBUG
#define _ui_buf_put(seq) __ui_buf_put((seq), __func__)

static int frame_number;
#else
#define _ui_buf_put(seq) __ui_buf_put((seq))
#endif

#ifdef UI_DEBUG_REFRESH
#define REC_REFRESH(...) PHOTON_DEBUG_OPT(photon_debug_record(PHOTON_UI_REFRESH_NUM, __VA_ARGS__))
#else
#define REC_REFRESH(...)
#endif
#ifdef UI_DEBUG_CALLS
#define REC_CALLS(...) PHOTON_DEBUG_OPT(photon_debug_record(PHOTON_UI_CALLS_NUM, __VA_ARGS__))
#else
#define REC_CALLS(...)
#endif
#define REC_BROADCAST(...) REC_CALLS(__VA_ARGS__); REC_REFRESH(__VA_ARGS__);

#define _ui_buf_calc_len(seq) _ui_buf_eval((seq), 0, 0)
#define _ui_buf_put_shorter(first, ...) _ui_put_shorter(first, __VA_ARGS__, NULL)

#define err_and_ret(edit, err, val) edit->error = err; return val;

static int cap;
static int top;
static char *buf;
static char evalBuf[512];
static struct termios old;

#define INITIAL_CAPACITY 256

static int c_y, c_x;

static int _ui_put_shorter(const ansi_seq_t *seq, ...){
    va_list va;
    va_start(va, seq);

    const ansi_seq_t *shortest = seq;
    int len = _ui_buf_calc_len(seq);
    
    while (1){
        const ansi_seq_t *seq = va_arg(va, ansi_seq_t *);
        if (!seq) break;
        int curLen = _ui_buf_calc_len(seq);
        if (curLen < len){
            len = curLen;
            shortest = seq;
        }
    }

    va_end(va);

    return _ui_buf_put(shortest);
}

static void _ui_pap(char **buf, int *total, int *n, const char *fmt, ...){
    va_list va;
    va_start(va, fmt);

    if (*n < 0) {
        *n = 0;
    }

    int count = vsnprintf(*buf, *n, fmt, va);
    *buf += count;
    *n -= count;
    *total += count;

    va_end(va);
}
static int _ui_buf_eval(const ansi_seq_t *seqPtr, char *buf, int n){
    ansi_seq_t seq = *seqPtr;
    if (seq.opt_flags){       
        while (seq.num_params){
            int p = seq.P[seq.num_params - 1];
            if ((p == 1 && (seq.opt_flags & OPT_REMOVE_TRAILING_1)) || (p == 0 && (seq.opt_flags & OPT_REMOVE_TRAILING_0)))
                seq.num_params--;
            else break;
        }
    }
    int totalLength = 0;
    _ui_pap(&buf, &totalLength, &n, "\x1b[");
    if (seq.num_params){
        _ui_pap(&buf, &totalLength, &n, "%u", seq.P[0]);
        for (int i = 1; i < seq.num_params; i++)
            _ui_pap(&buf, &totalLength, &n, ";%u", seq.P[i]);
    }
    _ui_pap(&buf, &totalLength, &n, "%c", seq.ch);
    return totalLength;
}
static int __ui_buf_put(const ansi_seq_t *seq
    PHOTON_DEBUG_OPT(, const char *fname)
){
    int len = _ui_buf_eval(seq, evalBuf, sizeof(evalBuf));
    int newCapacity = cap;
    while (newCapacity < len + top)
        newCapacity <<= 1;
    if (newCapacity != cap){
        char *newBuffer = realloc(buf, newCapacity);
        if (!newBuffer)
            return 0;
        buf = newBuffer;
        cap = newCapacity;
    }
    REC_REFRESH("%s(): %.*s\n", fname, len, evalBuf);
    memcpy(&buf[top], evalBuf, len);
    top += len;
    return 1;
}
static int _ui_buf_putch(char c){
    if (cap == top){
        char *newBuffer = realloc(buf, cap << 1);
        if (!newBuffer)
            return 0;
        buf = newBuffer;
        cap <<= 1;
    }
    buf[top++] = c;
    return 1;
}
static int _ui_buf_flush(void){
    int ntotal = 0;
    while (ntotal != top){
        int n = write(STDOUT_FILENO, &buf[ntotal], top - ntotal);
        if (n == -1)
            return -1;
        ntotal += n;
    }
    top = 0;
    return ntotal;
}

typedef struct {
    float h, s, v;
} hsv_t;

typedef struct {
    uint8_t r, g, b;
} rgb_t;

static int has_true_color;
static int has_color_;
static int is_4bit_color;
static hsv_t palette[16];

static struct {
    int y, x;
    int fg, bg;
} state;

static void _to_hsv(uint8_t r8, uint8_t g8, uint8_t b8, hsv_t *hsv){
    float r = r8/255.0f;
    float g = g8/255.0f;
    float b = b8/255.0f;
    float h, s;
    float max, min;
    max = r > g ? r : g;
    max = max > b ? max : b;
    min = r > g ? g : r;
    min = min > b ? b : min;
    float range = max - min;
    if (max == 0)
        s = 0.0f;
    else
        s = range / max;
    if (range == 0.0f) {
        h = 0.0f;
    } else if (max == r) {
        h = 60.0f * fmodf(((g - b) / range), 6.0f);
    } else if (max == g) {
        h = 60.0f * (((b - r) / range) + 2.0f);
    } else {
        h = 60.0f * (((r - g) / range) + 4.0f);
    }
    hsv->h = h;
    hsv->s = s;
    hsv->v = max;
}

static float _hsv_cmp(const hsv_t *c1, const hsv_t *c2) {
    float dh = fabsf(c1->h - c2->h);
    if (dh > 180.0f) dh = 360.0f - dh;
    dh /= 180.0f;

    float ds = fabsf(c1->s - c2->s);
    float dv = fabsf(c1->v - c2->v);

    const float wH = 0.5f;
    const float wS = 0.25f;
    const float wV = 0.25f;

    return sqrtf(wH * dh * dh + wS * ds * ds + wV * dv * dv);
}

static hsv_t defined_range[240];
static rgb_t defined_range_rgb[240];

#define hsv(x,y,z) (hsv_t){ (float)(x), (float)(y), (float)z }

int has_color(void){
    return has_color_;
}

static void _ui_set_color(ansi_seq_t *seq, int color, int bg){
    int *p = (bg ? &state.bg : &state.fg);
    int value = *p;
    int r = (color >> 16) & 0xff;
    int g = (color >> 8)  & 0xff;
    int b =  color        & 0xff;
    if (!has_color_){
        if (bg){
            hsv_t hsv;
            _to_hsv(r, g, b, &hsv);
            if (hsv.v > 0.5){
                seq->P[seq->num_params++] = 7;
            }
        }
        return;
    }
    if (has_true_color){
        if (value == color) return;
        rgb_t val;
        val.r = r;
        val.g = g;
        val.b = b;
        // check if it's in the defined range, and if so just use the index
        for (int i = 0; i < 240; i++){
            if (memcmp(&val, &defined_range_rgb[i], sizeof(rgb_t)) == 0){
                seq->P[seq->num_params++] = 38 + bg * 10;
                seq->P[seq->num_params++] = 5;
                seq->P[seq->num_params++] = i + 16;
                *p = color;
                return;
            }
        }
        seq->P[seq->num_params++] = 38 + bg * 10;
        seq->P[seq->num_params++] = 2;
        seq->P[seq->num_params++] = r;
        seq->P[seq->num_params++] = g;
        seq->P[seq->num_params++] = b;
        *p = color;
        return;
    }
    // grayscale;
    hsv_t hsv;
    _to_hsv(r, g, b, &hsv);
    if (hsv.s <= 0.01){
        // grayscale
        if (is_4bit_color){
            // black, dark gray, very light gray, or white?
            // is it closer to black?
            if (hsv.v < 0.25){
                seq->P[seq->num_params] = 30;
            }
            // is it closer to dark gray?
            else if (hsv.v < 0.5){
                seq->P[seq->num_params] = 90;
            }
            // is it closer to light gray?
            else if (hsv.v < 0.75){
                seq->P[seq->num_params] = 37;
            }
            // white
            else seq->P[seq->num_params] = 97;

            if (bg)
                seq->P[seq->num_params] += 10;
            if (value == seq->P[seq->num_params])
                *p = seq->P[seq->num_params++];
        } else {
            int closest;
            float closeness = INFINITY;
            for (int i = 0; i < 24; i++){
                float f;
                if ((f = fabs(hsv.v - defined_range[216 + i].v)) < closeness){
                    closeness = f;
                    closest = i;
                }
            }
            int x = 216 + closest % 8;
            if (value == x) return;
            *p = x;
            seq->P[seq->num_params++] = 38 + bg * 10;
            seq->P[seq->num_params++] = 5;
            seq->P[seq->num_params++] = x + 16;
        }
        return;
    }
    if (is_4bit_color){
        int closest;
        float closeness = INFINITY;
        for (int i = 0; i < 16; i++){
            float cmp = _hsv_cmp(&hsv, &palette[i]);
            if (cmp < closeness){
                closest = i;
                closeness = cmp;
            }
        }
        int x = ((closest < 8) ? 30 : 90) + bg * 10 + closest % 8;
        if (value == x) return;
        seq->P[seq->num_params++] = x;
        *p = x;
        return;
    }
    int closest;
    float closeness = INFINITY;
    for (int i = 0; i < 240; i++){
        float cmp = _hsv_cmp(&hsv, &defined_range[i]);
        if (cmp < closeness){
            closest = i;
            closeness = cmp;
        }
    }
    if (value == closest) return;
    seq->P[seq->num_params++] = 38 + bg * 10;
    seq->P[seq->num_params++] = 5;
    seq->P[seq->num_params++] = closest + 16;
    *p = closest;
}

typedef struct ui_cell {
    int fg, bg;
    char style;
    char ch;
} ui_cell_t;

static ui_cell_t *front, *back;
static int did_resize;
static int rows, cols;

static void _ui_get_size(void){
    struct winsize sz;
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &sz) == -1){
        did_resize = -1;
        return;
    }
    did_resize = 1;
    rows = sz.ws_row;
    cols = sz.ws_col;
}

int photon_ui_init(photon_editor_t *editor){
#ifdef UI_DEBUG_CALLS
    photon_debug_begin_rec(PHOTON_UI_CALLS_NUM, UI_DEBUG_CALLS);
#endif
#ifdef UI_DEBUG_REFRESH
    photon_debug_begin_rec(PHOTON_UI_REFRESH_NUM, UI_DEBUG_REFRESH);
#endif
    REC_BROADCAST("Frame #%d\n", frame_number);
    // set impossible state color values
    state.fg = state.bg = -1;

    for (int i = 0; i < 216; i++){
        uint8_t r, g, b;
        r = i / 36 * 51;
        g = (i / 6) % 36 * 51;
        b = i % 6 * 51;
        defined_range_rgb[i] = (rgb_t){
            .r = r,
            .g = g,
            .b = b
        };
        _to_hsv(r, g, b, &defined_range[i]);
    }
    for (int i = 0; i < 24; i++){
        uint8_t r, g, b;
        r = g = b = 8 + 10 * i;
        defined_range_rgb[i + 216] = (rgb_t){
            .r = r,
            .g = g,
            .b = b
        };
        _to_hsv(r, g, b, &defined_range[i + 216]);
    }

    // approx.
    palette[0] = hsv(0,0,0);
    palette[1] = hsv(0,1,.6);
    palette[2] = hsv(120,1,.651);
    palette[3] = hsv(60,1,.6);
    palette[4] = hsv(240,1,.702);
    palette[5] = hsv(300,1,.702);
    palette[6] = hsv(184,1,.702);
    palette[7] = hsv(0,0,.749);

    palette[8] = hsv(0,0,.4);
    palette[9] = hsv(0,1,.898);
    palette[10] = hsv(120,1,.851);
    palette[11] = hsv(60,1,.898);
    palette[12] = hsv(240,1,1);
    palette[13] = hsv(300,1,.898);
    palette[14] = hsv(184,1,.898);
    palette[15] = hsv(0,0,.898);

#ifdef _WIN32
#error "Windows is not supported yet"
#else
    char *s = getenv("COLORTERM");
    has_true_color = s && strcmp(s, "truecolor") == 0;
    if (!has_true_color){
        char *s = getenv("TERM");
        has_color_ = strncmp(s, "xterm", 5) == 0 || strcmp(s, "ansi") == 0 || strstr(s, "color") != NULL;
        is_4bit_color = strstr(s, "256") == NULL;
    } else has_color_ = has_true_color;
#endif

    buf = malloc(INITIAL_CAPACITY);
    if (buf == NULL) {
        err_and_ret(editor, PHOTON_NO_MEM, 0);
    }
    cap = INITIAL_CAPACITY;

    printf("\x1b[?1049h\x1b[2J\x1b[H");
    fflush(stdout);
    tcgetattr(STDIN_FILENO, &old);
    struct termios raw = old;
    cfmakeraw(&raw);
    tcsetattr(STDIN_FILENO, TCSADRAIN, &raw);

    _ui_get_size();
    did_resize = 0;
    int area = rows * cols;
    back = calloc(area, sizeof(ui_cell_t));
    front = calloc(area, sizeof(ui_cell_t));
    if (!back || !front){
        free(back);
        free(front);
        back = front = NULL;
        free(buf);
        buf = NULL;
        err_and_ret(editor, PHOTON_NO_MEM, 0);
    }
    return 1;
}

static void photon_request(photon_editor_t *editor, photon_draw_req_t *req){
    if (editor->pre_draw){
        if (!editor->pre_draw(req))
            return;
    }
    if (req->ch == 0) return;
    ui_cell_t cell;
    cell.bg = req->bg;
    cell.fg = req->fg;
    cell.ch = req->ch;
    cell.style = req->style;
    back[req->y * cols + req->x] = cell;
}

void photon_move_ui_cursor(int y, int x){
    c_y = y;
    c_x = x;
    REC_CALLS("cursor moved to %d, %d\n", y, x);
}

void photon_ui_cursor_loc(int *y, int *x){
    if (y)
        *y = c_y;
    if (x)
        *x = c_x;
}

void photon_draw_str(photon_editor_t *editor, const char *str){
    photon_draw_nstr(editor, str, strlen(str));
}

#define TRUNC_LEN 32

void photon_draw_nstr(photon_editor_t *editor, const char *str, size_t sz){
#ifdef UI_DEBUG_CALLS
    photon_draw_req_t req = {0};
    char trunc[TRUNC_LEN + 10] = {0};
    if (sz > TRUNC_LEN){
        sprintf(trunc, "%.32s...", str);
    } else {
        memcpy(trunc, str, sz > TRUNC_LEN ? TRUNC_LEN : sz);
    }
    REC_CALLS("attempting to draw string \"%.*s\" size of %zu bytes with color #%06x\n", sz > TRUNC_LEN ? TRUNC_LEN : (int)sz, trunc, sz, editor->ui_hints.fg);
#endif
    while (sz){
        int p = c_y * cols + c_x;
        req.style = editor->ui_hints.style;
        req.fg = editor->ui_hints.fg;
        req.bg = back[p].bg;
        req.ch = *str++;
        req.x = c_x;
        req.y = c_y;
        photon_request(editor, &req);
        if (++c_x == cols){
            c_x = 0;
            c_y++;
        }
        sz--;
    }
    // do the end of string as well
    int p = c_y * cols + c_x;
    req.ch = 0;
    photon_request(editor, &req);
}

void photon_draw_box(photon_editor_t *editor, int rows, int cols){
    photon_draw_req_t req = {0};
    REC_CALLS("attempting to draw box size of %dx%d, with color #%06x\n", cols, rows, editor->ui_hints.bg);
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++){
            int p = (c_y + r) * cols + (c_x + c);
            req.fg = back[p].fg;
            req.bg = editor->ui_hints.bg;
            req.style = back[p].style;
            req.ch = back[p].ch ? back[p].ch : ' ';
            req.x = c_x + c;
            req.y = c_y + r;
            photon_request(editor, &req);
        }
}

void photon_ui_clear(void){
    memset(back, 0, rows * cols * sizeof(ui_cell_t));
}

static void _ui_gen_ar_pair(char c, ansi_seq_t *abs, ansi_seq_t *rel, int diff, int v){
    ansi_seq_t moveAbs = {0};
    moveAbs.num_params = 1;
    moveAbs.opt_flags = OPT_REMOVE_TRAILING_1;
    if (abs){
        moveAbs.ch = c;
        moveAbs.P[0] = v + 1;
        *abs = moveAbs;
    }
    
    if (rel){
        ansi_seq_t moveRel = moveAbs;
        moveRel.num_params = 1;
        char relCh;
        if (c == 'H'){
            relCh = diff < 0 ? 'A' : 'B';
        } else {
            relCh = diff < 0 ? 'D' : 'C';
        }
        moveRel.ch = relCh;
        moveRel.P[0] = diff < 0 ? -diff : diff;
        *rel = moveRel;
    }
}

static void _ui_move_cursor(unsigned int y, unsigned int x){
    REC_REFRESH("_ui_move_cursor(%u, %u) called, cursor already at %u, %u\n", y, x, state.y, state.x);
    int diffX = (int)(x - state.x);
    int diffY = (int)(y - state.y);
    if (!diffX && !diffY) return;
    if (y == 0 && x == 0){
        // just stop wasting time and GO HOME.
        ansi_seq_t moveAbs = {0};
        moveAbs.ch = 'H';
        _ui_buf_put(&moveAbs);
        state.x = state.y = 0;
        return;
    }
    // \r is the cheapest way to go to the first column since its a single byte
    if (x == 0 && diffX){
        _ui_buf_putch('\r');
        diffX = 0;
        state.x = 0;
    }
    if (x == state.x - 1){
        _ui_buf_putch('\b');
        diffX = 0;
        state.x = x;
    }
    if (y - state.y <= 4 && diffY > 0){
        // literally move down a line
        for (int i = 0; i < diffY; i++)
            _ui_buf_putch('\n');
        diffY = 0;
        state.y = y;
    }
    if (diffX && diffY){
        ansi_seq_t moveAbs = {0};
        moveAbs.num_params = 2;
        moveAbs.opt_flags = OPT_REMOVE_TRAILING_1;
        moveAbs.P[0] = y + 1;
        moveAbs.P[1] = x + 1;
        moveAbs.ch = 'H';

        ansi_seq_t moveRelY = {0};
        ansi_seq_t moveRelX = {0};
        _ui_gen_ar_pair('H', NULL, &moveRelY, diffY, y);
        _ui_gen_ar_pair('G', NULL, &moveRelX, diffX, x);
        int totalLen = _ui_buf_calc_len(&moveRelY) + _ui_buf_calc_len(&moveRelX);
        int moveLen = _ui_buf_calc_len(&moveAbs);
        if (moveLen < totalLen){
            _ui_buf_put(&moveAbs);
        } else {
            _ui_buf_put(&moveRelY);
            _ui_buf_put(&moveRelX);
        }
    } else if (diffX){
        ansi_seq_t moveAbs = {0};
        ansi_seq_t moveRel = {0};
        _ui_gen_ar_pair('G', &moveAbs, &moveRel, diffX, x);
        _ui_buf_put_shorter(&moveAbs, &moveRel);
    } else if (diffY){
        ansi_seq_t moveAbs = {0};
        ansi_seq_t moveRel = moveAbs;
        _ui_gen_ar_pair('H', &moveAbs, &moveRel, diffY, y);
        _ui_buf_put_shorter(&moveAbs, &moveRel);
    }
    state.x = x;
    state.y = y;
}

void photon_ui_refresh(void){
    for (int y = 0; y < rows; y++){
        int n = 0;
        int can_clear = 0;
        for (int x = 0; x < cols; x++){
            int i = y * cols + x;
            ui_cell_t *log, *cur;
            cur = &front[i];
            log = &back [i];
            if (memcmp(cur, log, sizeof(*cur)) == 0) continue;
            else can_clear = 1;
            ansi_seq_t mSeq = {0};
            mSeq.ch = 'm';
            int oldFg = state.fg;
            int oldBg = state.bg;
            if (log->fg != cur->fg)
                _ui_set_color(&mSeq, log->fg, 0);
            if (log->bg != cur->bg)
                _ui_set_color(&mSeq, log->bg, 1);
            if (mSeq.num_params > 0){
                _ui_buf_put(&mSeq);
                REC_REFRESH("set color to (#%06x, #%06x) called, were set to #%06x #%06x\n", log->fg, log->bg, oldFg, oldBg);
            }
            _ui_move_cursor(y, x);
            REC_REFRESH("printing '%c'\n", log->ch);
            if (log->ch && !isspace(log->ch)){
                _ui_buf_putch(log->ch);
                n = state.x + 1;
            }/* else if (cur->ch || mSeq.num_params > 0) {
                _ui_buf_putch(' ');
            }*/ else _ui_buf_putch(' ');
            if (state.x + 1 != cols){
                state.x++;
            }

            *cur = *log;
        }
        if (!can_clear) continue;
        _ui_move_cursor(state.y, n);
        ansi_seq_t clearLine = {0};
        clearLine.ch = 'K';
        _ui_buf_put(&clearLine);
    }
    _ui_move_cursor(c_y, c_x);
    _ui_buf_flush();
    frame_number++;
    REC_BROADCAST("\nFrame #%d\n", frame_number);
}

void photon_tint_line(photon_editor_t *editor, int y, int x, int n){
    photon_draw_req_t req = {0};
    for (int c = 0; c < cols; c++){
        req.style = 1;
        req.fg = editor->ui_hints.fg;
        req.bg = editor->ui_hints.bg;
        req.ch = back[y * cols + (x + c)].ch;
        req.x = x + c;
        req.y = y;
        photon_request(editor, &req);
    }
}

void photon_ui_end(void){
#ifdef UI_DEBUG_CALLS
    photon_debug_end_rec(PHOTON_UI_CALLS_NUM);
#endif
#ifdef UI_DEBUG_REFRESH
    photon_debug_end_rec(PHOTON_UI_REFRESH_NUM);
#endif
    cap = top = 0;
    free(buf);
    buf = NULL;

    tcsetattr(STDIN_FILENO, TCSADRAIN, &old);
    printf("\x1b[?1049l\x1b[0m");
}

int photon_ui_width(void){
    return cols;
}
int photon_ui_height(void){
    return rows;
}

#if PHOTON_DEBUG
int photon_ui_frame_number(void){
    return frame_number;
}

int photon_ui_snapshot(const char *fpath, const char *bpath){
    FILE *backfp = fopen(bpath, "wb");
    if (!backfp) return 0;
    FILE *frontfp = fopen(fpath, "wb");
    if (!frontfp){
        fclose(backfp);
        return 0;
    }
    int area = rows * cols;
    int values[4] = {
        (int)0xDECAFC0F,
        rows,
        cols,
        area
    };
    if (fwrite(values, 4, sizeof(int), backfp) < 4 || fwrite(values, 4, sizeof(int), frontfp) < 4) {
        goto err;
    }
    if (fwrite(front, sizeof(ui_cell_t), area, frontfp) < area || fwrite(back, sizeof(ui_cell_t), area, backfp) < area){
        goto err;
    }

    int c = 0;
    goto skip;
err:
    c = 1;
skip:
    fclose(backfp);
    fclose(frontfp);
    return c;
}
#endif
