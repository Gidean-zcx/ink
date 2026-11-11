/* Parsing extracted from simulator/runtime/script.c; environment verbs remain simulator-only. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "script.h"
static action_t *push(ink_script_t *s, uint64_t t, akind_t k) {
    if (s->count == s->capacity) {
        s->capacity = s->capacity ? s->capacity * 2 : 64;
        s->actions = s->resize(s->actions, s->capacity * sizeof *s->actions);
        if (!s->actions) abort();
    }
    action_t *x = &s->actions[s->count++];
    memset(x, 0, sizeof *x);
    x->t = t;
    x->kind = k;
    x->line = s->line;
    snprintf(x->verb, sizeof x->verb, "%.11s", s->verb);
    return x;
}

static int parse_dur(const char *s, uint64_t *out) {
    char *end;
    double v = strtod(s, &end);
    if (end == s) return -1;
    double mul = 1e6; /* ms */
    if (!strcmp(end, "us")) mul = 1e3;
    else if (!strcmp(end, "ms") || !*end) mul = 1e6;
    else if (!strcmp(end, "s")) mul = 1e9;
    else if (!strcmp(end, "m")) mul = 60e9;
    else return -1;
    *out = (uint64_t)(v * mul);
    return 0;
}

/* coordinate with unit: px (default), dp, or % of the logical axis */
static int coord(ink_script_t *script, const char *s, bool is_x) {
    char *end;
    double v = strtod(s, &end);
    if (!strcmp(end, "dp")) return (int)(v * script->dpi / 160.0 + 0.5);
    if (!strcmp(end, "%")) return (int)(v / 100.0 * (is_x ? script->width : script->height) + 0.5);
    return (int)v;
}

static hal_key_t key_named(const char *s) {
    static const struct { const char *n; hal_key_t k; } M[] = {
        {"ai", HAL_KEY_AI}, {"power", HAL_KEY_POWER}, {"vol_up", HAL_KEY_VOL_UP}, {"vol_down", HAL_KEY_VOL_DOWN},
        {"home", HAL_KEY_HOME}, {"prev", HAL_KEY_PREV}, {"next", HAL_KEY_NEXT}, {"back", HAL_KEY_BACK},
    };
    for (size_t i = 0; i < sizeof M / sizeof M[0]; i++)
        if (!strcmp(M[i].n, s)) return M[i].k;
    return HAL_KEY_NONE;
}

static void touch(ink_script_t *s, uint64_t t, int phase, int x, int y) {
    action_t *a = push(s, t, A_TOUCH);
    a->a = phase; /* 0 down/move, 1 up */
    a->b = x;
    a->c = y;
}

int ink_script_line(ink_script_t *s, char *line, int lineno, char *err, size_t errlen) {
    char stamp[64], command[12], name[96];
    int json = 0;
    if (sscanf(line, "%63s %11s %95s %n", stamp, command, name, &json) == 3 && !strcmp(command, "cap") && json) {
        uint64_t d;
        if ((stamp[0] != '@' && stamp[0] != '+') || parse_dur(stamp + 1, &d) || !line[json]) return -1;
        s->previous = stamp[0] == '@' ? d : s->previous + d;
        s->line = lineno; s->verb = "cap";
        action_t *a = push(s, s->previous, A_CAP);
        snprintf(a->text, sizeof a->text, "%s", name);
        snprintf(a->path, sizeof a->path, "%s", line + json);
        return 0;
    }
    char *hash = strchr(line, '#');
    if (hash) *hash = 0;
    char *tok[12];
    int n = 0;
    for (char *p = strtok(line, " \t\r\n"); p && n < 12; p = strtok(NULL, " \t\r\n")) tok[n++] = p;
    if (!n) return 0;
    uint64_t t, d;
    if (tok[0][0] == '@' && !parse_dur(tok[0] + 1, &d)) t = d;
    else if (tok[0][0] == '+' && !parse_dur(tok[0] + 1, &d)) t = s->previous + d;
    else {
        snprintf(err, errlen, "time must look like @1s or +200ms");
        return -1;
    }
    s->previous = t;
    if (n < 2) {
        snprintf(err, errlen, "missing command");
        return -1;
    }
    const char *cmd = tok[1];
    s->line = lineno;
    s->verb = cmd;
    bool ok = true;
    if (!strcmp(cmd, "tap") || !strcmp(cmd, "longpress")) {
        uint64_t hold = !strcmp(cmd, "tap") ? INK_SCRIPT_MS(80) : INK_SCRIPT_MS(800);
        ok = n >= 4 && (n < 5 || !parse_dur(tok[4], &hold));
        if (ok) {
            touch(s, t, 0, coord(s, tok[2], true), coord(s, tok[3], false));
            touch(s, t + hold, 1, coord(s, tok[2], true), coord(s, tok[3], false));
        }
    } else if (!strcmp(cmd, "press") || !strcmp(cmd, "move")) {
        ok = n >= 4;
        if (ok) touch(s, t, 0, coord(s, tok[2], true), coord(s, tok[3], false));
    } else if (!strcmp(cmd, "release")) {
        touch(s, t, 1, -1, -1);
    } else if (!strcmp(cmd, "swipe")) {
        uint64_t dur = INK_SCRIPT_MS(300);
        ok = n >= 6 && (n < 7 || !parse_dur(tok[6], &dur));
        if (ok) {
            int x1 = coord(s, tok[2], true), y1 = coord(s, tok[3], false), x2 = coord(s, tok[4], true), y2 = coord(s, tok[5], false);
            int steps = (int)(dur / INK_SCRIPT_MS(8));
            if (steps < 2) steps = 2;
            for (int i = 0; i <= steps; i++) {
                double frac = (double)i / steps;
                touch(s, t + (uint64_t)(frac * (double)dur), 0, x1 + (int)(frac * (x2 - x1)), y1 + (int)(frac * (y2 - y1)));
            }
            touch(s, t + dur + INK_SCRIPT_MS(4), 1, x2, y2);
        }
    } else if (!strcmp(cmd, "key")) {
        uint64_t hold = INK_SCRIPT_MS(80);
        hal_key_t k = n >= 3 ? key_named(tok[2]) : HAL_KEY_NONE;
        ok = k != HAL_KEY_NONE && (n < 4 || !parse_dur(tok[3], &hold));
        if (ok) {
            action_t *a = push(s, t, A_KEY);
            a->a = k;
            a->b = 1;
            a = push(s, t + hold, A_KEY);
            a->a = k;
            a->b = 0;
        }
    } else if (!strcmp(cmd, "snap") || !strcmp(cmd, "mark") || !strcmp(cmd, "note")) {
        ok = n >= 3;
        if (ok) {
            action_t *a = push(s, t, !strcmp(cmd, "snap") ? A_SNAP : !strcmp(cmd, "mark") ? A_MARK : A_NOTE);
            a->text[0] = 0;
            for (int i = 2; i < n; i++) {
                if (i > 2) strncat(a->text, " ", sizeof a->text - strlen(a->text) - 1);
                strncat(a->text, tok[i], sizeof a->text - strlen(a->text) - 1);
            }
        }
    } else if (!strcmp(cmd, "battery")) {
        ok = n >= 3;
        if (ok) {
            action_t *a = push(s, t, A_BATTERY);
            a->a = atoi(tok[2]);
            a->b = n >= 4 && !strcmp(tok[3], "charging");
        }
    } else if (!strcmp(cmd, "net")) {
        ok = n >= 3 && (!strcmp(tok[2], "lte") || !strcmp(tok[2], "wifi") || !strcmp(tok[2], "none"));
        if (ok) snprintf(push(s, t, A_NET)->text, 16, "%s", tok[2]);
    } else if (!strcmp(cmd, "mic") && n >= 4 && !strcmp(tok[2], "say")) { /* mic say <words>: speech the device may recognise */
        ok = true;
        snprintf(push(s, t, A_MIC_SAY)->text, 96, "%s", tok[3]);
    } else if (!strcmp(cmd, "mic")) {
        ok = n >= 3 && (!strcmp(tok[2], "speech") || !strcmp(tok[2], "silence") || !strcmp(tok[2], "noise") || !strcmp(tok[2], "wake"));
        if (ok) snprintf(push(s, t, A_MIC)->text, 16, "%s", tok[2]);
    } else if (!strcmp(cmd, "sim")) {
        static const char *const kinds[] = {"none", "data", "sms", "voice"};
        int slot = n >= 3 ? (!strcmp(tok[2], "external") ? 0 : !strcmp(tok[2], "internal") ? 1 : -1) : -1, kind = -1;
        for (int k = 0; k < 4 && n >= 4; k++)
            if (!strcmp(tok[3], kinds[k])) kind = k;
        ok = slot >= 0 && kind >= 0;
        if (ok) {
            action_t *a = push(s, t, A_SIM);
            a->a = slot;
            a->b = kind;
        }
    } else if (!strcmp(cmd, "orient")) {
        static const char *const o[] = {"portrait", "upside_down", "landscape_left", "landscape_right", "face_up", "face_down"};
        ok = false;
        for (int k = 0; k < 6 && n >= 3; k++)
            if (!strcmp(tok[2], o[k])) ok = true;
        if (ok) snprintf(push(s, t, A_ORIENT)->text, 16, "%s", tok[2]);
    } else if (!strcmp(cmd, "ap")) {
        ok = n >= 4 && (!strcmp(tok[2], "del") || (!strcmp(tok[2], "add") && n >= 5));
        if (ok) {
            action_t *a = push(s, t, A_AP);
            a->a = !strcmp(tok[2], "add");
            snprintf(a->num, 24, "%s", tok[3]);
            if (a->a) a->b = atoi(tok[4]);
            if (n >= 6) snprintf(a->text, 96, "%s", tok[5]);
        }
    } else if (!strcmp(cmd, "headset")) {
        static const char *const ops[] = {"add", "on", "off", "in", "out"};
        int op = -1;
        for (int k = 0; k < 5 && n >= 4; k++)
            if (!strcmp(tok[2], ops[k])) op = k;
        ok = op >= 0;
        if (ok) {
            action_t *a = push(s, t, A_HEADSET);
            a->a = op;
            snprintf(a->num, 24, "%s", tok[3]);
            if (n >= 5) snprintf(a->text, 96, "%s", tok[4]);
        }
    } else if (!strcmp(cmd, "shake")) {
        push(s, t, A_SHAKE);
    } else if (!strcmp(cmd, "rtc")) {
        ok = n >= 3 && (!strcmp(tok[2], "lost") || (!strcmp(tok[2], "line") && n >= 4 && !strcmp(tok[3], "stuck")));
        if (ok) push(s, t, !strcmp(tok[2], "lost") ? A_RTC_LOST : A_RTC_LINE);
    } else if (!strcmp(cmd, "hub-clock")) {
        ok = n >= 3;
        if (ok) push(s, t, A_HUB_CLOCK)->c = atoi(tok[2]);
    } else if (!strcmp(cmd, "wifi") || !strcmp(cmd, "cell")) {
        bool broken = !strcmp(cmd, "wifi") && n >= 3 && !strcmp(tok[2], "broken");
        ok = n >= 3 && (!strcmp(tok[2], "on") || !strcmp(tok[2], "off") || broken);
        if (ok) {
            action_t *a = push(s, t, A_ENV);
            a->a = broken ? 2 : !strcmp(cmd, "cell");
            a->b = broken || !strcmp(tok[2], "on");
            a->c = n >= 4 ? atoi(tok[3]) : 0;
        }
    } else if (!strcmp(cmd, "sms-in")) {
        ok = n >= 4;
        if (ok) {
            action_t *a = push(s, t, A_SMS_IN);
            snprintf(a->num, sizeof a->num, "%s", tok[2]);
            a->text[0] = 0;
            for (int k = 3; k < n; k++) {
                size_t l = strlen(a->text);
                snprintf(a->text + l, sizeof a->text - l, "%s%s", k > 3 ? " " : "", tok[k]);
            }
        }
    } else if (!strcmp(cmd, "call-in")) {
        ok = n >= 3;
        if (ok) snprintf(push(s, t, A_CALL_IN)->num, 24, "%s", tok[2]);
    } else if (!strcmp(cmd, "remote")) {
        ok = n >= 3 && (!strcmp(tok[2], "answer") || !strcmp(tok[2], "hangup"));
        if (ok) push(s, t, A_REMOTE)->a = !strcmp(tok[2], "answer");
    } else if (!strcmp(cmd, "outage")) {
        uint64_t dur;
        ok = n >= 3 && !parse_dur(tok[2], &dur);
        if (ok) push(s, t, A_OUTAGE)->c = (int)(dur / 1000000ull); /* ms */
    } else if (!strcmp(cmd, "faults")) {
        ok = n >= 3 && (!strcmp(tok[2], "off") || (n >= 4 && atof(tok[3]) >= 0 && atof(tok[3]) <= 1));
        if (ok) {
            action_t *a = push(s, t, A_FAULTS);
            if (strcmp(tok[2], "off")) {
                snprintf(a->text, sizeof a->text, "%s", tok[2]);
                a->c = (int)(atof(tok[3]) * 1000 + 0.5); /* per mille */
            }
        }
    } else if (!strcmp(cmd, "sd")) {
        ok = n >= 3 && (!strcmp(tok[2], "remove") || !strcmp(tok[2], "insert") || !strcmp(tok[2], "full") || (!strcmp(tok[2], "free") && n >= 4));
        if (ok) {
            action_t *a = push(s, t, A_SD);
            a->a = !strcmp(tok[2], "remove") ? 0 : !strcmp(tok[2], "insert") ? 1 : !strcmp(tok[2], "full") ? 2 : 3;
            a->c = n >= 4 ? atoi(tok[3]) : 0;
        }
    } else if (!strcmp(cmd, "usb")) {
        ok = n >= 3 && (!strcmp(tok[2], "plug") || !strcmp(tok[2], "unplug") || !strcmp(tok[2], "eject") || (!strcmp(tok[2], "copy-in") && n >= 5));
        if (ok) {
            action_t *a = push(s, t, A_USB);
            a->a = !strcmp(tok[2], "plug") ? 0 : !strcmp(tok[2], "unplug") ? 1 : !strcmp(tok[2], "eject") ? 2 : 3;
            if (a->a == 3) {
                snprintf(a->path, 200, "%s", tok[3]);
                snprintf(a->text, sizeof a->text, "%s", tok[4]);
            }
        }
    } else if (!strcmp(cmd, "powercut")) {
        action_t *a = push(s, t, A_POWERCUT);
        a->c = (n >= 3 && !strcmp(tok[2], "write")) ? (n >= 4 ? atoi(tok[3]) : 1) : 0;
    } else if (!strcmp(cmd, "end")) {
        push(s, t, A_END);
        s->has_end = true;
    } else {
        ok = false;
    }
    if (!ok) {
        snprintf(err, errlen, "bad command or arguments: %s", cmd);
        return -1;
    }
return 0;
}


bool ink_script_cap_allowed(const ink_cap_t *c) {
    static const char *const denied[] = {"member.", "kid.", "pairing.", "restore.", "backup.", "factory.", "pin.",
        "shell.pin", "shell.member", "shell.kid", "shell.confirm", "confirm.", "system.undo", "data.", "file.",
        "message.", "sms.", "phone.", "call.", "hub/", "ai.", "text.submit", "text.enter"};
    if (!c || c->effect >= INK_FX_LOCAL_DESTRUCTIVE || c->parent_pin || c->ask_by_hand || (c->expose & INK_EXPOSE_OWNER)) return false;
    if (strstr(c->id, "delete") || strstr(c->id, "remove") || strstr(c->id, "password") || strstr(c->id, "reset") || strstr(c->id, "pay")) return false;
    for (size_t i = 0; i < sizeof denied / sizeof *denied; i++)
        if (!strncmp(c->id, denied[i], strlen(denied[i]))) return false;
    return true;
}
