# 节气日期表来源

查阅日期：2026-11-12。

- 上游：[OPN48/cnlunar](https://github.com/OPN48/cnlunar/tree/3f674fe0f458b480c79eaaf5464f43f4bc815a91)，固定提交 `3f674fe0f458b480c79eaaf5464f43f4bc815a91`。
- 取 `cnlunar/config.py` 的 `ENC_VECTOR_LIST`、`SOLAR_TERMS_NAME_LIST`、`SOLAR_TERMS_DATA_LIST`；保留 200 年的原始数值，使用同一按低位到高位解包规则。不是世纪常数近似公式。
- 上游将数据标为香港天文台 1901～2100 年节气民用日期，时区 UTC+08:00；本接口不提供节气瞬时或其他时区换算。普通日期、范围外日期返回空值。
- 上游根许可 MIT，版权 OPN48，全文见 [CNLUNAR-LICENSE](CNLUNAR-LICENSE)。上游注明的原数据出处为[香港天文台公历与农历日期对照表](https://www.hko.gov.hk/sc/gts/time/conversion.htm)。
- `lunar.c` 只新增查询；调用声明暂留唯一调用方 `faces.c`，任务不允许修改公共时间头文件。合并时可把声明移到 `ink/sys/time.h`。

| 上游文件 | SHA-256 |
| --- | --- |
| `LICENSE` | `8bc77e1f9ab5c48cfc9e532b5eb30ff02c67cdc14af27b88b9c1ae815f1364bc` |
| `cnlunar/config.py` | `aa804333238a07633057f614ae611798cc64da4621549940bb0d2025c2ab478a` |
| `cnlunar/solar24.py` | `a284dbe276637fb27d93f12c608c234b516cd4fbb1201fa2415df27a49886bf1` |
