#include <string.h>
#include "lua.h"
#include "lauxlib.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "spike.h"

#define INK_SPIKE_VM_LIMIT (128u * 1024u)
static lua_State *vm;
static size_t used;
static ink_source_t caller;
static bool building;

static void *allocate(void *ctx, void *ptr, size_t old_size, size_t new_size) {
    (void)ctx;
    if (!ptr) old_size = 0;
    if (!new_size) { osal_free(ptr); used -= old_size; return NULL; }
    if (new_size > INK_SPIKE_VM_LIMIT - (used - old_size)) return NULL;
    const char *prev = osal_mem_tag("appfw.lua");
    void *next = osal_realloc(ptr, new_size, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (next) {
        used = used - old_size + new_size;
        if (used > ink_spike_vm_peak) ink_spike_vm_peak = used;
    }
    return next;
}

static int error(lua_State *l) { lua_pushliteral(l, "invalid page or action"); return lua_error(l); }
static void budget(lua_State *l, lua_Debug *ar) {
    (void)ar;
    lua_pushliteral(l, "instruction budget exceeded");
    lua_error(l);
}
static int copy(lua_State *l, int at, char *dst, size_t cap) {
    size_t n;
    const char *s = lua_tolstring(l, at, &n);
    if (!s || n >= cap || memchr(s, 0, n)) return error(l);
    memcpy(dst, s, n + 1);
    return 0;
}
static int title(lua_State *l) { return building ? copy(l, 1, ink_spike_model->title, 64) : error(l); }
static int toggle(lua_State *l) {
    unsigned i = ink_spike_model->count;
    if (!building || i >= INK_SPIKE_ROWS || !lua_isboolean(l, 2)) return error(l);
    copy(l, 1, ink_spike_model->rows[i].label, 64);
    ink_spike_model->rows[i].value = lua_toboolean(l, 2);
    ink_spike_model->count++;
    return 0;
}
static int field(lua_State *l) {
    if (!building) return error(l);
    copy(l, 1, ink_spike_model->field_label, 64);
    return copy(l, 2, ink_spike_model->text, 128);
}
static int button(lua_State *l) { return building ? copy(l, 1, ink_spike_model->button, 64) : error(l); }
static int set(lua_State *l) {
    int ok;
    lua_Integer row = lua_tointegerx(l, 1, &ok);
    if (building || !ok || row < 0 || row >= INK_SPIKE_ROWS || !lua_isboolean(l, 2)) return error(l);
    if (ink_spike_dispatch((unsigned)row, lua_toboolean(l, 2), caller) != 0) return error(l);
    return 0;
}

int ink_spike_runtime_start(void) {
    vm = lua_newstate(allocate, NULL);
    if (!vm) return -1;
    const luaL_Reg functions[] = {{"title", title}, {"toggle", toggle}, {"textfield", field},
                                  {"button", button}, {"set", set}, {NULL, NULL}};
    for (const luaL_Reg *r = functions; r->name; r++) {
        lua_pushcfunction(vm, r->func);
        lua_setglobal(vm, r->name);
    }
    return 0;
}

static int evaluate(const char *source, int results, int instructions) {
    lua_sethook(vm, budget, LUA_MASKCOUNT, instructions);
    int rc = luaL_loadbufferx(vm, source, strlen(source), "page", "t");
    if (rc == LUA_OK) rc = lua_pcall(vm, 0, results, 0);
    lua_sethook(vm, NULL, 0, 0);
    return rc;
}
int ink_spike_build_model(void) {
    building = true;
    int rc = evaluate(ink_spike_source(), 0, 20000);
    building = false;
    if (rc != LUA_OK) { lua_pop(vm, 1); return -1; }
    return ink_spike_model->count == INK_SPIKE_ROWS ? 0 : -1;
}
int ink_spike_toggle(unsigned row, bool value, ink_source_t source) {
    caller = source;
    lua_getglobal(vm, "on_toggle");
    lua_pushinteger(vm, row);
    lua_pushboolean(vm, value);
    lua_sethook(vm, budget, LUA_MASKCOUNT, 20000);
    int rc = lua_pcall(vm, 2, 0, 0);
    lua_sethook(vm, NULL, 0, 0);
    if (rc != LUA_OK) lua_pop(vm, 1);
    return rc == LUA_OK ? 0 : -1;
}
void ink_spike_runtime_stop(void) { if (vm) lua_close(vm); vm = NULL; }
void ink_spike_tests(void) {
    int rc = evaluate("return io == nil and os == nil and debug == nil and package == nil", 1, 20000);
    ink_probe_value("spike.modules_hidden", rc == LUA_OK && lua_toboolean(vm, -1));
    lua_pop(vm, 1);
    rc = evaluate("while true do end", 0, 20000);
    ink_probe_value("spike.loop_stopped", rc == LUA_ERRRUN);
    lua_pop(vm, 1);
    void *over = allocate(NULL, NULL, 0, INK_SPIKE_VM_LIMIT + 1);
    ink_probe_value("spike.quota_rejected", over == NULL);
    if (over) allocate(NULL, over, INK_SPIKE_VM_LIMIT + 1, 0);
    rc = evaluate("set(8, true)", 0, 20000);
    ink_probe_value("spike.invalid_row_rejected", rc == LUA_ERRRUN);
    lua_pop(vm, 1);
}
