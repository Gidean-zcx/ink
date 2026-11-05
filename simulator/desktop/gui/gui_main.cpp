/*
 * ink-sim-gui: the desktop simulator (R-042, D-43 A).
 *
 * A window shows the simulated device (panel, cover keys, side keys) next to a control panel for
 * the hardware around it — battery and USB cable, network, microphone, power — with live logs and
 * resource use. The simulation is ink-sim's: sim_main() runs on a thread of its own in realtime
 * mode, and the window reaches it only through sim_gui.h.
 *
 *   ink-sim-gui [ink-sim options...] [--speed X] [--data DIR]
 *               [--gui-screenshot F.png --gui-exit-after SECONDS]   (headless check, CI)
 *               [--gui-click X,Y@SECONDS]...  (a mouse click at window point X,Y at that virtual time)
 *
 * Defaults when not given: the expected-cost profile of the Plus, the build's assets and sample
 * books, and a persistent flash image and SD card under DIR (default <build>/gui-data).
 *
 * The device follows the official board (config.h @690a023, docs/research/simulator/
 * hardware-parity.md): cover keys HOME, back/PREV and NEXT are touch points the touch controller
 * reports below the panel at X 100/200/300; POWER (TCA9555 P1.7) and VOL+/VOL- (P1.0/P1.1) are on
 * the side; BOOT (GPIO61) is the AI key. Where the keys sit on the case is schematic: the official
 * project publishes no case drawing.
 */
#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include "gui_glyphs.h"
#include "host_camera.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include "ink/hal_input.h"
#include "ink/hal_power.h"
#include "sim_gui.h"
#include "stb_image_write.h"

namespace {

/* ------------------------------------------------------------------ logs (stdout + stderr) */

struct LogLine {
    std::string text;
    int level; /* 0 normal, 1 warning, 2 error */
};

std::mutex g_log_m;
std::deque<LogLine> g_log;
const size_t kLogMax = 5000;
int g_orig_out = -1, g_orig_err = -1;

int classify(const std::string &s) {
    if (s.find("panic") != std::string::npos || s.find("PANIC") != std::string::npos ||
        s.find("FATAL") != std::string::npos || s.find("BUDGET FAIL") != std::string::npos)
        return 2;
    if (s.find("warning") != std::string::npos || s.find("WARN") != std::string::npos ||
        s.find("[W]") != std::string::npos)
        return 1;
    return 0;
}

void push_line(std::string s) {
    if (!s.empty() && s.back() == '\r') s.pop_back();
    std::lock_guard<std::mutex> lock(g_log_m);
    int level = classify(s);
    g_log.push_back(LogLine{std::move(s), level});
    while (g_log.size() > kLogMax) g_log.pop_front();
}

struct Pipe {
    int read_fd, orig_fd;
};

void *log_reader(void *arg) {
    Pipe *p = static_cast<Pipe *>(arg);
    std::string acc;
    char buf[4096];
    for (;;) {
        ssize_t n = read(p->read_fd, buf, sizeof buf);
        if (n <= 0) break;
        if (p->orig_fd >= 0) {
            ssize_t w = write(p->orig_fd, buf, (size_t)n); /* the terminal still sees everything */
            (void)w;
        }
        acc.append(buf, (size_t)n);
        size_t pos;
        while ((pos = acc.find('\n')) != std::string::npos) {
            push_line(acc.substr(0, pos));
            acc.erase(0, pos + 1);
        }
    }
    return nullptr;
}

/* Route fd through a pipe whose reader copies it to the original fd and to the log panel. */
/* The read end and the saved original stay out of a restarted process (exec closes them). */
int capture(int fd, FILE *f) {
    int fds[2];
    if (pipe(fds)) return -1;
    int orig = dup(fd);
    if (orig < 0) {
        close(fds[0]);
        close(fds[1]);
        return -1;
    }
    fcntl(fds[0], F_SETFD, FD_CLOEXEC);
    fcntl(orig, F_SETFD, FD_CLOEXEC);
    fflush(f);
    dup2(fds[1], fd);
    close(fds[1]);
    setvbuf(f, nullptr, _IOLBF, 0);
    Pipe *p = new Pipe{fds[0], orig};
    pthread_t th;
    if (pthread_create(&th, nullptr, log_reader, p) == 0) {
        pthread_detach(th);
    } else { /* no reader: put the original back rather than fill a pipe nobody drains */
        dup2(orig, fd);
        close(fds[0]);
        delete p;
    }
    return orig;
}

void restore_std_fds() {
    fflush(stdout);
    fflush(stderr);
    if (g_orig_out >= 0) dup2(g_orig_out, 1);
    if (g_orig_err >= 0) dup2(g_orig_err, 2);
}

/* ------------------------------------------------------------------ options and the simulation */

struct Options {
    std::vector<std::string> sim_args; /* argv for sim_main, [0] = program name */
    double speed = 1.0;
    std::string data_dir;
    std::string screenshot;  /* --gui-screenshot */
    double exit_after_s = 0; /* --gui-exit-after, virtual seconds */
    struct Click {
        float x, y;
        double t;
        int stage; /* 0 waiting, 1 pressed, 2 released */
    };
    std::vector<Click> clicks; /* --gui-click: drives the window's own input path in tests */
    std::string profile;     /* for the title */
};

Options g_opt;
std::vector<char *> g_sim_argv;
std::atomic<bool> g_sim_done{false};
std::atomic<int> g_sim_exit{-1};
pthread_t g_sim_th;

bool has_arg(const std::vector<std::string> &v, const char *name) {
    for (const std::string &a : v)
        if (a == name) return true;
    return false;
}

bool exists(const std::string &p) { return access(p.c_str(), F_OK) == 0; }

void mkdirs(const std::string &path) {
    std::string cur;
    for (size_t i = 0; i < path.size(); i++) {
        cur += path[i];
        if ((path[i] == '/' && i > 0) || i + 1 == path.size()) mkdir(cur.c_str(), 0755);
    }
}

void parse_options(int argc, char **argv) {
    g_opt.data_dir = INK_BINARY_DIR "/gui-data";
    g_opt.sim_args.push_back(argv[0]);
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        const char *v = i + 1 < argc ? argv[i + 1] : nullptr;
        if (a == "--speed" && v) {
            g_opt.speed = atof(v);
            i++;
        } else if (a == "--data" && v) {
            g_opt.data_dir = v;
            i++;
        } else if (a == "--gui-screenshot" && v) {
            g_opt.screenshot = v;
            i++;
        } else if (a == "--gui-exit-after" && v) {
            g_opt.exit_after_s = atof(v);
            i++;
        } else if (a == "--gui-click" && v) {
            Options::Click c = {0, 0, 0, 0};
            if (sscanf(v, "%f,%f@%lf", &c.x, &c.y, &c.t) == 3) g_opt.clicks.push_back(c);
            i++;
        } else {
            if (a == "--profile" && v) g_opt.profile = v;
            g_opt.sim_args.push_back(a);
        }
    }
    std::vector<std::string> &s = g_opt.sim_args;
    const std::string data = g_opt.data_dir;
    mkdirs(data + "/sd");
    if (!has_arg(s, "--profile")) {
        g_opt.profile = INK_SOURCE_DIR "/simulator/profiles/metalio-plus-psram16.json";
        s.insert(s.end(), {"--profile", g_opt.profile});
    }
    if (!has_arg(s, "--assets") && exists(INK_BINARY_DIR "/assets")) s.insert(s.end(), {"--assets", INK_BINARY_DIR "/assets"});
    if (!has_arg(s, "--content") && exists(INK_BINARY_DIR "/test-content"))
        s.insert(s.end(), {"--content", INK_BINARY_DIR "/test-content"});
    if (!has_arg(s, "--sd-dir")) s.insert(s.end(), {"--sd-dir", data + "/sd"});
    if (!has_arg(s, "--flash-image")) s.insert(s.end(), {"--flash-image", data + "/user-flash.bin"});
    if (!has_arg(s, "--out")) s.insert(s.end(), {"--out", data + "/out"});
    for (std::string &x : s) g_sim_argv.push_back(&x[0]);
    g_sim_argv.push_back(nullptr);
}

void *sim_thread(void *) {
    int rc = sim_main((int)g_sim_argv.size() - 1, g_sim_argv.data());
    g_sim_exit = rc;
    g_sim_done = true;
    return nullptr;
}

/* ------------------------------------------------------------------ the panel image */

SDL_Renderer *g_ren;
SDL_Texture *g_tex;
int g_tex_w, g_tex_h;
std::vector<uint8_t> g_gray;
int g_gray_w, g_gray_h;
uint32_t g_lut[256];
int g_lut_cold = -1, g_lut_warm = -1;
unsigned g_frames;

/* E-paper reflects ambient light; the front light (cold/warm LEDs, permille) adds tinted light. */
void build_lut(int cold, int warm) {
    const float ink = 30.0f, paper = 222.0f;
    float sum = (float)(cold + warm);
    float level = sum / 1000.0f > 1.0f ? 1.0f : sum / 1000.0f;
    float t = sum > 0 ? (float)warm / sum : 0.5f;
    const float lc[3] = {0.92f + 0.08f * t, 0.95f - 0.11f * t, 1.0f - 0.37f * t}; /* cold -> warm */
    for (int v = 0; v < 256; v++) {
        float base = ink + (paper - ink) * (float)v / 255.0f;
        uint32_t px = 0xFF000000u;
        for (int c = 0; c < 3; c++) {
            float out = base * (1.0f + 0.45f * level * lc[c]);
            int o = out > 255.0f ? 255 : (int)out;
            px |= (uint32_t)o << (16 - 8 * c);
        }
        g_lut[v] = px;
    }
    g_lut_cold = cold;
    g_lut_warm = warm;
}

void upload_frame() {
    if (g_gray.empty()) return;
    if (!g_tex || g_tex_w != g_gray_w || g_tex_h != g_gray_h) {
        if (g_tex) SDL_DestroyTexture(g_tex);
        g_tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, g_gray_w, g_gray_h);
        if (!g_tex) return;
        SDL_SetTextureScaleMode(g_tex, SDL_ScaleModeLinear);
        g_tex_w = g_gray_w;
        g_tex_h = g_gray_h;
    }
    void *pixels;
    int pitch;
    if (SDL_LockTexture(g_tex, nullptr, &pixels, &pitch)) return;
    for (int y = 0; y < g_gray_h; y++) {
        uint32_t *row = reinterpret_cast<uint32_t *>(static_cast<uint8_t *>(pixels) + (size_t)y * (size_t)pitch);
        const uint8_t *src = &g_gray[(size_t)y * (size_t)g_gray_w];
        for (int x = 0; x < g_gray_w; x++) row[x] = g_lut[src[x]];
    }
    SDL_UnlockTexture(g_tex);
}

/* ------------------------------------------------------------------ speaker -> computer */

std::atomic<bool> g_sound{true};

void audio_cb(void *, Uint8 *stream, int len) { /* SDL's audio thread */
    int16_t *out = reinterpret_cast<int16_t *>(stream);
    int n = len / 2, got = g_sound ? sim_gui_audio_pull(out, n) : 0;
    if (!g_sound) sim_gui_audio_pull(out, n); /* keep the ring moving while muted */
    for (int i = got; i < n; i++) out[i] = 0;
}

SDL_AudioDeviceID open_audio() {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return 0;
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = 16000; /* the device's I2S rate */
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 512;
    want.callback = audio_cb;
    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (dev) SDL_PauseAudioDevice(dev, 0);
    return dev;
}

/* ------------------------------------------------------------------ computer microphone -> mic */

SDL_AudioDeviceID g_mic_dev;

void mic_cb(void *, Uint8 *stream, int len) { sim_gui_mic_push(reinterpret_cast<const int16_t *>(stream), len / 2); }

bool open_mic() {
    if (g_mic_dev) return true;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return false;
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = 16000; /* the device's I2S rate */
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 512;
    want.callback = mic_cb;
    g_mic_dev = SDL_OpenAudioDevice(nullptr, 1, &want, &have, 0);
    if (g_mic_dev) SDL_PauseAudioDevice(g_mic_dev, 0);
    return g_mic_dev != 0;
}

/* ------------------------------------------------------------------ computer camera -> GC2145 */

std::atomic<bool> g_cam_host{true};
std::vector<uint8_t> g_cam_bgra;

/* The sensor asks for a picture (simulation thread): the newest camera frame, cropped to 4:3 and
 * scaled to VGA, as RGB565 with the high byte first. */
bool camera_source(uint8_t *out, int w, int h, void *) {
    if (!g_cam_host) return false;
    if (g_cam_bgra.empty()) g_cam_bgra.resize(1920u * 1440u * 4u);
    int fw = 0, fh = 0;
    if (!host_camera_frame(g_cam_bgra.data(), g_cam_bgra.size(), &fw, &fh) || fw <= 0 || fh <= 0) return false;
    int cw = fw, ch = fh; /* crop to the sensor's aspect */
    if (cw * h > ch * w) cw = ch * w / h;
    else ch = cw * h / w;
    int x0 = (fw - cw) / 2, y0 = (fh - ch) / 2;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const uint8_t *p = &g_cam_bgra[4 * ((size_t)(y0 + y * ch / h) * (size_t)fw + (size_t)(x0 + x * cw / w))];
            uint16_t c = (uint16_t)(((p[2] >> 3) << 11) | ((p[1] >> 2) << 5) | (p[0] >> 3));
            out[2 * (y * w + x)] = (uint8_t)(c >> 8);
            out[2 * (y * w + x) + 1] = (uint8_t)c;
        }
    return true;
}

SDL_Texture *g_pic_tex;
std::vector<uint8_t> g_pic;

void update_picture() {
    if (g_pic.empty()) g_pic.resize(640u * 480u * 2u);
    if (!sim_gui_take_picture(g_pic.data(), g_pic.size())) return;
    if (!g_pic_tex) g_pic_tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 640, 480);
    void *pixels;
    int pitch;
    if (!g_pic_tex || SDL_LockTexture(g_pic_tex, nullptr, &pixels, &pitch)) return;
    for (int y = 0; y < 480; y++) {
        uint32_t *row = reinterpret_cast<uint32_t *>(static_cast<uint8_t *>(pixels) + (size_t)y * (size_t)pitch);
        for (int x = 0; x < 640; x++) {
            uint16_t c = (uint16_t)(g_pic[2 * (y * 640 + x)] << 8 | g_pic[2 * (y * 640 + x) + 1]);
            uint32_t r = (c >> 11) << 3, g = ((c >> 5) & 63) << 2, b = (c & 31) << 3;
            row[x] = 0xFF000000u | r << 16 | g << 8 | b;
        }
    }
    SDL_UnlockTexture(g_pic_tex);
}

/* ------------------------------------------------------------------ device keys */

bool g_mouse_key[HAL_KEY_COUNT], g_kb_key[HAL_KEY_COUNT], g_sent_key[HAL_KEY_COUNT];

void sync_key(int k) {
    bool down = g_mouse_key[k] || g_kb_key[k];
    if (down != g_sent_key[k]) {
        g_sent_key[k] = down;
        sim_gui_key(k, down);
    }
}

int key_for(SDL_Keycode code) {
    switch (code) {
    case SDLK_SPACE: return HAL_KEY_AI;
    case SDLK_p: return HAL_KEY_POWER;
    case SDLK_EQUALS:
    case SDLK_PLUS:
    case SDLK_KP_PLUS: return HAL_KEY_VOL_UP;
    case SDLK_MINUS:
    case SDLK_KP_MINUS: return HAL_KEY_VOL_DOWN;
    case SDLK_h:
    case SDLK_HOME: return HAL_KEY_HOME;
    case SDLK_LEFT:
    case SDLK_PAGEUP: return HAL_KEY_PREV;
    case SDLK_RIGHT:
    case SDLK_PAGEDOWN: return HAL_KEY_NEXT;
    default: return HAL_KEY_NONE;
    }
}

/* ------------------------------------------------------------------ state shown by the window */

sim_gui_stats_t g_st;
double g_load[4];
uint64_t g_load_t0, g_load_busy0[4];
unsigned g_haptic_seen;
uint64_t g_shake_until_ms;
bool g_paused;
double g_speed = 1.0;
int g_batt = 80;
bool g_usb;
int g_mic = 1; /* synthetic speech, as the simulation starts */
bool g_fit = true;
bool g_restart;
bool g_autoscroll = true;
char g_filter[96];
std::string g_toast;
uint64_t g_toast_until;

void toast(const std::string &s) {
    g_toast = s;
    g_toast_until = SDL_GetTicks64() + 4000;
}

void update_stats() {
    sim_gui_stats_t st;
    sim_gui_get_stats(&st);
    if (st.seq == g_st.seq) return;
    if (!g_load_t0 || st.now_ns < g_load_t0) {
        g_load_t0 = st.now_ns;
        for (int c = 0; c < 4; c++) g_load_busy0[c] = st.core_busy_ns[c];
    } else if (st.now_ns - g_load_t0 >= 500000000ull) {
        double dt = (double)(st.now_ns - g_load_t0);
        for (int c = 0; c < st.ncores && c < 4; c++) g_load[c] = (double)(st.core_busy_ns[c] - g_load_busy0[c]) / dt;
        g_load_t0 = st.now_ns;
        for (int c = 0; c < 4; c++) g_load_busy0[c] = st.core_busy_ns[c];
    }
    if (st.haptic_pulses != g_haptic_seen || st.haptic_on) {
        g_haptic_seen = st.haptic_pulses;
        g_shake_until_ms = SDL_GetTicks64() + 200;
    }
    g_st = st;
}

const char *phase_text(sim_gui_phase_t p) {
    switch (p) {
    case SIM_GUI_STARTING: return g_sim_done ? "启动失败（看日志）" : "启动中";
    case SIM_GUI_RUNNING: return g_paused ? "已暂停" : "运行中";
    case SIM_GUI_STOPPED: return "已停止";
    case SIM_GUI_CRASHED: return "已崩溃";
    }
    return "";
}

/* ------------------------------------------------------------------ drawing the device */

/* Geometry in panel pixels (logical, portrait). The cover keys' X are the official touch
 * coordinates; their distance below the panel and the side keys' places are schematic. */
const float kSide = 40, kTop = 66, kChin = 170, kRadius = 44, kKeyOut = 12;

struct Dev {
    ImVec2 o; /* top-left of the body on screen */
    float s;  /* screen points per panel pixel */
    ImVec2 at(float x, float y) const { return ImVec2(o.x + x * s, o.y + y * s); }
};

/* The device is off (shutdown or power-off pulses): pressing POWER starts it again. */
bool powered_off() { return sim_gui_phase() == SIM_GUI_STOPPED && sim_gui_off_reason(); }

bool key_button(const char *id, ImVec2 a, ImVec2 b, int key) {
    ImGui::SetCursorScreenPos(a);
    ImGui::InvisibleButton(id, ImVec2(b.x - a.x, b.y - a.y));
    bool active = ImGui::IsItemActive();
    if (key == HAL_KEY_POWER && ImGui::IsItemActivated() && powered_off()) g_restart = true;
    if (ImGui::IsItemActivated() || ImGui::IsItemDeactivated()) {
        g_mouse_key[key] = active;
        sync_key(key);
    }
    return ImGui::IsItemHovered();
}

void tooltip(const char *text) {
    if (ImGui::BeginTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 22);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void side_key(ImDrawList *dl, const Dev &d, const char *id, bool right, float y0, float y1, int key,
              const char *label, const char *help) {
    float bw = 684 + 2 * kSide;
    ImVec2 a = right ? d.at(bw - 2, y0) : d.at(-kKeyOut, y0);
    ImVec2 b = right ? d.at(bw + kKeyOut, y1) : d.at(2, y1);
    bool hover = key_button(id, a, b, key);
    bool down = g_sent_key[key];
    ImU32 col = down ? IM_COL32(90, 140, 210, 255) : hover ? IM_COL32(120, 120, 128, 255) : IM_COL32(78, 78, 84, 255);
    dl->AddRectFilled(a, b, col, 3.0f * d.s + 1);
    ImVec2 ts = ImGui::CalcTextSize(label);
    float tx = right ? b.x + 6 : a.x - ts.x - 6;
    dl->AddText(ImVec2(tx, (a.y + b.y - ts.y) / 2), IM_COL32(60, 60, 66, 255), label);
    if (hover) tooltip(help);
}

void cover_key(ImDrawList *dl, const Dev &d, const char *id, float cx, float cy, int key, int icon, const char *help) {
    const float r = 34;
    ImVec2 a = d.at(cx - r, cy - r), b = d.at(cx + r, cy + r);
    bool hover = key_button(id, a, b, key);
    bool down = g_sent_key[key];
    ImVec2 c = d.at(cx, cy);
    float u = 12 * d.s;
    ImU32 col = down ? IM_COL32(120, 180, 255, 255) : hover ? IM_COL32(200, 200, 205, 255) : IM_COL32(150, 150, 156, 255);
    if (down) dl->AddCircleFilled(c, r * d.s, IM_COL32(255, 255, 255, 30));
    if (icon == 0) { /* house */
        dl->AddTriangle(ImVec2(c.x - u * 1.2f, c.y - u * 0.1f), ImVec2(c.x, c.y - u * 1.2f), ImVec2(c.x + u * 1.2f, c.y - u * 0.1f), col, 2);
        dl->AddRect(ImVec2(c.x - u * 0.8f, c.y - u * 0.1f), ImVec2(c.x + u * 0.8f, c.y + u * 1.0f), col, 0, 0, 2);
    } else { /* arrow */
        float sgn = icon == 1 ? -1.0f : 1.0f;
        dl->AddTriangleFilled(ImVec2(c.x + sgn * u, c.y), ImVec2(c.x - sgn * u * 0.6f, c.y - u), ImVec2(c.x - sgn * u * 0.6f, c.y + u), col);
    }
    if (hover) tooltip(help);
}

void screen_input(const Dev &d) {
    static bool down;
    static int lx, ly;
    ImVec2 a = d.at(kSide, kTop), b = d.at(kSide + (float)g_tex_w, kTop + (float)g_tex_h);
    ImGui::SetCursorScreenPos(a);
    ImGui::InvisibleButton("##screen", ImVec2(b.x - a.x, b.y - a.y));
    ImVec2 m = ImGui::GetIO().MousePos;
    int x = lx, y = ly;
    if (ImGui::IsMousePosValid(&m) && d.s > 0 && g_tex_w > 0 && g_tex_h > 0) {
        x = (int)std::clamp((m.x - a.x) / d.s, 0.0f, (float)(g_tex_w - 1));
        y = (int)std::clamp((m.y - a.y) / d.s, 0.0f, (float)(g_tex_h - 1));
    }
    bool paused = g_paused;
    if (ImGui::IsItemActivated() && !paused) {
        down = true;
        lx = x;
        ly = y;
        sim_gui_touch(true, x, y);
    } else if (down && ImGui::IsItemActive() && (x != lx || y != ly)) {
        lx = x;
        ly = y;
        sim_gui_touch(true, x, y);
    } else if (down && !ImGui::IsItemActive()) {
        down = false;
        sim_gui_touch(false, lx, ly);
    }
    if (ImGui::IsItemHovered() && !down) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (ImGui::GetIO().KeyShift) tooltip(("触摸坐标 " + std::to_string(x) + ", " + std::to_string(y)).c_str());
    }
}

void draw_device(ImVec2 area_pos, ImVec2 area_size, float fb_scale) {
    ImGui::SetNextWindowPos(area_pos);
    ImGui::SetNextWindowSize(area_size);
    ImGui::Begin("##device", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse);
    ImDrawList *dl = ImGui::GetWindowDrawList();
    sim_gui_phase_t phase = sim_gui_phase();

    char line[200];
    snprintf(line, sizeof line, "%s · 虚拟时间 %.1f s · %g×%s", phase_text(phase), (double)g_st.now_ns / 1e9, g_speed,
             g_st.sleeping ? " · 设备在待机（浅睡）" : "");
    ImGui::SetCursorScreenPos(ImVec2(area_pos.x + 14, area_pos.y + 10));
    ImGui::TextUnformatted(line);

    int pw = g_tex ? g_tex_w : 684, ph = g_tex ? g_tex_h : 1216;
    float bw = (float)pw + 2 * kSide, bh = kTop + (float)ph + kChin;
    float avail_w = area_size.x - 2 * (kKeyOut + 70), avail_h = area_size.y - 60;
    float s = g_fit ? std::min(avail_w / bw, avail_h / bh) : 1.0f / fb_scale;
    if (s <= 0) s = 0.1f;
    Dev d;
    d.s = s;
    d.o = ImVec2(area_pos.x + (area_size.x - bw * s) / 2, area_pos.y + 44 + std::max(0.0f, (avail_h - bh * s) / 2));
    if (SDL_GetTicks64() < g_shake_until_ms) d.o.x += 3.0f * std::sin((float)SDL_GetTicks64() * 0.25f);

    /* body */
    dl->AddRectFilled(d.at(4, 6), d.at(bw + 4, bh + 6), IM_COL32(0, 0, 0, 40), kRadius * s);
    dl->AddRectFilled(d.at(0, 0), d.at(bw, bh), IM_COL32(52, 52, 57, 255), kRadius * s);
    dl->AddRect(d.at(0, 0), d.at(bw, bh), IM_COL32(90, 90, 98, 255), kRadius * s, 0, 1.5f);
    /* panel */
    ImVec2 sa = d.at(kSide, kTop), sb = d.at(kSide + (float)pw, kTop + (float)ph);
    dl->AddRectFilled(ImVec2(sa.x - 3, sa.y - 3), ImVec2(sb.x + 3, sb.y + 3), IM_COL32(25, 25, 28, 255));
    if (g_tex)
        dl->AddImage((ImTextureID)(intptr_t)g_tex, sa, sb);
    else
        dl->AddRectFilled(sa, sb, IM_COL32(200, 199, 192, 255));
    if (g_tex) screen_input(d);

    /* indicators in the top bezel */
    float iy = kTop / 2;
    ImVec2 ip = d.at(kSide, iy);
    auto badge = [&](bool on, const char *text, ImU32 col) {
        if (!on) return;
        ImVec2 ts = ImGui::CalcTextSize(text);
        dl->AddCircleFilled(ImVec2(ip.x + 5, ip.y), 4, col);
        dl->AddText(ImVec2(ip.x + 13, ip.y - ts.y / 2), IM_COL32(220, 220, 225, 255), text);
        ip.x += ts.x + 30;
    };
    badge(g_st.display_busy, "刷新中", IM_COL32(255, 196, 60, 255));
    badge(g_st.sleeping, "待机", IM_COL32(120, 160, 255, 255));
    badge(g_st.charging, "充电", IM_COL32(90, 210, 120, 255));
    badge(SDL_GetTicks64() < g_shake_until_ms, "振动", IM_COL32(255, 120, 120, 255));
    badge(g_st.light_cold + g_st.light_warm > 0, "前光", IM_COL32(255, 230, 170, 255));
    char batt[48];
    const char *net = !strcmp(g_st.link, "wifi") ? (g_st.link_up ? "Wi-Fi" : "Wi-Fi 断开")
                      : !strcmp(g_st.link, "lte")  ? (!g_st.sim_registered ? (g_st.sim_searching ? "4G 注册中" : "4G 无服务") : g_st.link_up ? "4G" : "4G 断开")
                                                   : "无网络";
    snprintf(batt, sizeof batt, "%d%%  %s", g_st.battery_percent, net);
    ImVec2 bts = ImGui::CalcTextSize(batt);
    dl->AddText(ImVec2(d.at(kSide + (float)pw, iy).x - bts.x, d.at(0, iy).y - bts.y / 2), IM_COL32(200, 200, 205, 255), batt);

    /* cover keys: touch X 100 / 200 / 300 below the panel (official TOUCH_VK_*) */
    float ky = kTop + (float)ph + kChin * 0.48f;
    cover_key(dl, d, "##home", kSide + 100, ky, HAL_KEY_HOME, 0,
              "主页（盖板键 HOME）：触摸控制器在屏外 Y≈1600、X≈100 报点。官方用法：返回 / 长按百问对讲。快捷键 H");
    cover_key(dl, d, "##prev", kSide + 200, ky, HAL_KEY_PREV, 1,
              "返回 / 上页（盖板键 PREV）：屏外 X≈200。快捷键 ←");
    cover_key(dl, d, "##next", kSide + 300, ky, HAL_KEY_NEXT, 2, "下页（盖板键 NEXT）：屏外 X≈300。快捷键 →");

    /* side keys (schematic places) */
    side_key(dl, d, "##power", true, 170, 250, HAL_KEY_POWER, "电源",
             "电源键（侧面，TCA9555 P1.7，中断经 GPIO2）。官方：短按进出待机，长按约 3 秒硬关机。快捷键 P");
    side_key(dl, d, "##volup", true, 310, 380, HAL_KEY_VOL_UP, "音量+", "音量+（TCA9555 P1.0），官方也用来翻页。快捷键 =");
    side_key(dl, d, "##voldn", true, 395, 465, HAL_KEY_VOL_DOWN, "音量−", "音量−（TCA9555 P1.1），官方也用来翻页。快捷键 -");
    side_key(dl, d, "##ai", false, 220, 300, HAL_KEY_AI, "AI",
             "AI 键（BOOT，GPIO61 直连）。按住说话，松开发送；麦克风听到什么在右侧“麦克风”里选。快捷键 空格");

    /* state over the panel */
    if (phase != SIM_GUI_RUNNING) {
        const char *msg = phase == SIM_GUI_CRASHED ? "设备崩溃"
                          : powered_off()             ? "已关机（按电源键开机）"
                          : phase == SIM_GUI_STOPPED  ? "设备已停止"
                          : g_sim_done                ? "启动失败"
                                                      : "开机中…";
        ImVec2 ts = ImGui::CalcTextSize(msg);
        ImVec2 c((sa.x + sb.x) / 2, (sa.y + sb.y) / 2);
        dl->AddRectFilled(ImVec2(c.x - ts.x / 2 - 18, c.y - ts.y / 2 - 12), ImVec2(c.x + ts.x / 2 + 18, c.y + ts.y / 2 + 12),
                          phase == SIM_GUI_CRASHED ? IM_COL32(170, 40, 40, 230) : IM_COL32(40, 40, 46, 220), 6);
        dl->AddText(ImVec2(c.x - ts.x / 2, c.y - ts.y / 2), IM_COL32(255, 255, 255, 255), msg);
    }
    if (!g_toast.empty() && SDL_GetTicks64() < g_toast_until) {
        ImVec2 ts = ImGui::CalcTextSize(g_toast.c_str());
        ImVec2 p(area_pos.x + (area_size.x - ts.x) / 2, area_pos.y + area_size.y - ts.y - 16);
        dl->AddRectFilled(ImVec2(p.x - 12, p.y - 6), ImVec2(p.x + ts.x + 12, p.y + ts.y + 6), IM_COL32(30, 30, 34, 230), 6);
        dl->AddText(p, IM_COL32(255, 255, 255, 255), g_toast.c_str());
    }
    ImGui::End();
}

/* ------------------------------------------------------------------ control panel */

void kib_line(const char *name, size_t free_now, size_t min_free, size_t total) {
    if (!total) return;
    float used = 1.0f - (float)free_now / (float)total;
    char buf[96];
    snprintf(buf, sizeof buf, "剩余 %zu KiB，最低 %zu KiB / 共 %zu KiB", free_now / 1024, min_free / 1024, total / 1024);
    ImGui::TextUnformatted(name);
    ImGui::ProgressBar(used, ImVec2(-1, 0), buf);
}

void draw_panel(ImVec2 pos, ImVec2 size) {
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("控制", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                                      ImGuiWindowFlags_NoSavedSettings);
    sim_gui_phase_t phase = sim_gui_phase();
    bool running = phase == SIM_GUI_RUNNING;
    const char *prof = strrchr(g_opt.profile.c_str(), '/');
    ImGui::Text("设备档案：%s", prof ? prof + 1 : g_opt.profile.c_str());
    ImGui::Text("状态：%s", phase_text(phase));
    if (phase == SIM_GUI_CRASHED) ImGui::TextColored(ImVec4(0.8f, 0.1f, 0.1f, 1), "原因：%s", sim_gui_crash_reason());
    if (phase == SIM_GUI_STOPPED || phase == SIM_GUI_CRASHED || (phase == SIM_GUI_STARTING && g_sim_done)) {
        if (ImGui::Button("重新开机", ImVec2(-1, 0))) g_restart = true;
    }

    if (ImGui::CollapsingHeader("运行", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BeginDisabled(!running);
        if (ImGui::Button(g_paused ? "继续" : "暂停")) {
            g_paused = !g_paused;
            sim_gui_set_paused(g_paused);
        }
        ImGui::SameLine();
        static const double speeds[] = {0.25, 0.5, 1, 2, 4, 10};
        for (double sp : speeds) {
            char b[16];
            snprintf(b, sizeof b, "%g×", sp);
            ImGui::SameLine();
            bool sel = g_speed == sp;
            if (sel) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::SmallButton(b)) {
                g_speed = sp;
                sim_gui_set_speed(sp);
            }
            if (sel) ImGui::PopStyleColor();
        }
        if (ImGui::Button("截图")) {
            char name[32];
            snprintf(name, sizeof name, "gui-%.1fs", (double)g_st.now_ns / 1e9);
            sim_gui_snapshot(name);
            toast(std::string("截图已存到 ") + sim_gui_out_dir() + "/snap-" + name + ".png");
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("重启设备")) g_restart = true;
        ImGui::SameLine();
        ImGui::Checkbox("适应窗口", &g_fit);
        bool snd = g_sound;
        if (ImGui::Checkbox("喇叭声音放到电脑上", &snd)) g_sound = snd;
        ImGui::TextDisabled("时间按真机节奏走；暂停时整台设备冻结。");
    }

    ImGui::BeginDisabled(!running);
    if (ImGui::CollapsingHeader("电池与 USB", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderInt("电量", &g_batt, 0, 100, "%d%%");
        bool apply = ImGui::IsItemDeactivatedAfterEdit();
        if (ImGui::Checkbox("插着 USB（充电）", &g_usb)) apply = true;
        if (apply) sim_gui_battery(g_batt, g_usb);
        ImGui::Text("电量计：%d%%，%d mV，电池电流 %+d mA", g_st.battery_percent, g_st.battery_mv, g_st.battery_ma);
        if (g_usb) ImGui::Text("充电芯片：%s%s", g_st.charge_ma > 0 ? "充电中 " : "已充满或未充电", g_st.charge_ma > 0 ? (std::to_string(g_st.charge_ma) + " mA").c_str() : "");
        ImGui::TextDisabled("电量计 BQ27220、充电 CX25601N（开机设 1 A，实际 960 mA），按寄存器模拟。");
    }
    if (ImGui::CollapsingHeader("网络", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool wifi = g_st.env_wifi, cell = g_st.env_cell;
        static int rssi = -60, rsrp = -100;
        if (ImGui::Checkbox("附近有已知的 Wi-Fi", &wifi)) sim_gui_env(0, wifi, rssi);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(130);
        if (ImGui::SliderInt("##rssi", &rssi, -90, -30, "%d dBm")) sim_gui_env(0, wifi, rssi);
        if (ImGui::Checkbox("有 4G 信号", &cell)) sim_gui_env(1, cell, rsrp);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(130);
        if (ImGui::SliderInt("##rsrp", &rsrp, -125, -70, "%d dBm")) sim_gui_env(1, cell, rsrp);
        static const char *chosen[] = {"不联网", "Wi-Fi", "4G"};
        ImGui::Text("系统选用：%s · %s", chosen[g_st.link_chosen >= 0 && g_st.link_chosen <= 2 ? g_st.link_chosen : 0], g_st.link_up ? "在线" : "不通");
        ImGui::TextDisabled("这里只设周围的信号；用哪条网络由系统决定（开机沿用档案里的默认）。");
        if (ImGui::Button("断网 5 秒")) sim_gui_outage(5000);
        if (g_st.cell_modem) {
            ImGui::SeparatorText("4G 卡（两张卡，同一时间只用一张）");
            static const char *kinds[] = {"没有卡", "只有流量", "流量 + 短信", "流量 + 短信 + 通话"};
            static const char *slot_names[] = {"外置卡（卡槽）", "内置卡（贴片）"};
            for (int i = 0; i < 2; i++) {
                ImGui::PushID(i);
                int k = g_st.sim_card[i];
                ImGui::SetNextItemWidth(170);
                if (ImGui::Combo(slot_names[i], &k, kinds, 4)) sim_gui_sim_card(i, k);
                ImGui::PopID();
            }
            const char *state = !g_st.modem_radio     ? "射频关"
                                : g_st.sim_registered ? (g_st.volte ? "已注册，可通话" : "已注册，不能通话")
                                : g_st.sim_searching  ? "正在注册"
                                                      : "未注册（这张卡不在）";
            ImGui::Text("模组：卡槽设置 %s · 在用 %s · %s", g_st.sim_selected ? "内置卡" : "外置卡",
                        g_st.sim_active ? "内置卡" : "外置卡", state);
            ImGui::TextDisabled("切卡由系统发 AT 命令完成，这里只模拟模组和卡本身。");
            ImGui::TextDisabled("插卡后要等模组重新开射频才识别：是否支持热插拔还没确认。");
        }
    }
    if (ImGui::CollapsingHeader("短信与电话（外面的人）")) {
        static char number[24] = "13800138000", text[256] = "你好，这是一条测试短信。";
        static bool auto_answer = true;
        ImGui::SetNextItemWidth(160);
        ImGui::InputText("号码", number, sizeof number);
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##sms", text, sizeof text);
        if (ImGui::Button("发短信给设备")) sim_gui_sms_to_device(number, text);
        ImGui::SameLine();
        if (ImGui::Button("打电话给设备")) sim_gui_call_device(number);
        static const char *states[] = {"没有通话", "通话中", "", "设备在呼叫对方", "正在呼叫设备"};
        ImGui::Text("通话：%s%s%s", states[g_st.call >= 0 && g_st.call <= 4 ? g_st.call : 0], g_st.call ? " · " : "", g_st.call ? g_st.call_number : "");
        ImGui::BeginDisabled(g_st.call != 3);
        if (ImGui::Button("对方接听")) sim_gui_remote(true);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(g_st.call == 0);
        if (ImGui::Button("对方挂断")) sim_gui_remote(false);
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Checkbox("对方 4 秒后自动接听", &auto_answer)) sim_gui_auto_answer(auto_answer);
        if (g_st.sms_sent > 0) ImGui::TextWrapped("设备发出 %d 条短信，最近一条给 %s：%s", g_st.sms_sent, g_st.last_sms_number, g_st.last_sms_text);
        ImGui::TextDisabled("需要卡有短信 / 通话业务（见“4G 卡”）。通话的声音还没有模拟。");
    }
    if (ImGui::CollapsingHeader("麦克风", ImGuiTreeNodeFlags_DefaultOpen)) {
        static const char *names[] = {"电脑麦克风", "合成的说话", "安静", "噪声"};
        static const char *ids[] = {"host", "speech", "silence", "noise"};
        for (int i = 0; i < 4; i++) {
            if (i) ImGui::SameLine();
            if (ImGui::RadioButton(names[i], g_mic == i)) {
                g_mic = i;
                if (i == 0 && !open_mic()) toast("打不开电脑麦克风（检查系统的麦克风权限）");
                sim_gui_mic(ids[i]);
            }
        }
        ImGui::TextDisabled("麦克风听到的声音（BH1098G 经 I2S，16 kHz）。");
        static const char *modes[] = {"未设置", "模式 1：对话", "模式 2：发给蓝牙耳机", "模式 3：当蓝牙音箱"};
        ImGui::TextDisabled("蓝牙音频芯片：%s%s", !g_st.bt_powered ? "没电（P14 关）" : modes[g_st.bt_mode & 3], "，串口 AT 设置");
    }
    if (ImGui::CollapsingHeader("摄像头 GC2145", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool host = g_cam_host;
        if (ImGui::RadioButton("电脑摄像头", host)) g_cam_host = true;
        ImGui::SameLine();
        if (ImGui::RadioButton("测试图案", !host)) g_cam_host = false;
        ImGui::TextDisabled("%s；设备拍了 %u 张", host_camera_status(), g_st.camera_captures);
        update_picture();
        if (g_pic_tex) ImGui::Image((ImTextureID)(intptr_t)g_pic_tex, ImVec2(240, 180));
        ImGui::TextDisabled("上面是设备最近拍的一张（640×480 RGB565）。拍照由系统调用相机接口完成。");
    }
    if (ImGui::CollapsingHeader("姿态与时钟", ImGuiTreeNodeFlags_DefaultOpen)) {
        static const char *names[] = {"竖着", "倒过来", "向左横放", "向右横放", "平放，屏幕朝上", "平放，屏幕朝下"};
        static const char *ids[] = {"portrait", "upside_down", "landscape_left", "landscape_right", "face_up", "face_down"};
        static int orient = 0;
        ImGui::SetNextItemWidth(170);
        if (ImGui::Combo("设备朝向", &orient, names, 6)) sim_gui_orient(ids[orient]);
        ImGui::SameLine();
        if (ImGui::Button("晃一下")) sim_gui_shake();
        ImGui::TextDisabled("加速度计读数：x %d，y %d，z %d mg；运动中断 %u 次", g_st.accel_mg[0], g_st.accel_mg[1], g_st.accel_mg[2], g_st.accel_motions);
        int64_t days = g_st.rtc_s / 86400, rem = g_st.rtc_s % 86400;
        int64_t z = days + 719468, era = (z >= 0 ? z : z - 146096) / 146097, doe = z - era * 146097;
        int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365, doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        int64_t mp = (5 * doy + 2) / 153, d = doy - (153 * mp + 2) / 5 + 1, m = mp < 10 ? mp + 3 : mp - 9, y = yoe + era * 400 + (m <= 2);
        ImGui::TextDisabled("时钟芯片 PCF8563：%04lld-%02lld-%02lld %02lld:%02lld:%02lld UTC", (long long)y, (long long)m, (long long)d,
                            (long long)(rem / 3600), (long long)(rem / 60 % 60), (long long)(rem % 60));
    }
    if (ImGui::CollapsingHeader("前光与振动")) {
        char b[32];
        snprintf(b, sizeof b, "冷光 %d‰", g_st.light_cold);
        ImGui::ProgressBar((float)g_st.light_cold / 1000.0f, ImVec2(-1, 0), b);
        snprintf(b, sizeof b, "暖光 %d‰", g_st.light_warm);
        ImGui::ProgressBar((float)g_st.light_warm / 1000.0f, ImVec2(-1, 0), b);
        ImGui::Text("振动：%u 次%s", g_st.haptic_pulses, g_st.haptic_on ? "（正在振）" : "");
        ImGui::TextDisabled("由系统控制：前光 GPIO6/7 两路 PWM，马达 TCA9555 P0.3。");
    }
    if (ImGui::CollapsingHeader("掉电")) {
        ImGui::TextWrapped("相当于突然拔掉电池：正在写的 Flash 会写坏一半，然后设备停止。再按“重新开机”看系统能否恢复。");
        if (ImGui::Button("拔掉电池")) ImGui::OpenPopup("确认掉电");
        if (ImGui::BeginPopupModal("确认掉电", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("现在让设备掉电？");
            if (ImGui::Button("掉电")) {
                sim_gui_power_cut();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("取消")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }
    ImGui::EndDisabled();

    if (ImGui::CollapsingHeader("资源", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (int c = 0; c < g_st.ncores && c < 4; c++) {
            char b[40];
            snprintf(b, sizeof b, "CPU%d %.0f%%", c, g_load[c] * 100);
            ImGui::ProgressBar((float)g_load[c], ImVec2(-1, 0), b);
        }
        kib_line("内部 RAM", g_st.internal_free, g_st.internal_min_free, g_st.internal_total);
        kib_line("PSRAM", g_st.external_free, g_st.external_min_free, g_st.external_total);
        ImGui::TextDisabled("CPU 按档案的成本模型折算到 S31，不是电脑的占用。");
    }
    if (ImGui::CollapsingHeader("快捷键")) {
        ImGui::TextUnformatted("鼠标左键：手指（按住拖动 = 滑动）\n"
                               "空格：AI 键（按住说话）\nP：电源键（按住 3 秒 = 长按）\n= / -：音量+ / 音量−\n"
                               "H：主页　← / →：上页 / 下页\nShift 悬停屏幕：显示触摸坐标");
    }
    ImGui::Separator();
    ImGui::TextDisabled("输出：%s", sim_gui_out_dir());
    ImGui::TextDisabled("数据：%s", g_opt.data_dir.c_str());
    ImGui::End();
}

/* ------------------------------------------------------------------ log panel */

void draw_logs(ImVec2 pos, ImVec2 size) {
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("日志", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                                      ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetNextItemWidth(260);
    ImGui::InputTextWithHint("##filter", "筛选（区分大小写）", g_filter, sizeof g_filter);
    ImGui::SameLine();
    ImGui::Checkbox("自动滚动", &g_autoscroll);
    ImGui::SameLine();
    if (ImGui::Button("清空")) {
        std::lock_guard<std::mutex> lock(g_log_m);
        g_log.clear();
    }
    ImGui::BeginChild("##lines", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
    {
        std::lock_guard<std::mutex> lock(g_log_m);
        std::vector<int> idx;
        idx.reserve(g_log.size());
        for (size_t i = 0; i < g_log.size(); i++)
            if (!g_filter[0] || g_log[i].text.find(g_filter) != std::string::npos) idx.push_back((int)i);
        ImGuiListClipper clip;
        clip.Begin((int)idx.size());
        while (clip.Step())
            for (int r = clip.DisplayStart; r < clip.DisplayEnd; r++) {
                const LogLine &l = g_log[(size_t)idx[(size_t)r]];
                if (l.level == 2)
                    ImGui::TextColored(ImVec4(0.85f, 0.15f, 0.15f, 1), "%s", l.text.c_str());
                else if (l.level == 1)
                    ImGui::TextColored(ImVec4(0.75f, 0.45f, 0.0f, 1), "%s", l.text.c_str());
                else
                    ImGui::TextUnformatted(l.text.c_str());
            }
    }
    if (g_autoscroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 40) ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
    ImGui::End();
}

/* ------------------------------------------------------------------ fonts, screenshot, restart */

void load_font(float fb_scale) {
    ImGuiIO &io = ImGui::GetIO();
    struct Cand {
        const char *path;
        int face;
    };
    const Cand cands[] = {
        {INK_GUI_FONT, INK_GUI_FONT_FACE},
        {"/System/Library/Fonts/Hiragino Sans GB.ttc", 0},
        {"/System/Library/Fonts/STHeiti Light.ttc", 0},
        {"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc", 2},
        {"/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc", 2},
    };
    static ImVector<ImWchar> ranges;
    ImFontGlyphRangesBuilder rb;
    rb.AddRanges(io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    rb.AddText(kGuiGlyphs);
    rb.BuildRanges(&ranges);
    for (const Cand &c : cands) {
        if (!c.path[0] || !exists(c.path)) continue;
        ImFontConfig cfg;
        cfg.FontNo = c.face;
        cfg.OversampleH = 1;
        cfg.OversampleV = 1;
        cfg.PixelSnapH = true;
        if (io.Fonts->AddFontFromFileTTF(c.path, 16.0f * fb_scale, &cfg, ranges.Data)) {
            io.FontGlobalScale = 1.0f / fb_scale;
            return;
        }
    }
    fprintf(stderr, "ink-sim-gui: no CJK font found; labels will show as '?'\n");
    io.Fonts->AddFontDefault();
}

bool save_screenshot(SDL_Renderer *ren, const std::string &path) {
    int w, h;
    if (SDL_GetRendererOutputSize(ren, &w, &h)) return false;
    std::vector<uint8_t> argb((size_t)w * (size_t)h * 4), rgb((size_t)w * (size_t)h * 3);
    SDL_RenderSetScale(ren, 1, 1); /* read in pixels */
    SDL_Rect all = {0, 0, w, h};
    if (SDL_RenderReadPixels(ren, &all, SDL_PIXELFORMAT_ARGB8888, argb.data(), w * 4)) return false;
    for (size_t i = 0; i < (size_t)w * (size_t)h; i++) {
        uint32_t p;
        memcpy(&p, &argb[i * 4], 4);
        rgb[i * 3] = (uint8_t)(p >> 16);
        rgb[i * 3 + 1] = (uint8_t)(p >> 8);
        rgb[i * 3 + 2] = (uint8_t)p;
    }
    return stbi_write_png(path.c_str(), w, h, 3, rgb.data(), w * 3) != 0;
}

std::string self_path(const char *argv0) {
    char buf[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
    if (n > 0) {
        buf[n] = 0;
        return buf;
    }
    if (realpath(argv0, buf)) return buf;
    return argv0;
}

/* Stop the simulation cleanly (the flash image is saved) before the window goes away. */
void stop_simulation() {
    if (sim_gui_phase() == SIM_GUI_CRASHED) return; /* its thread is parked for good */
    sim_gui_set_paused(false);
    sim_gui_stop();
    for (int i = 0; i < 500 && !g_sim_done; i++) SDL_Delay(10);
    if (g_sim_done) pthread_join(g_sim_th, nullptr);
}

} // namespace

int main(int argc, char **argv) {
    std::string self = self_path(argv[0]);
    std::vector<char *> orig_argv(argv, argv + argc);
    orig_argv.push_back(nullptr);
    parse_options(argc, argv);
    bool headless = !g_opt.screenshot.empty();
    g_orig_out = capture(1, stdout);
    g_orig_err = capture(2, stderr);

    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        restore_std_fds();
        fprintf(stderr, "ink-sim-gui: SDL_Init failed: %s\n", SDL_GetError());
        return 3;
    }
    SDL_Window *win = SDL_CreateWindow("ink 模拟器 · Metalio E-Ink4 Plus", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1400,
                                       940, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!win) {
        restore_std_fds();
        fprintf(stderr, "ink-sim-gui: cannot open a window: %s\n", SDL_GetError());
        return 3;
    }
    g_ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_ren) g_ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!g_ren) {
        restore_std_fds();
        fprintf(stderr, "ink-sim-gui: no renderer: %s\n", SDL_GetError());
        return 3;
    }
    int ww, wh, dw, dh;
    SDL_GetWindowSize(win, &ww, &wh);
    SDL_GetRendererOutputSize(g_ren, &dw, &dh);
    float fb_scale = ww > 0 ? (float)dw / (float)ww : 1.0f;
    if (fb_scale < 1) fb_scale = 1;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    ImGui::StyleColorsLight();
    ImGui::GetStyle().WindowRounding = 0;
    ImGui_ImplSDL2_InitForSDLRenderer(win, g_ren);
    ImGui_ImplSDLRenderer2_Init(g_ren);
    load_font(fb_scale);
    build_lut(0, 0);
    SDL_AudioDeviceID audio = headless ? 0 : open_audio();
    if (!headless) host_camera_open();
    sim_gui_set_camera_source(camera_source, nullptr);

    sim_gui_enable();
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 8u << 20);
    if (pthread_create(&g_sim_th, &attr, sim_thread, nullptr) != 0) {
        restore_std_fds();
        fprintf(stderr, "ink-sim-gui: cannot start the simulation thread\n");
        return 3;
    }
    pthread_attr_destroy(&attr);

    bool speed_set = false, quit = false, shot_done = false;
    int exit_code = 0;
    uint64_t last_poll = 0, start_ms = SDL_GetTicks64();
    while (!quit) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            ImGui_ImplSDL2_ProcessEvent(&ev);
            if (ev.type == SDL_QUIT) quit = true;
            if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_CLOSE) quit = true;
            if ((ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP) && !ev.key.repeat && !io.WantTextInput) {
                int k = key_for(ev.key.keysym.sym);
                if (k == HAL_KEY_POWER && ev.type == SDL_KEYDOWN && powered_off()) g_restart = true;
                if (k != HAL_KEY_NONE && (!g_paused || ev.type == SDL_KEYUP)) {
                    g_kb_key[k] = ev.type == SDL_KEYDOWN;
                    sync_key(k);
                }
            }
        }
        sim_gui_phase_t phase = sim_gui_phase();
        if (phase == SIM_GUI_RUNNING && !speed_set) {
            speed_set = true;
            if (g_opt.speed > 0 && g_opt.speed != 1.0) {
                g_speed = g_opt.speed;
                sim_gui_set_speed(g_speed);
            }
        }
        uint64_t now = SDL_GetTicks64();
        if (now - last_poll >= 33) {
            last_poll = now;
            sim_gui_poll();
        }
        update_stats();
        for (Options::Click &c : g_opt.clicks) {
            double vt = (double)g_st.now_ns / 1e9;
            SDL_Event e;
            if (c.stage == 0 && g_frames > 0 && vt >= c.t) {
                memset(&e, 0, sizeof e);
                e.type = SDL_MOUSEMOTION;
                e.motion.windowID = SDL_GetWindowID(win);
                e.motion.x = (int)c.x;
                e.motion.y = (int)c.y;
                SDL_PushEvent(&e);
                memset(&e, 0, sizeof e);
                e.type = SDL_MOUSEBUTTONDOWN;
                e.button.windowID = SDL_GetWindowID(win);
                e.button.button = SDL_BUTTON_LEFT;
                e.button.state = SDL_PRESSED;
                e.button.clicks = 1;
                e.button.x = (int)c.x;
                e.button.y = (int)c.y;
                SDL_PushEvent(&e);
                c.stage = 1;
            } else if (c.stage == 1 && vt >= c.t + 0.15) {
                memset(&e, 0, sizeof e);
                e.type = SDL_MOUSEBUTTONUP;
                e.button.windowID = SDL_GetWindowID(win);
                e.button.button = SDL_BUTTON_LEFT;
                e.button.state = SDL_RELEASED;
                e.button.x = (int)c.x;
                e.button.y = (int)c.y;
                SDL_PushEvent(&e);
                c.stage = 2;
            }
        }
        size_t cap = 1216u * 1216u;
        if (g_gray.size() < cap) g_gray.resize(cap);
        int fw, fh;
        bool fresh = sim_gui_take_frame(g_gray.data(), g_gray.size(), &fw, &fh);
        if (fresh) {
            g_gray_w = fw;
            g_gray_h = fh;
            g_frames++;
        }
        if (g_st.light_cold != g_lut_cold || g_st.light_warm != g_lut_warm) {
            build_lut(g_st.light_cold, g_st.light_warm);
            fresh = g_frames > 0;
        }
        if (fresh) upload_frame();

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        ImVec2 ds = io.DisplaySize;
        float panel_w = 400, log_h = std::max(150.0f, ds.y * 0.24f);
        draw_device(ImVec2(0, 0), ImVec2(ds.x - panel_w, ds.y - log_h), fb_scale);
        draw_logs(ImVec2(0, ds.y - log_h), ImVec2(ds.x - panel_w, log_h));
        draw_panel(ImVec2(ds.x - panel_w, 0), ImVec2(panel_w, ds.y));
        ImGui::Render();
        SDL_RenderSetScale(g_ren, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColor(g_ren, 236, 236, 240, 255);
        SDL_RenderClear(g_ren);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), g_ren);

        if (headless && !shot_done) {
            bool reached = g_frames > 0 && (double)g_st.now_ns / 1e9 >= g_opt.exit_after_s;
            bool over = phase != SIM_GUI_RUNNING && g_sim_done;
            if (reached || over || now - start_ms > 120000) {
                shot_done = true;
                bool ok = reached && save_screenshot(g_ren, g_opt.screenshot);
                fprintf(stderr, "ink-sim-gui: %s at %.2f s virtual, %u frames from the panel%s\n",
                        ok ? "screenshot saved" : "no screenshot", (double)g_st.now_ns / 1e9, g_frames,
                        ok ? "" : " — the simulation stopped or never drew");
                exit_code = ok ? 0 : 1;
                quit = true;
            }
        }
        SDL_RenderPresent(g_ren);
        if (g_restart) quit = true;
        if (headless) SDL_Delay(5);
    }

    for (int k = 0; k < HAL_KEY_COUNT; k++) { /* nothing stays held */
        g_mouse_key[k] = g_kb_key[k] = false;
        sync_key(k);
    }
    stop_simulation();
    if (audio) SDL_CloseAudioDevice(audio);
    if (g_mic_dev) SDL_CloseAudioDevice(g_mic_dev);
    host_camera_close();
    if (g_pic_tex) SDL_DestroyTexture(g_pic_tex);
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    if (g_tex) SDL_DestroyTexture(g_tex);
    SDL_DestroyRenderer(g_ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    restore_std_fds();
    if (g_restart) {
        execv(self.c_str(), orig_argv.data());
        fprintf(stderr, "ink-sim-gui: restart failed\n");
        return 3;
    }
    if (!headless && g_sim_done && g_sim_exit > 0) exit_code = g_sim_exit;
    fflush(NULL);
    _exit(exit_code); /* a crashed simulation's threads stay parked: do not wait for them */
}
