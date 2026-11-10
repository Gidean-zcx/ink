# Opus 依赖

- 原厂 `main/idf_component.yml:8` 使用 `78/esp-opus-encoder ~2.4.1`；其 2.4.1 依赖 `78/esp-opus ^1.0.5`。
- 本端口直接调用同一个 C API，不引入 C++ 包装的向量和队列；固定为 `78/esp-opus 1.0.5`，用该组件自己的固定点源码列表和编译参数。16 kHz、单声道、60 ms、VOIP、DTX 开启，码率与复杂度由现有 HAL 参数给出。
- [注册表](https://components.espressif.com/components/78/esp-opus/versions/1.0.5/readme)，[2.4.1 包装源码](https://github.com/78/esp-opus-encoder)，查阅日期：2026-11-10。
- 注册表压缩包 SHA256：`f04982787072427616f2ef86e228844d4912f0a6952a2ee597bb4dc6e5ced829`。组件声明来源提交 `5854a9f7de06ab3505b8fe6e8943db581c2cbe70`。
- 构建时下载并检查哈希，源码及原有版权保留在构建目录。离线时在对应构建目录预置同哈希的 `esp-opus-1.0.5.zip`；缺失或哈希不符会明确报错，不生成无声桩。
- 注册表标成 MIT，但库内 `COPYING` 是 BSD 三条款及免版税专利声明；以随附 [COPYING](COPYING) 为准。原厂适配部分遵守其 MIT 许可证（`../s31_sleep/LICENSE`）。
