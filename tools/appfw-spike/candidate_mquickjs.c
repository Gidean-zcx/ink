#include <string.h>
#include "mquickjs.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "spike.h"

#define INK_SPIKE_VM_LIMIT (128u * 1024u)
static JSContext *vm;
static void *memory;
static unsigned polls;
static bool building;
static ink_source_t caller;

static JSValue error(JSContext *ctx) {
    return JS_ThrowTypeError(ctx, "invalid page or action");
}
static int copy(JSContext *ctx, JSValue value, char *dst, size_t cap) {
    if (!JS_IsString(ctx, value)) return -1;
    JSCStringBuf buf;
    size_t len;
    const char *s = JS_ToCStringLen(ctx, &len, value, &buf);
    if (!s || len >= cap || memchr(s, 0, len)) return -1;
    memcpy(dst, s, len);
    dst[len] = 0;
    return 0;
}
static JSValue ink_js_title(JSContext *ctx, JSValue *self, int argc, JSValue *argv) {
    (void)self;
    if (!building || argc != 1 || copy(ctx, argv[0], ink_spike_model->title, 64)) return error(ctx);
    return JS_UNDEFINED;
}
static JSValue ink_js_toggle(JSContext *ctx, JSValue *self, int argc, JSValue *argv) {
    (void)self;
    unsigned i = ink_spike_model->count;
    if (!building || argc != 2 || i >= INK_SPIKE_ROWS || !JS_IsBool(argv[1]) ||
        copy(ctx, argv[0], ink_spike_model->rows[i].label, 64)) return error(ctx);
    ink_spike_model->rows[i].value = argv[1] == JS_TRUE;
    ink_spike_model->count++;
    return JS_UNDEFINED;
}
static JSValue ink_js_field(JSContext *ctx, JSValue *self, int argc, JSValue *argv) {
    (void)self;
    if (!building || argc != 2 || copy(ctx, argv[0], ink_spike_model->field_label, 64) ||
        copy(ctx, argv[1], ink_spike_model->text, 128)) return error(ctx);
    return JS_UNDEFINED;
}
static JSValue ink_js_button(JSContext *ctx, JSValue *self, int argc, JSValue *argv) {
    (void)self;
    if (!building || argc != 1 || copy(ctx, argv[0], ink_spike_model->button, 64)) return error(ctx);
    return JS_UNDEFINED;
}
static JSValue ink_js_set(JSContext *ctx, JSValue *self, int argc, JSValue *argv) {
    (void)self;
    int row;
    if (building || argc != 2 || !JS_IsInt(argv[0]) || !JS_IsBool(argv[1]) ||
        JS_ToInt32(ctx, &row, argv[0]) || row < 0 || row >= INK_SPIKE_ROWS ||
        ink_spike_dispatch((unsigned)row, argv[1] == JS_TRUE, caller)) return error(ctx);
    return JS_UNDEFINED;
}
#include "ink_mquickjs_stdlib.h"

static int interrupt(JSContext *ctx, void *opaque) {
    (void)ctx;
    (void)opaque;
    return ++polls >= 2;
}
int ink_spike_runtime_start(void) {
    const char *prev = osal_mem_tag("appfw.mquickjs");
    memory = osal_malloc(INK_SPIKE_VM_LIMIT, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!memory) return -1;
    ink_spike_vm_peak = INK_SPIKE_VM_LIMIT;
    vm = JS_NewContext(memory, INK_SPIKE_VM_LIMIT, &js_stdlib);
    if (!vm) {
        osal_free(memory);
        memory = NULL;
        return -1;
    }
    JS_SetInterruptHandler(vm, interrupt);
    return 0;
}
static JSValue evaluate(const char *source, int flags) {
    polls = 0;
    return JS_Eval(vm, source, strlen(source), "page", flags);
}
static int result(JSValue value) {
    if (!JS_IsException(value)) return 0;
    JS_GetException(vm);
    return -1;
}
int ink_spike_build_model(void) {
    building = true;
    int rc = result(evaluate(ink_spike_source(), 0));
    building = false;
    return !rc && ink_spike_model->count == INK_SPIKE_ROWS ? 0 : -1;
}
int ink_spike_toggle(unsigned row, bool value, ink_source_t source) {
    caller = source;
    polls = 0;
    if (JS_StackCheck(vm, 4)) return -1;
    /* The C API stack takes arguments in reverse order. */
    JS_PushArg(vm, value ? JS_TRUE : JS_FALSE);
    JS_PushArg(vm, JS_NewInt32(vm, (int)row));
    JSValue function = JS_GetPropertyStr(vm, JS_GetGlobalObject(vm), "on_toggle");
    JS_PushArg(vm, function);
    JS_PushArg(vm, JS_NULL);
    return result(JS_Call(vm, 2));
}
void ink_spike_runtime_stop(void) {
    if (vm) JS_FreeContext(vm);
    vm = NULL;
    osal_free(memory);
    memory = NULL;
}
void ink_spike_tests(void) {
    JSValue value = evaluate("typeof load === 'undefined' && typeof Date === 'undefined' && "
                            "typeof print === 'undefined' && typeof setTimeout === 'undefined' && "
                            "typeof require === 'undefined' && typeof eval === 'undefined'", JS_EVAL_RETVAL);
    ink_probe_value("spike.modules_hidden", value == JS_TRUE);
    int rc = result(evaluate("while (true) {}", 0));
    ink_probe_value("spike.loop_stopped", rc < 0 && polls == 2);
    ink_probe_value("spike.quota_rejected", result(evaluate("'x'.repeat(1048576)", 0)) < 0);
    ink_probe_value("spike.invalid_row_rejected", result(evaluate("set(8, true)", 0)) < 0);
    ink_probe_value("spike.invalid_type_rejected", result(evaluate("set(0, 'true')", 0)) < 0);
    ink_probe_value("spike.subset_rejected", result(evaluate("var holes = [1,,3]", 0)) < 0);
    ink_probe_value("spike.recovered", evaluate("1 + 1 === 2", JS_EVAL_RETVAL) == JS_TRUE);
}
