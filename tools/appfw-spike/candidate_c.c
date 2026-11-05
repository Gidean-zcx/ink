#include <string.h>
#include "spike.h"

int ink_spike_runtime_start(void) { return 0; }

int ink_spike_build_model(void) {
    *ink_spike_model = (ink_spike_model_t){
        .title = "阅读偏好", .field_label = "备注", .text = "每天读一会儿",
        .button = "完成", .count = INK_SPIKE_ROWS,
        .rows = {{"显示进度", true}, {"显示页码", false}, {"记住位置", true},
                 {"阅读提醒", false}, {"词语提示", true}, {"显示注释", false},
                 {"听书续播", true}, {"显示书名", false}}};
    return 0;
}

int ink_spike_toggle(unsigned row, bool value, ink_source_t source) {
    return ink_spike_dispatch(row, value, source);
}
void ink_spike_runtime_stop(void) {}
void ink_spike_tests(void) {}
