/* Calculator: decimal fixed point with 8 places, so 0.1+0.2 is 0.3. One evaluator behind the keys and the AI. */
#include <stdio.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/sys/appreg.h"
#include "ink/sys/i18n.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/ui_semantics.h"
#include "ink/ui_shell.h"

enum { PLACES = 8, INT_DIGITS = 10, EXPR_MAX = 64, RESULT_MAX = 48 };
#define SCALE 100000000LL           /* 10^PLACES */
#define LIMIT 1000000000000000000LL /* |value| < 10^INT_DIGITS, in units of 10^-PLACES */
typedef enum { CALC_OK, CALC_SYNTAX, CALC_ZERO, CALC_RANGE } calc_err_t;
static const char *const MESSAGE[] = {"", TR_NOOP("算式不完整"), TR_NOOP("不能除以 0"), TR_NOOP("超出范围，整数最多 10 位")};
typedef struct { const char *p; calc_err_t err; } parser_t;

static int64_t fail(parser_t *ps, calc_err_t e) { if (!ps->err) ps->err = e; return 0; }
static int64_t checked(parser_t *ps, int64_t v) { return v >= LIMIT || v <= -LIMIT ? fail(ps, CALC_RANGE) : v; }
static uint64_t magnitude(int64_t v) { return v < 0 ? (uint64_t)-v : (uint64_t)v; }
static int64_t signed_as(uint64_t m, bool negative) { return negative ? -(int64_t)m : (int64_t)m; }

/* a * b / SCALE in base-10^9 limbs, rounded half away from zero: no 128-bit type on the device. */
static int64_t mul(parser_t *ps, int64_t a, int64_t b) {
    const uint64_t B = 1000000000u;
    uint64_t x = magnitude(a), y = magnitude(b), x1 = x / B, x0 = x % B, y1 = y / B, y0 = y % B;
    uint64_t p0 = x0 * y0, p1 = x1 * y0 + x0 * y1 + p0 / B, p2 = x1 * y1 + p1 / B;
    if (p2 >= (uint64_t)(LIMIT / 10000000000LL)) return fail(ps, CALC_RANGE);
    uint64_t low = (p1 % B) * B + p0 % B;
    uint64_t r = p2 * 10000000000ull + low / SCALE + (low % SCALE >= SCALE / 2);
    return checked(ps, signed_as(r, (a < 0) != (b < 0)));
}
/* a / b by long division, one decimal place at a time. */
static int64_t divide(parser_t *ps, int64_t a, int64_t b) {
    if (!b) return fail(ps, CALC_ZERO);
    uint64_t x = magnitude(a), y = magnitude(b), q = x / y, r = x % y;
    if (q >= (uint64_t)(LIMIT / SCALE)) return fail(ps, CALC_RANGE);
    for (int i = 0; i < PLACES; i++) { r *= 10; q = q * 10 + r / y; r %= y; }
    q += 2 * r >= y;
    return checked(ps, signed_as(q, (a < 0) != (b < 0)));
}
static bool digit(char c) { return c >= '0' && c <= '9'; }
static int64_t number(parser_t *ps) {
    int64_t whole = 0, frac = 0; int places = -1; bool any = false, up = false;
    for (; digit(*ps->p) || *ps->p == '.'; ps->p++) {
        if (*ps->p == '.') { if (places >= 0) return fail(ps, CALC_SYNTAX); places = 0; continue; }
        int d = *ps->p - '0'; any = true;
        if (places < 0 && (whole = whole * 10 + d) >= LIMIT / SCALE) return fail(ps, CALC_RANGE);
        else if (places >= 0 && places < PLACES) { frac = frac * 10 + d; places++; }
        else if (places == PLACES) { up = d >= 5; places++; } /* more places than kept: round */
    }
    if (!any) return fail(ps, CALC_SYNTAX);
    for (int i = places < 0 ? 0 : places; i < PLACES; i++) frac *= 10;
    return checked(ps, whole * SCALE + frac + up);
}
static int64_t factor(parser_t *ps, bool *pct) {
    *pct = false;
    if (*ps->p == '-') { ps->p++; return -factor(ps, pct); }
    int64_t v = number(ps);
    if (*ps->p == '%') { ps->p++; *pct = true; v = divide(ps, v, 100 * SCALE); }
    return v;
}
static int64_t term(parser_t *ps, bool *pct) {
    int64_t v = factor(ps, pct);
    while (!ps->err && (*ps->p == '*' || *ps->p == '/')) {
        char op = *ps->p++; bool ignored;
        int64_t r = factor(ps, &ignored);
        v = op == '*' ? mul(ps, v, r) : divide(ps, v, r); *pct = false;
    }
    return v;
}
static int64_t sum(parser_t *ps) {
    bool pct; int64_t v = term(ps, &pct);
    while (!ps->err && (*ps->p == '+' || *ps->p == '-')) {
        char op = *ps->p++; int64_t r = term(ps, &pct);
        if (pct) r = mul(ps, v, r); /* 200+10% is 220, as on a phone */
        v = checked(ps, op == '+' ? v + r : v - r);
    }
    return v;
}
/* × ÷ − and full-width signs as the AI may write them; spaces dropped. False: too long or not ours. */
static bool normalize(const char *in, char *out, size_t cap) {
    static const char *const WIDE[][2] = {{"×", "*"}, {"÷", "/"}, {"−", "-"}, {"＋", "+"}, {"％", "%"}, {"．", "."}};
    size_t n = 0;
    while (*in) {
        unsigned w = 0;
        while (w < sizeof WIDE / sizeof *WIDE && strncmp(in, WIDE[w][0], strlen(WIDE[w][0]))) w++;
        char c = w < sizeof WIDE / sizeof *WIDE ? WIDE[w][1][0] : *in;
        in += w < sizeof WIDE / sizeof *WIDE ? strlen(WIDE[w][0]) : 1;
        if (c == ' ') continue;
        if ((!digit(c) && !strchr(".+-*/%", c)) || n + 1 >= cap) { out[0] = 0; return false; }
        out[n++] = c;
    }
    out[n] = 0;
    return true;
}
static calc_err_t evaluate(const char *ascii, char *out, size_t cap) {
    parser_t ps = {ascii, CALC_OK};
    int64_t v = sum(&ps);
    if (!ps.err && *ps.p) ps.err = CALC_SYNTAX;
    if (ps.err) return ps.err;
    uint64_t m = magnitude(v);
    int n = snprintf(out, cap, "%s%llu.%08llu", v < 0 ? "-" : "", (unsigned long long)(m / SCALE), (unsigned long long)(m % SCALE));
    while (n > 0 && out[n - 1] == '0') out[--n] = 0;
    if (n > 0 && out[n - 1] == '.') out[--n] = 0;
    return CALC_OK;
}

/* ---------------------------------------------------------------- the page */
static struct {
    char expr[EXPR_MAX + 1], result[RESULT_MAX], plain[2 * EXPR_MAX + 8], shown[2 * EXPR_MAX + 16], member[16];
    bool done, error; /* done: what = gave is showing (a result, or why not) */
    lv_obj_t *expr_label, *result_label;
    ink_ui_field_spec_t fields[2];
} K INK_PSRAM_BSS;
static const int TOP = INK_STATUS_BAR + INK_PRIMARY + INK_GAP, KEY_H = INK_PRIMARY, KEY_GAP = INK_GAP_M;
static int keys_top(void) { return INK_CANVAS_H - INK_MARGIN - 5 * KEY_H - 4 * KEY_GAP; }
static int result_top(void) { return keys_top() - INK_GAP_L - INK_LINE_NUMBER; }

static bool is_op(char c) { return c == '+' || c == '-' || c == '*' || c == '/'; }
/* Where the number being typed starts, its own minus sign included. */
static size_t number_start(void) {
    size_t i = strlen(K.expr);
    while (i && !is_op(K.expr[i - 1])) i--;
    if (i && K.expr[i - 1] == '-' && (i == 1 || is_op(K.expr[i - 2]))) i--;
    return i;
}
static void append(const char *s) {
    if (strlen(K.expr) + strlen(s) <= EXPR_MAX) strcat(K.expr, s);
}
/* What the expression is worth so far; a trailing operator is not counted yet. */
static calc_err_t worth(char *out, size_t cap) {
    char ascii[EXPR_MAX + 1];
    snprintf(ascii, sizeof ascii, "%s", K.expr);
    for (size_t n = strlen(ascii); n && is_op(ascii[n - 1]); n--) ascii[n - 1] = 0;
    if (!ascii[0]) { snprintf(out, cap, "0"); return CALC_OK; }
    return evaluate(ascii, out, cap);
}
static void press(const char *k) {
    if (!strcmp(k, "clear") || (K.done && !strcmp(k, "back"))) { K.expr[0] = 0; K.done = K.error = false; snprintf(K.result, sizeof K.result, "0"); return; }
    if (K.done) { /* After =, digits start again and operators go on from the result. */
        if (!strcmp(k, "=")) return;
        bool go_on = !K.error && ((is_op(k[0]) && !k[1]) || !strcmp(k, "%") || !strcmp(k, "neg"));
        snprintf(K.expr, sizeof K.expr, "%s", go_on ? K.result : "");
        K.done = K.error = false;
    }
    size_t len = strlen(K.expr), start = number_start();
    char last = len ? K.expr[len - 1] : 0;
    const char *num = K.expr + start + (K.expr[start] == '-'), *dot = strchr(num, '.');
    if (digit(k[0])) {
        if (last == '%' || (dot ? strlen(dot + 1) >= PLACES : strlen(num) >= INT_DIGITS)) return;
        if (!strcmp(num, "0")) K.expr[len - 1] = 0; /* no leading zeros */
        append(k);
    } else if (!strcmp(k, ".")) {
        if (last != '%' && !dot) append(*num ? "." : "0.");
    } else if (is_op(k[0]) && !k[1]) {
        while (len && is_op(K.expr[len - 1])) K.expr[--len] = 0; /* the last operator is replaced */
        if (!len) append("0");
        append(k);
    } else if (!strcmp(k, "%")) {
        if (digit(last) || last == '.') append("%");
    } else if (!strcmp(k, "neg")) {
        if (K.expr[start] == '-') memmove(K.expr + start, K.expr + start + 1, len - start);
        else if (len < EXPR_MAX) { memmove(K.expr + start + 1, K.expr + start, len - start + 1); K.expr[start] = '-'; }
    } else if (!strcmp(k, "back")) {
        if (len) K.expr[len - 1] = 0;
    } else if (!strcmp(k, "=")) {
        while (len && is_op(K.expr[len - 1])) K.expr[--len] = 0;
        if (!len) return;
        calc_err_t e = worth(K.result, sizeof K.result);
        if (e) snprintf(K.result, sizeof K.result, "%s", MESSAGE[e]);
        K.done = true; K.error = e != CALC_OK;
        return;
    }
    char value[RESULT_MAX];
    if (worth(value, sizeof value) == CALC_OK) snprintf(K.result, sizeof K.result, "%s", value);
}
/* Appends `add` to `out` (of cap) if it all fits. */
static bool put(char *out, size_t cap, const char *add) {
    size_t n = strlen(out), m = strlen(add);
    if (n + m >= cap) return false;
    memcpy(out + n, add, m + 1);
    return true;
}
/* The expression as people read it (× ÷), into K.plain; K.shown is the same in lines of w at px, broken only before an
 * operator so a number never splits. Returns the lines. */
static int layout(int px, int w) {
    char line[sizeof K.shown] = "", token[sizeof K.shown], trial[sizeof K.shown];
    int lines = 1;
    K.plain[0] = K.shown[0] = 0;
    for (const char *p = K.expr; *p;) {
        const char *q = p + 1;
        while (*q && !(is_op(*q) && !is_op(q[-1]))) q++; /* up to the next operator that is not a sign */
        token[0] = 0;
        for (; p < q; p++) put(token, sizeof token, *p == '*' ? "×" : *p == '/' ? "÷" : (char[2]){*p, 0});
        if (!*q && K.done) put(token, sizeof token, " =");
        put(K.plain, sizeof K.plain, token);
        trial[0] = 0; put(trial, sizeof trial, line);
        if (line[0] && (!put(trial, sizeof trial, token) || ink_ui_text_width(trial, px, INK_WEIGHT_REGULAR) > w)) {
            put(K.shown, sizeof K.shown, line); put(K.shown, sizeof K.shown, "\n"); lines++;
            line[0] = 0; put(line, sizeof line, token);
        } else {
            line[0] = 0; put(line, sizeof line, trial[0] ? trial : token);
        }
    }
    put(K.shown, sizeof K.shown, line);
    return lines;
}
/* The biggest size of the spec that fits: no cut-off digits. */
static void show(void) {
    if (!K.result_label) return;
    int w = INK_CANVAS_W - 2 * INK_MARGIN, room = result_top() - INK_GAP_S - TOP;
    static const int EXPR_PX[] = {INK_FONT_SUBTITLE, INK_FONT_LIST, INK_FONT_NOTE};
    int px = INK_FONT_NOTE, h = 0;
    for (unsigned i = 0; i < 3; i++) {
        px = EXPR_PX[i];
        if ((h = layout(px, w) * ink_ui_line_height(px)) <= room) break;
    }
    if (h > room) h = room;
    lv_obj_set_style_text_font(K.expr_label, ink_ui_font_px(px, INK_WEIGHT_REGULAR), 0);
    lv_obj_set_y(K.expr_label, ink_ui_px(result_top() - INK_GAP_S - h));
    lv_obj_set_height(K.expr_label, ink_ui_px(h));
    lv_label_set_text(K.expr_label, K.shown);
    static const int RESULT_PX[] = {INK_FONT_NUMBER, INK_FONT_HEADING, INK_FONT_TITLE, INK_FONT_SUBTITLE};
    px = INK_FONT_SUBTITLE;
    for (unsigned i = K.error ? 2 : 0; i < 4; i++) if (ink_ui_text_width(K.error ? TR(K.result) : K.result, RESULT_PX[i], INK_WEIGHT_REGULAR) <= w) { px = RESULT_PX[i]; break; }
    lv_obj_set_style_text_font(K.result_label, ink_ui_font_px(px, INK_WEIGHT_REGULAR), 0);
    lv_obj_set_y(K.result_label, ink_ui_px(result_top() + INK_LINE_NUMBER - ink_ui_line_height(px)));
    lv_obj_set_height(K.result_label, ink_ui_px(ink_ui_line_height(px)));
    lv_label_set_text(K.result_label, K.error ? TR(K.result) : K.result);
    K.fields[0] = (ink_ui_field_spec_t){.id = "expression", .role = "status", .label = TR("算式"), .value = K.plain};
    K.fields[1] = (ink_ui_field_spec_t){.id = "result", .role = "status", .label = K.error ? TR("算不出") : TR("结果"), .value = K.error ? TR(K.result) : K.result};
    ink_ui_content_changed();
}
static void key_cap(ink_call_t *c) {
    char k[8] = "";
    ink_args_get_str(&c->args, "key", k, sizeof k);
    press(k);
    show();
    snprintf(c->rc.object, sizeof c->rc.object, "calculator");
    snprintf(c->rc.say, sizeof c->rc.say, "%.47s", K.error ? TR(K.result) : K.result);
}
static void eval_cap(ink_call_t *c) {
    char text[4 * EXPR_MAX + 1] = "", ascii[EXPR_MAX + 1], out[RESULT_MAX];
    ink_args_get_str(&c->args, "expr", text, sizeof text);
    calc_err_t e = normalize(text, ascii, sizeof ascii) ? evaluate(ascii, out, sizeof out) : CALC_SYNTAX;
    snprintf(c->rc.object, sizeof c->rc.object, "calculator");
    if (e) {
        c->rc.status = INK_RC_INVALID;
        snprintf(c->rc.detail, sizeof c->rc.detail, "%s", TR(MESSAGE[e]));
        snprintf(c->rc.say, sizeof c->rc.say, "%s", TR(MESSAGE[e]));
    } else {
        char json[2 * EXPR_MAX + RESULT_MAX + 32];
        ink_jw_t jw; ink_jw_init(&jw, json, sizeof json);
        ink_jw_raw(&jw, "{\"expr\":"); ink_jw_str(&jw, ascii); ink_jw_raw(&jw, ",\"result\":"); ink_jw_str(&jw, out); ink_jw_raw(&jw, "}");
        ink_cap_result(c, json);
        snprintf(c->rc.say, sizeof c->rc.say, TR("等于 %s"), out);
    }
    if (K.result_label && !strcmp(ink_ui_page_name(), "Calculator")) { /* the page shows what was asked */
        snprintf(K.expr, sizeof K.expr, "%s", ascii);
        snprintf(K.result, sizeof K.result, "%s", e ? MESSAGE[e] : out);
        K.done = true; K.error = e != CALC_OK;
        show();
    }
}

/* Label or icon, key. The signs are icons drawn alike: the fonts give + - × ÷ in different sizes and weights. */
static const char *const KEYS[20][3] = {
    {"C", NULL, "clear"}, {NULL, "del", "back"}, {"%", NULL, "%"}, {NULL, "divide", "/"},
    {"7", NULL, "7"}, {"8", NULL, "8"}, {"9", NULL, "9"}, {NULL, "times", "*"},
    {"4", NULL, "4"}, {"5", NULL, "5"}, {"6", NULL, "6"}, {NULL, "minus", "-"},
    {"1", NULL, "1"}, {"2", NULL, "2"}, {"3", NULL, "3"}, {NULL, "plus", "+"},
    {NULL, "plusminus", "neg"}, {"0", NULL, "0"}, {".", NULL, "."}, {NULL, "equals", "="}};
static void create(lv_obj_t *screen, void *arg) {
    (void)arg;
    if (strcmp(K.member, ink_member_current()->id)) { /* one member's sums are not the next one's */
        K.expr[0] = 0; K.done = K.error = false; snprintf(K.result, sizeof K.result, "0");
        snprintf(K.member, sizeof K.member, "%s", ink_member_current()->id);
    }
    if (!K.result[0]) snprintf(K.result, sizeof K.result, "0");
    ink_shell_statusbar(screen, true); ink_shell_topbar(screen, TR("计算器"));
    int w = INK_CANVAS_W - 2 * INK_MARGIN, cell = (w + KEY_GAP) / 4;
    K.expr_label = ink_ui_label(screen, "", INK_FONT_SUBTITLE, INK_WEIGHT_REGULAR, INK_MARGIN, TOP, w);
    K.result_label = ink_ui_line(screen, "", INK_FONT_NUMBER, INK_WEIGHT_REGULAR, INK_MARGIN, result_top(), w);
    lv_obj_set_style_text_align(K.expr_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_align(K.result_label, LV_TEXT_ALIGN_RIGHT, 0);
    for (int i = 0; i < 20; i++) {
        char json[32];
        snprintf(json, sizeof json, "{\"key\":\"%s\"}", KEYS[i][2]);
        lv_obj_t *b = ink_ui_pill(screen, KEYS[i][0] ? KEYS[i][0] : "", INK_MARGIN + (i % 4) * cell, keys_top() + (i / 4) * (KEY_H + KEY_GAP),
                                  cell - KEY_GAP, KEY_H, i == 19, NULL, NULL);
        ink_ui_pill_text(b, INK_FONT_HEADING, INK_WEIGHT_MEDIUM);
        if (KEYS[i][1]) lv_obj_center(ink_ui_icon(b, KEYS[i][1], INK_ICON_APP, i == 1 ? INK_ICON_LINE : INK_ICON_SOLID, 0, 0, i == 19));
        ink_shell_bind(b, "calc.key", json);
    }
    show();
}
static void destroy(void) { K.expr_label = K.result_label = NULL; }
static int snapshot(char *out, size_t cap) {
    const ink_ui_action_spec_t ACTIONS[] = {
        {.id = "eval", .label = TR("给一个算式求值"), .capability = "calc.eval", .enabled = true},
        {.id = "equals", .label = TR("等于"), .capability = "calc.key", .args_json = "{\"key\":\"=\"}", .enabled = true},
        {.id = "back", .label = TR("删除一位"), .capability = "calc.key", .args_json = "{\"key\":\"back\"}", .enabled = true},
        {.id = "clear", .label = TR("清除"), .capability = "calc.key", .args_json = "{\"key\":\"clear\"}", .enabled = true}};
    const ink_ui_page_spec_t page = {.id = "Calculator", .title = TR("计算器"), .object = {"calculator", 1}, .fields = K.fields,
        .field_count = 2, .actions = ACTIONS, .action_count = sizeof ACTIONS / sizeof *ACTIONS};
    return ink_ui_semantics_snapshot(&page, out, cap);
}
static const ink_activity_t PAGE = {.name = "Calculator", .app = "calculator", .create = create, .destroy = destroy, .snapshot = snapshot};
static const ink_app_def_t APP = {.id = "calculator", .title = TR_NOOP("计算器"), .icon = INK_UI_ICON(calc), .entry = &PAGE, .system_app = true, .child_ok = true};
static const ink_cap_t CAPS[] = {
    {.id = "calc.eval", .app = "calculator", .title = "给一个算式求值（加减乘除、百分号，十进制 8 位小数）",
     .input = "{\"type\":\"object\",\"properties\":{\"expr\":{\"type\":\"string\",\"minLength\":1,\"maxLength\":64}},\"required\":[\"expr\"]}",
     .output = "{\"type\":\"object\",\"properties\":{\"expr\":{\"type\":\"string\"},\"result\":{\"type\":\"string\"}}}",
     .effect = INK_FX_READ, .child_ok = true, .run = eval_cap},
    {.id = "calc.key", .app = "calculator", .title = "按计算器的一个键（back 删除一位，neg 正负号，clear 清除）",
     .input = "{\"type\":\"object\",\"properties\":{\"key\":{\"type\":\"string\",\"enum\":[\"0\",\"1\",\"2\",\"3\",\"4\",\"5\",\"6\",\"7\",\"8\",\"9\","
              "\".\",\"+\",\"-\",\"*\",\"/\",\"%\",\"neg\",\"back\",\"clear\",\"=\"]}},\"required\":[\"key\"]}",
     .activity = "Calculator", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = key_cap}};
int ink_calculator_init(void) { return ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) || ink_app_register(&APP) ? -1 : 0; }
