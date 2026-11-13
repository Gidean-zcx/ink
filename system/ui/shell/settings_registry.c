/* The settings registry: one declaration per setting (settings_registry.h). Every row the settings pages draw,
 * everything settings.search finds and every id settings.open takes is in ITEMS, nowhere else. */
#include "shell_internal.h"
#include "settings_registry.h"
#include "ink/hal_afe.h"
#include "ink/hal_device.h"

enum { NET, SOUND, SHOW, FAMILY, AI, STORE, SYS };
static const char *const GROUPS[] = {TR_NOOP("网络和连接"), TR_NOOP("声音和通知"), TR_NOOP("显示和阅读"), TR_NOOP("家人和孩子"),
                                     "AI", TR_NOOP("存储和备份"), TR_NOOP("系统")};
const ink_sr_item_t *ink_sr_target;

static void tenths(const ink_sr_item_t *it, char *out, size_t cap) { snprintf(out, cap, "%lld", (long long)ink_setting_get(it->setting) / 10); }
static void minutes(const ink_sr_item_t *it, char *out, size_t cap) { snprintf(out, cap, TR("%lld 分钟"), (long long)ink_setting_get(it->setting) / 60); }
static void wifi(const ink_sr_item_t *it, char *out, size_t cap) {
    (void)it; state_text("wifi.ssid", out, cap);
    if (!ink_setting_get("wifi.enabled") || !out[0]) snprintf(out, cap, "%s", ink_setting_get("wifi.enabled") ? TR("还没有连接") : TR("已关闭"));
}
static void cell(const ink_sr_item_t *it, char *out, size_t cap) {
    (void)it; snprintf(out, cap, "%s", integer("cell.registered") ? TR("4G 在线") : integer("cell.sim_missing") ? TR("没有 SIM 卡") : TR("4G 还没有连上"));
}
static void bluetooth(const ink_sr_item_t *it, char *out, size_t cap) {
    (void)it; state_text("bt.device", out, cap);
    if (!integer("bt.connected") || !out[0]) snprintf(out, cap, "%s", integer("bt.connected") ? TR("已连接") : TR("没有连接"));
}
static void people(const ink_sr_item_t *it, char *out, size_t cap) {
    int n = 0; (void)it;
    while (ink_member_at(n)) n++;
    snprintf(out, cap, TR("家里 %d 个人"), n);
}
static void handsfree(const ink_sr_item_t *it, char *out, size_t cap) { /* the home names it whole, its own page's choices are short */
    int64_t v = ink_setting_get("voice.handsfree");
    bool home = !strcmp(it->id, "ai");
    snprintf(out, cap, "%s", v == 2 ? (home ? TR("叫名字已关闭") : TR("关")) : v == 1 ? (home ? TR("一直能叫名字") : TR("一直")) : home ? TR("使用时能叫名字") : TR("使用时"));
}
static void voice_name(const ink_sr_item_t *it, char *out, size_t cap) {
    const char *const *names = hal_afe_names(); int n = 0;
    while (names[n]) n++;
    int64_t v = ink_setting_get(it->id);
    snprintf(out, cap, "%s", names[v >= 0 && v < n ? v : 0]);
}
static void channels(const ink_sr_item_t *it, char *out, size_t cap) {
    int off = 0; const ink_notify_channel_t *ch; (void)it;
    for (int i = 0; (ch = ink_notify_channel_at(i)); i++) off += !ink_notify_channel_on(ch->id);
    if (off) snprintf(out, cap, TR("关了 %d 类"), off); else snprintf(out, cap, "%s", TR("都收"));
}
static void space(const ink_sr_item_t *it, char *out, size_t cap) { /* what is left on the storage card */
    int64_t present = 0, v = 0; char mode[32] = ""; (void)it;
    ink_state_get_int(ink_state_find("storage.card"), &present);
    state_text("storage.mode", mode, sizeof mode);
    if (!present || !strcmp(mode, "usb")) snprintf(out, cap, "%s", !strcmp(mode, "usb") ? TR("存储卡正借给电脑") : TR("没有可用的存储卡"));
    else if (ink_state_get_int(ink_state_find("storage.free"), &v) && v >= 0) {
        int64_t unit = v >= 1024LL * 1024 * 1024 ? 1024LL * 1024 * 1024 : v >= 1024 * 1024 ? 1024 * 1024 : 1024;
        snprintf(out, cap, TR("还能放 %lld.%lld %s"), (long long)(v / unit), (long long)(v % unit * 10 / unit), unit > 1024 * 1024 ? "GB" : unit > 1024 ? "MB" : "KB");
    } else snprintf(out, cap, "%s", TR("暂时读不出来"));
}
static void automatic(const ink_sr_item_t *it, char *out, size_t cap) {
    int64_t mode = ink_setting_get("backup.auto"); (void)it;
    snprintf(out, cap, TR("自动备份：%s"), mode == 1 ? TR("每天") : mode == 2 ? TR("每周") : TR("关"));
}
static void release(const ink_sr_item_t *it, char *out, size_t cap) {
    (void)it; if (ink_state_get_str(ink_state_find("update.version"), out, cap) < 0) snprintf(out, cap, "%s", TR("暂时读不出来"));
}
static void update(const ink_sr_item_t *it, char *out, size_t cap) {
    char s[32]; (void)it; state_text("update.state", s, sizeof s);
    snprintf(out, cap, "%s", !strcmp(s, "idle") ? TR("检查新版本") : !strcmp(s, "available") ? TR("有新版本") :
        !strcmp(s, "failed") ? TR("检查或更新失败，可以再试") : !strcmp(s, "ready") ? TR("已下载") : TR("正在更新"));
}
static void prefer(const ink_sr_item_t *it, char *out, size_t cap) {
    int64_t v = ink_setting_get(it->setting); snprintf(out, cap, "%s", !v ? TR("自动") : v == 1 ? "Wi-Fi" : "4G");
}
static void slot(const ink_sr_item_t *it, char *out, size_t cap) { snprintf(out, cap, "%s", ink_setting_get(it->setting) ? TR("内置卡") : TR("外置卡")); }
static void language(const ink_sr_item_t *it, char *out, size_t cap) {
    const char *name = ink_lang_name((int)ink_setting_get(it->id)); snprintf(out, cap, "%s", name ? name : "");
}
static void owner(const ink_sr_item_t *it, char *out, size_t cap) { (void)it; snprintf(out, cap, TR("%s的设备"), ink_member_current()->name); }
static void model(const ink_sr_item_t *it, char *out, size_t cap) {
    hal_device_info_t info; (void)it; hal_device_get_info(&info); snprintf(out, cap, "%s", info.display_name ? info.display_name : "");
}
static void battery(const ink_sr_item_t *it, char *out, size_t cap) { (void)it; snprintf(out, cap, "%lld%%", (long long)integer("battery.level")); }

static ink_sr_state_t why(ink_sr_state_t state, char *out, size_t cap, const char *text) { snprintf(out, cap, "%s", text); return state; }
static ink_sr_state_t unlisted(char *out, size_t cap) { (void)out; (void)cap; return INK_SR_UNSEARCHABLE; }
static ink_sr_state_t airplane(char *out, size_t cap) { return ink_setting_get("net.airplane") ? why(INK_SR_DEPENDENT, out, cap, TR("飞行模式开着")) : INK_SR_AVAILABLE; }
static ink_sr_state_t wifi_on(char *out, size_t cap) { return ink_setting_get("wifi.enabled") ? INK_SR_AVAILABLE : why(INK_SR_DEPENDENT, out, cap, TR("Wi-Fi 已关闭")); }
static ink_sr_state_t vibrating(char *out, size_t cap) { return ink_setting_get("audio.vibration") ? INK_SR_AVAILABLE : why(INK_SR_DEPENDENT, out, cap, TR("振动已关闭")); }
static ink_sr_state_t sim_free(char *out, size_t cap) { return integer("cell.config_readonly") ? why(INK_SR_NOT_NOW, out, cap, TR("暂不能更改 SIM 卡")) : INK_SR_AVAILABLE; }
static ink_sr_state_t has_bt(char *out, size_t cap) { return integer("bt.available") ? INK_SR_AVAILABLE : why(INK_SR_UNSUPPORTED, out, cap, TR("这台设备不能用蓝牙耳机")); }
static ink_sr_state_t has_afe(char *out, size_t cap) { return hal_afe_available() ? INK_SR_AVAILABLE : why(INK_SR_UNSUPPORTED, out, cap, TR("这台设备暂不支持叫名字")); }

#define H INK_SR_HOME
#define L INK_SR_LIST
/* In the order the home lists them, then the order of each list page's rows. */
static const ink_sr_item_t ITEMS[] = {
    {.id = "wifi", .title = "Wi-Fi", .aliases = TR_NOOP("wifi wlan 无线 上网 网络"), .icon = "wifi", .group = NET, .page = "SetNetwork", .setting = "wifi.enabled", .summary = wifi, .where = H, .adult = true},
    {.id = "cell", .title = TR_NOOP("移动网络"), .aliases = TR_NOOP("4g sim 流量 运营商 接入点 apn 漫游"), .icon = "sim", .group = NET, .page = "SetMobile", .open = "shell.mobile", .summary = cell, .state = airplane, .where = H, .adult = true},
    {.id = "bt", .title = TR_NOOP("蓝牙"), .aliases = TR_NOOP("耳机 蓝牙耳机"), .icon = "bluetooth", .group = NET, .page = "SetBluetooth", .summary = bluetooth, .state = has_bt, .where = H, .adult = true},
    {.id = "audio.volume", .title = TR_NOOP("音量"), .aliases = TR_NOOP("声音 大声 小声"), .icon = "volume", .group = SOUND, .page = "SetSound", .setting = "audio.volume", .summary = tenths, .where = H},
    {.id = "notify.dnd", .title = TR_NOOP("勿扰"), .aliases = TR_NOOP("免打扰 安静 静音"), .icon = "moon", .group = SOUND, .page = "SetSound", .setting = "notify.dnd", .where = H},
    {.id = "display.light", .title = TR_NOOP("亮度"), .aliases = TR_NOOP("前光 屏幕亮度 灯"), .icon = "sun", .group = SHOW, .page = "SetDisplay", .setting = "display.light", .summary = tenths, .where = H},
    {.id = "display.warmth", .title = TR_NOOP("暖光"), .aliases = TR_NOOP("色温 夜间 护眼 暖色"), .icon = "warm", .group = SHOW, .page = "SetDisplay", .setting = "display.warmth", .summary = tenths, .where = H},
    {.id = "display.font_size", .title = TR_NOOP("字号"), .aliases = TR_NOOP("文字大小 字体 大字"), .icon = "word", .group = SHOW, .page = "SetDisplay", .setting = "display.font_size", .where = H, .adult = true},
    {.id = "lock", .title = TR_NOOP("锁屏和壁纸"), .aliases = TR_NOOP("壁纸 表盘 锁屏画面"), .icon = "lock", .group = SHOW, .page = "Wallpaper", .setting = "display.lock_style", .where = H, .adult = true},
    {.id = "family", .title = TR_NOOP("家人"), .aliases = TR_NOOP("成员 家长 孩子 密码"), .icon = "family", .group = FAMILY, .page = "SetMembers", .summary = people, .where = H, .adult = true},
    {.id = "ai", .title = TR_NOOP("语音和 AI"), .aliases = TR_NOOP("叫名字 语音 唤醒"), .icon = "wave", .group = AI, .page = "SetVoice", .summary = handsfree, .where = H, .adult = true},
    {.id = "storage", .title = TR_NOOP("存储"), .aliases = TR_NOOP("空间 存储卡 容量"), .icon = "storage", .group = STORE, .page = "SetStorage", .summary = space, .where = H, .adult = true},
    {.id = "backup", .title = TR_NOOP("备份"), .aliases = TR_NOOP("家庭服务器 恢复"), .icon = "cloud", .group = STORE, .page = "SetStorage", .cap = "backup.open", .summary = automatic, .where = H | L, .adult = true},
    {.id = "about", .title = TR_NOOP("关于和更新"), .aliases = TR_NOOP("版本 系统更新 升级 关于本机"), .icon = "info", .group = SYS, .page = "SetAbout", .summary = release, .where = H, .adult = true},
    /* Rows of the hand-laid pages (settings_body.c, settings_family.c), found by search and located by settings.open. */
    {.id = "net.airplane", .title = TR_NOOP("飞行模式"), .aliases = TR_NOOP("飞行 断网"), .icon = "plane", .group = NET, .page = "SetNetwork", .setting = "net.airplane", .adult = true},
    {.id = "net.prefer", .title = TR_NOOP("上网方式"), .aliases = TR_NOOP("4g 自动 流量"), .icon = "globe", .group = NET, .page = "SetNetwork", .setting = "net.prefer", .summary = prefer, .adult = true},
    {.id = "cell.sim_slot", .title = TR_NOOP("SIM 卡"), .aliases = TR_NOOP("切卡 内置卡 外置卡 sim"), .icon = "sim", .group = NET, .page = "SetNetwork", .setting = "cell.sim_slot", .summary = slot, .state = sim_free, .adult = true},
    {.id = "wifi.known", .title = TR_NOOP("已存的网络"), .aliases = TR_NOOP("忘掉 密码 wifi"), .icon = "wifi", .group = NET, .page = "KnownNetworks", .state = wifi_on, .adult = true},
    {.id = "audio.key_tone", .title = TR_NOOP("按键提示音"), .aliases = TR_NOOP("按键音 提示音"), .icon = "bell", .group = SOUND, .page = "SetSound", .setting = "audio.key_tone"},
    {.id = "audio.vibration", .title = TR_NOOP("振动"), .aliases = TR_NOOP("震动"), .icon = "wave", .group = SOUND, .page = "SetSound", .setting = "audio.vibration"},
    {.id = "audio.vibration_level", .title = TR_NOOP("振动强度"), .aliases = TR_NOOP("震动强度"), .icon = "wave", .group = SOUND, .page = "SetSound", .setting = "audio.vibration_level", .state = vibrating},
    {.id = "audio.call_ring", .title = TR_NOOP("来电铃声"), .aliases = TR_NOOP("铃声 电话"), .icon = "phone", .group = SOUND, .page = "SetSound", .setting = "audio.call_ring"},
    {.id = "audio.alarm_ring", .title = TR_NOOP("闹钟铃声"), .aliases = TR_NOOP("铃声 闹钟"), .icon = "clock", .group = SOUND, .page = "SetSound", .setting = "audio.alarm_ring"},
    {.id = "notify.channels", .title = TR_NOOP("通知"), .aliases = TR_NOOP("提醒 消息"), .icon = "bell", .group = SOUND, .page = "SetNotifications", .summary = channels, .adult = true},
    {.id = "power.lock_after_s", .title = TR_NOOP("自动锁屏"), .aliases = TR_NOOP("锁屏 休眠 待机"), .icon = "lock", .group = SHOW, .page = "SetDisplay", .setting = "power.lock_after_s", .summary = minutes, .adult = true},
    {.id = "refresh", .title = TR_NOOP("刷新屏幕"), .aliases = TR_NOOP("残影 清屏"), .icon = "display", .group = SHOW, .page = "SetDisplay"},
    {.id = "today", .title = TR_NOOP("今日屏放什么"), .aliases = TR_NOOP("主屏 卡片"), .icon = "grid", .group = SHOW, .page = "SetDisplay"},
    {.id = "kid", .title = TR_NOOP("孩子的设置"), .aliases = TR_NOOP("孩子 时长 应用名单 禁用时段"), .icon = "kid", .group = FAMILY, .page = "SetKid", .adult = true},
    /* The list pages' rows (settings_pages.c), in their order. */
    {.id = "voice.handsfree", .title = TR_NOOP("什么时候能叫名字"), .aliases = TR_NOOP("叫名字 唤醒"), .group = AI, .page = "SetVoice", .setting = "voice.handsfree", .note = TR_NOOP("没锁屏时才听，不影响待机。"), .summary = handsfree, .state = has_afe, .where = L, .adult = true},
    {.id = "voice.name", .title = TR_NOOP("用什么名字叫它"), .aliases = TR_NOOP("名字 唤醒词"), .icon = "wave", .group = AI, .page = "SetVoice", .cap = "settings.set", .summary = voice_name, .state = has_afe, .where = L, .adult = true},
    {.id = "voice.interrupt", .title = TR_NOOP("说话能打断回答"), .aliases = TR_NOOP("打断"), .icon = "mic", .group = AI, .page = "SetVoice", .setting = "voice.interrupt", .where = L, .adult = true},
    {.id = "ai.records", .title = TR_NOOP("AI 替你做过的事"), .aliases = TR_NOOP("记录"), .icon = "list", .group = AI, .page = "SetVoice", .cap = "shell.ai_log", .section = "AI", .where = L, .adult = true},
    {.id = "ai.memory", .title = TR_NOOP("AI 记得的事"), .aliases = TR_NOOP("记忆"), .icon = "ai", .group = AI, .page = "SetVoice", .cap = "shell.ai_memory", .text = TR_NOOP("查看"), .note = TR_NOOP("你说的事 AI 会直接做。花钱、填密码、删了找不回来的，才会问你。"), .where = L, .adult = true},
    {.id = "text.assist", .title = TR_NOOP("打字时帮你接话"), .aliases = TR_NOOP("联想 输入法"), .icon = "edit", .group = AI, .page = "SetVoice", .setting = "text.assist", .section = TR_NOOP("打字"), .where = L, .adult = true},
    {.id = "storage.free", .title = TR_NOOP("空间"), .group = STORE, .page = "SetStorage", .section = TR_NOOP("空间"), .summary = space, .state = unlisted, .where = L, .adult = true},
    {.id = "storage.books", .title = TR_NOOP("书"), .icon = "book", .group = STORE, .page = "SetStorage", .state = unlisted, .where = L, .adult = true},
    {.id = "storage.audio", .title = TR_NOOP("音频"), .icon = "headphones", .group = STORE, .page = "SetStorage", .state = unlisted, .where = L, .adult = true},
    {.id = "storage.photos", .title = TR_NOOP("照片和其他"), .icon = "gallery", .group = STORE, .page = "SetStorage", .state = unlisted, .where = L, .adult = true},
    {.id = "storage.clean", .title = TR_NOOP("清理临时文件"), .aliases = TR_NOOP("缓存 临时文件"), .icon = "trash", .group = STORE, .page = "SetStorage", .cap = "file.cleanup", .section = "", .note = TR_NOOP("仅清理临时文件、缓存和未传完的文件。"), .where = L, .adult = true},
    {.id = "device", .title = TR_NOOP("这台设备"), .group = SYS, .page = "SetAbout", .summary = owner, .state = unlisted, .where = L, .adult = true},
    {.id = "update.version", .title = TR_NOOP("系统版本"), .aliases = TR_NOOP("版本"), .group = SYS, .page = "SetAbout", .summary = release, .where = L, .adult = true},
    {.id = "update.check", .title = TR_NOOP("系统更新"), .aliases = TR_NOOP("更新 升级 新版本"), .icon = "update", .group = SYS, .page = "SetAbout", .cap = "shell.update", .summary = update, .where = L, .adult = true},
    {.id = "system.language", .title = TR_NOOP("语言"), .aliases = TR_NOOP("language english 中文 英文"), .icon = "globe", .group = SYS, .page = "SetAbout", .cap = "settings.set", .summary = language, .where = L, .adult = true},
    {.id = "time", .title = TR_NOOP("日期和时间"), .aliases = TR_NOOP("时间 时区 校时 日期"), .icon = "clock", .group = SYS, .page = "SetAbout", .cap = "shell.time", .text = "", .where = L, .adult = true},
    {.id = "privacy", .title = TR_NOOP("隐私"), .aliases = TR_NOOP("数据"), .icon = "shield", .group = SYS, .page = "SetAbout", .cap = "shell.privacy", .text = TR_NOOP("查看"), .where = L, .adult = true},
    {.id = "device.details", .title = TR_NOOP("设备详情"), .aliases = TR_NOOP("型号 电量"), .icon = "display", .group = SYS, .page = "SetAbout", .cap = "shell.device", .section = "", .text = "", .where = L, .adult = true},
    {.id = "licenses", .title = TR_NOOP("开源许可"), .aliases = TR_NOOP("许可 开源"), .icon = "list", .group = SYS, .page = "SetAbout", .cap = "shell.licenses", .text = "", .where = L, .adult = true},
    {.id = "update.auto_download", .title = TR_NOOP("自动下载新版本"), .aliases = TR_NOOP("流量"), .group = SYS, .page = "SetAbout", .setting = "update.auto_download", .section = "", .where = L, .adult = true},
    {.id = "reset", .title = TR_NOOP("清空这台设备"), .aliases = TR_NOOP("恢复出厂 重置"), .group = SYS, .page = "SetAbout", .cap = "factory.open", .text = TR_NOOP("清掉所有家人的数据，找不回来"), .where = L, .adult = true},
    {.id = "privacy.storage", .title = TR_NOOP("你的东西放在哪"), .group = SYS, .page = "SetPrivacy", .text = TR_NOOP("设置和备忘在设备里，书和录音在存储卡上；备份在家庭服务器上。"), .state = unlisted, .where = L, .adult = true},
    {.id = "privacy.ai", .title = TR_NOOP("AI 能看到什么"), .group = SYS, .page = "SetPrivacy", .text = TR_NOOP("提问时能看到当前页。密码框里的字看不到；语音和页面会交给家庭选择的模型服务商。"), .state = unlisted, .where = L, .adult = true},
    {.id = "privacy.retention", .title = TR_NOOP("留多久"), .group = SYS, .page = "SetPrivacy", .text = TR_NOOP("活动记录：大人 1 年，孩子 180 天。对话、记忆和运行记录的期限暂时查不到。"), .state = unlisted, .where = L, .adult = true},
    {.id = "privacy.child", .title = TR_NOOP("孩子的记录"), .group = SYS, .page = "SetPrivacy", .cap = "shell.records", .args = "{\"children\":true}", .text = TR_NOOP("爸爸妈妈可以查看孩子的活动记录，确认后删除；其他记录暂不能删除。"), .state = unlisted, .where = L, .adult = true},
    {.id = "ai.read_clipboard", .title = TR_NOOP("AI 可以读剪贴板"), .aliases = TR_NOOP("剪贴板 复制"), .group = SYS, .page = "SetPrivacy", .setting = "ai.read_clipboard", .where = L, .adult = true},
    {.id = "privacy.records", .title = TR_NOOP("查看和删除记录"), .aliases = TR_NOOP("记录 删除记录"), .group = SYS, .page = "SetPrivacy", .cap = "shell.records", .text = "", .where = L, .adult = true},
    {.id = "device.model", .title = TR_NOOP("型号"), .group = SYS, .page = "DeviceDetails", .summary = model, .where = L, .adult = true},
    {.id = "device.version", .title = TR_NOOP("系统版本"), .group = SYS, .page = "DeviceDetails", .summary = release, .state = unlisted, .where = L, .adult = true},
    {.id = "storage.total", .title = TR_NOOP("存储卡总量"), .group = SYS, .page = "DeviceDetails", .state = unlisted, .where = L, .adult = true},
    {.id = "device.free", .title = TR_NOOP("存储卡剩余"), .group = SYS, .page = "DeviceDetails", .summary = space, .state = unlisted, .where = L, .adult = true},
    {.id = "battery.level", .title = TR_NOOP("电量"), .aliases = TR_NOOP("电池"), .group = SYS, .page = "DeviceDetails", .summary = battery, .where = L, .adult = true},
};
#undef H
#undef L

const ink_sr_item_t *ink_sr_at(int i) { return i >= 0 && i < (int)(sizeof ITEMS / sizeof *ITEMS) ? &ITEMS[i] : NULL; }
const ink_sr_item_t *ink_sr_find(const char *id) {
    for (int i = 0; ink_sr_at(i); i++) if (!strcmp(ITEMS[i].id, id)) return &ITEMS[i];
    return NULL;
}
const char *ink_sr_group(int g) { return g >= 0 && g < (int)(sizeof GROUPS / sizeof *GROUPS) ? TR(GROUPS[g]) : ""; }

static ink_sr_state_t state_of(const ink_sr_item_t *it, char *out, size_t cap) {
    out[0] = 0;
    if (it->adult && child()) return why(INK_SR_PARENT, out, cap, TR("家长管理"));
    return it->state ? it->state(out, cap) : INK_SR_AVAILABLE;
}
ink_sr_state_t ink_sr_value(const ink_sr_item_t *it, char *out, size_t cap) {
    ink_sr_state_t s = state_of(it, out, cap);
    if (s != INK_SR_AVAILABLE && s != INK_SR_UNSEARCHABLE) return s;
    const ink_setting_def_t *d = NULL;
    for (int i = 0; it->setting && (d = ink_setting_at(i)) && strcmp(d->id, it->setting); i++) {}
    int64_t v = it->setting ? ink_setting_get(it->setting) : 0;
    if (it->summary) it->summary(it, out, cap);
    else if (it->text) snprintf(out, cap, "%s", it->text[0] ? TR(it->text) : "");
    else if (d && d->type == INK_SET_ENUM && d->choices) snprintf(out, cap, "%s", TR(d->choices[v]));
    else if (d && d->type == INK_SET_BOOL) snprintf(out, cap, "%s", v ? TR("开") : TR("关"));
    else if (d) snprintf(out, cap, "%lld", (long long)v);
    else if (ink_state_get_str(ink_state_find(it->id), out, cap) < 0) snprintf(out, cap, "%s", TR("暂时读不出来"));
    return s;
}

int ink_sr_search(const char *q, const ink_sr_item_t **out, int max) {
    int n = 0;
    size_t len = strlen(q);
    for (int rank = 0; len && rank < 3; rank++) /* the title begins with it, has it, or an alias or the group has it */
        for (int i = 0; ink_sr_at(i) && n < max; i++) {
            const ink_sr_item_t *it = &ITEMS[i];
            char why_not[96];
            ink_sr_state_t s = state_of(it, why_not, sizeof why_not);
            if (s == INK_SR_UNSEARCHABLE || s == INK_SR_NOT_NOW || s == INK_SR_UNSUPPORTED) continue;
            const char *title = TR(it->title);
            int r = !strncmp(title, q, len) ? 0 : strstr(title, q) ? 1 :
                (it->aliases && strstr(TR(it->aliases), q)) || strstr(ink_sr_group(it->group), q) ? 2 : -1;
            if (r == rank) out[n++] = it;
        }
    return n;
}
