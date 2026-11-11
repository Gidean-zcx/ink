#!/usr/bin/env python3
"""Manual checks on complete functions extracted from the actual fixed factory checkout."""
import argparse
from pathlib import Path
import re
import resource
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('upstream', type=Path)
parser.add_argument('build', type=Path)
args = parser.parse_args()
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
source = (args.upstream / 'main/boards/metalio-e-ink4-plus/display/output_lcd/lcd_driver.c').read_text()

def function(name):
    match = re.search(r'^(?:static )?(?:IRAM_ATTR )?(?:void|bool|int) ' + name + r'\([^;]*?\) \{', source, re.M)
    assert match, name
    start = match.start()
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end] + '\n'

prefix = r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define CONFIG_IDF_TARGET_ESP32S31 1
#define IRAM_ATTR
#define GDMA_LL_EVENT_TX_EOF 1
#define LCD_LL_EVENT_VSYNC_END 2
#define LCD_LL_EVENT_TRANS_DONE 4
#define ESP_ERROR_CHECK(x) assert((x) == 0)
typedef struct { int bus_width, dummy_clocks_after_data; } LcdEpdConfig_t;
static struct {
    void *dma_dev, *dma_chan;
    int dma_channel, irq_core;
    struct { void *dev; } hal;
    bool frame_finishing;
    int64_t frame_t0_us, last_frame_us;
    void (*frame_done_cb)(void *);
    bool (*line_source_cb)(void *, uint8_t *);
    void *frame_cb_payload;
} lcd;
static bool eof_enabled = true, lcd_enabled = true, pending = true, stopped;
static unsigned reads, done;
static void axi_dma_ll_tx_enable_interrupt(void *d, int c, int events, bool enable) { (void)d; (void)c; (void)events; eof_enabled = enable; }
static void axi_dma_ll_tx_clear_interrupt_status(void *d, int c, uint32_t events) { (void)d; (void)c; assert(events == UINT32_MAX); pending = false; }
static void lcd_ll_enable_interrupt(void *d, int events, bool enable) { (void)d; (void)events; lcd_enabled = enable; }
static int64_t esp_timer_get_time(void) { return 123; }
static void lcd_ll_stop(void *d) { (void)d; stopped = true; }
static void gdma_stop(void *d) { (void)d; }
static void gdma_reset(void *d) { (void)d; assert(stopped && !eof_enabled && !lcd_enabled); }
static int lcd_ll_get_interrupt_status(void *d) { (void)d; return 0; }
static void lcd_ll_clear_interrupt_status(void *d, int events) { (void)d; (void)events; }
static int esp_ipc_call_blocking(int core, void (*fn)(void *), void *ctx) { assert(core == 1 && !eof_enabled && !lcd_enabled); fn(ctx); return 0; }
static bool line(void *ctx, uint8_t *bytes) { (void)ctx; (void)bytes; reads++; return false; }
static void finished(void *ctx) { (void)ctx; done++; }
'''
functions = ''.join(function(name) for name in [
    'use_native_scan_timing', 'enable_dma_eof', 'clear_dma_pending',
    'lcd_finish_frame_from_isr', 'abort_frame_on_irq_core', 'epd_lcd_abort_frame'])
geometry_start = source.index('    lcd.display_lines = use_native_scan_timing(config)')
geometry_end = source.index(';', geometry_start) + 1
geometry = source[geometry_start:geometry_end].replace('lcd.display_lines', 'display_lines')
suffix = r'''
int main(void) {
    LcdEpdConfig_t cfg = {.bus_width = 8, .dummy_clocks_after_data = 10};
    LcdEpdConfig_t *config = &cfg;
    int display_height = 684, display_lines = 0;
GEOMETRY
    assert(display_lines == 684);
    lcd.irq_core = 1;
    lcd.frame_t0_us = 1;
    lcd.line_source_cb = line;
    lcd.frame_done_cb = finished;
    lcd.line_source_cb(NULL, NULL);
    assert(reads == 1);
    epd_lcd_abort_frame();
    assert(stopped && !pending && !eof_enabled && !lcd_enabled && !lcd.line_source_cb && done == 0);
    lcd.line_source_cb = line; /* Reusing the queue is now safe: old EOF is disabled and cleared. */
    if (pending && eof_enabled) lcd.line_source_cb(NULL, NULL);
    assert(reads == 1);
    return 0;
}
'''.replace('GEOMETRY', geometry)
program = prefix + functions + suffix
mutations = [
    ('native-684-lines', 'use_native_scan_timing(config) ? display_height', 'false && use_native_scan_timing(config) ? display_height'),
    ('abort-no-completion', 'lcd_finish_frame_from_isr(false);\n    gdma_reset', 'lcd_finish_frame_from_isr(true);\n    gdma_reset'),
    ('abort-clears-old-eof', '    clear_dma_pending();', '    (void)clear_dma_pending;'),
]
args.build.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix='factory-display-', dir=args.build.resolve()) as temp:
    work = Path(temp)
    for name, old, new in [('baseline', '', '')] + mutations:
        text = program
        if old:
            assert text.count(old) == 1, name
            text = text.replace(old, new)
        (work / 'test.c').write_text(text)
        built = subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-UNDEBUG', str(work / 'test.c'), '-o', str(work / 'test')], capture_output=True, text=True)
        assert built.returncode == 0, built.stderr
        result = subprocess.run([str(work / 'test')], capture_output=True, text=True)
        assert (result.returncode == 0) == (name == 'baseline'), f'{name}: {result.stderr}'
        print(f'{name}: compile=0, test={result.returncode}')
print('factory display functions: native geometry and abort passed; 3/3 reverse controls rejected')

# These source/config properties complement the target ELF section inspection.
queue = (args.upstream / 'main/boards/metalio-e-ink4-plus/display/output_common/line_queue.c').read_text()
settings = (Path(__file__).resolve().parents[1] / 'sdkconfig.defaults').read_text()
def cache_contract(lcd_source, queue_source, config):
    assert 'static IRAM_ATTR void start_ckv_cycles(int cycles)' in lcd_source
    assert 'heap_caps_calloc(queue.size, sizeof(uint8_t*), MALLOC_CAP_INTERNAL)' in queue_source
    assert 'CONFIG_GDMA_CTRL_FUNC_IN_IRAM=y' in config
    assert 'CONFIG_GDMA_ISR_HANDLER_IN_IRAM=y' in config

cache_contract(source, queue, settings)
changes = [
    (source.replace('static IRAM_ATTR void start_ckv_cycles', 'static void start_ckv_cycles'), queue, settings),
    (source, queue.replace('heap_caps_calloc(queue.size, sizeof(uint8_t*), MALLOC_CAP_INTERNAL)', 'calloc(queue.size, sizeof(uint8_t*))'), settings),
    (source, queue, settings.replace('CONFIG_GDMA_CTRL_FUNC_IN_IRAM=y', '')),
    (source, queue, settings.replace('CONFIG_GDMA_ISR_HANDLER_IN_IRAM=y', '')),
]
for changed in changes:
    try:
        cache_contract(*changed)
    except AssertionError:
        continue
    raise AssertionError('missing IRAM protection accepted')
print('IRAM source/config contract: baseline passed; 4/4 reverse controls rejected (static checks)')
