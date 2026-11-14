# Mogo 读音优先筛选（文本服务这一侧）

- 项目：`github.com/chopper1026/mogo-e4p`，固定提交 `864f2f9dc75980a67d2d6b67b17365020b7a50ce`，取自本机只读副本 `ink-hw/upstream/oss/mogo-e4p`。

| 我们的文件 | 上游文件和行 | 上游 SHA-256 | 许可（上游文件头） |
| --- | --- | --- | --- |
| `preferred.h` | `main/ui/ui_ime.c:1-4`（文件头）、`:91-93`（`preferred_ok()`） | `76b8a548bbca9fbbd2c747d681fc2d242b4ae23bda3f49996fc6ae0e0bb1af80` | Apache-2.0 |
| `LICENSE` | `third_party/mogo/licenses/Apache-2.0.txt` 复制 | `c2ede378097fb1fcdcd128c47880c918663f475eba53e1e2dc59d95b537c8809` | Apache-2.0 全文 |

## 改了什么

- 函数体没改，只在前面加了 `stdbool.h`、`stddef.h`、`string.h` 三个头文件。
- 调用点（我们写的接线，各一行）：
  - `pinyin.c` 的 `py_lookup_nine`：`prefix(composing, roman, true)` 之前 `if (!preferred_ok(roman, ink_text_preferred_reading(), 0)) continue;`。
  - `google_pinyin.cpp` 的 `nine()`：`prefix(digits + pos, part, true)` 之前 `if (!preferred_ok(part, ink_text_preferred_reading(), pos)) continue;`（`pos == 0` 是第一个音节，原文“不是第一个音节就放行”）。C++ 直接包含这个只用 `string.h` 的头文件，`ink_text_preferred_reading()` 走 `textinput.h` 的 `extern "C"`。
- 文本服务加的三个函数在 `text_service.c` 的 `flush_composing` 上面同一段：`ink_text_prefer_reading`、`ink_text_preferred_reading`、`ink_text_clear_composing`；选中的读音在选词、删一位、`flush_composing`、重输时清空（照 Mogo：删除和选词清空，继续按数字保留）。

## 许可范围、原文测试

同 `system/ui/shell/vendor/mogo/SOURCE.md`：这个文件头是 Apache-2.0，不是个人学习许可；原文没有能带的测试。
