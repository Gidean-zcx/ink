# 试用版说明（2026-11-11 早）

这是我们自己的系统第一次能在这台机器上日常试用的版本。2026-11-10 晚上在设备上跑了十几轮，每轮都照整片备份写回、核对一致。上机记录见 [2026-11-10-trial.md](../hardware/2026-11-10-trial.md)。

## 现在设备上是什么

**试用版已经装好，在运行**（2026-11-11 19:24 换装，版本 `gbf0ad40c`，镜像和 ELF 在 `ink-hw/trial-20261111/delivery-15/`：4G 外置卡能注册、能上网、重启后自动恢复上网，设置里可以手选内置卡或外置卡（D-128）；摄像头、蓝牙耳机、充电管理照原厂；键盘有中英切换、中文词库在闪存里。再之前是 13:2x 的 `ga9a3eeb1`：灰底改成白底细边框、换页不闪且按次数补干净刷新（D-127）、电池分格；镜像和 ELF 在 `ink-hw/trial-20261111/delivery-3/`。之前的 `g2d550dea`、`gdffba4c5` 在 `delivery/`、`delivery-2/`，第一版 `g1390e166` 是 01:00 装的）。

注意：锁屏以后画面不再变化（包括时间），按电源键解锁。

## 能用的

- 开机进主页，屏幕用我们自己的刷新算法（D-110）。每次开机先证明屏幕保护有效，才会驱动屏幕。
- 触摸、按键、马达、前光、电量和充电、时钟芯片、加速度计。
- 设置、家庭成员、孩子模式、时钟和闹钟、日历、计算器、文件、阅读、录音备忘。存储卡可以读写。
- Wi-Fi：在设置里连上以后，自动对时。
- 喇叭和麦克风。

## 这版还没有的

- **AI**：要经过家庭中枢服务器，中枢的真实模型还没做。
- **电话、短信**：4G 模组还没接。
- 蓝牙耳机、摄像头、唤醒词。
- **睡眠**：浅睡还没在真机上验证过，这版不睡，所以电掉得比出厂固件快。不用时可以长按电源键关机。

## 遇到问题怎么告诉我

1. 按电源键打开电源菜单，点“记录问题”。屏幕会提示“已记录问题 #编号”。
2. 告诉我编号，再说一句：在哪个页面、做了什么、看到了什么。
3. 来不及记也没关系，说个大概时间就行，日志一直在记。

设备里有一段 512 KiB 的日志环，崩溃前最后的日志和崩溃现场也会留下。你把设备用 USB 连上电脑，我就能拉日志来查，不用你动手。

## 想回到出厂固件

跟我说一声就行。你自己也可以在终端运行：

```bash
cd ~/Documents/Workspace/ink-hw && venv/bin/python ../ink/tools/device/trial_run.py --restore-only --port '/dev/cu.usbmodem*' --backup backup-20261110/private/flash_full_1.bin --out trial-20261110/restore
```

它会把这次动过的五个区（otadata、ota_1、model、font_data、coredump）全部照备份写回，逐段核对，然后启动出厂固件。另外有个安全网：试用版如果开机 60 秒内连续崩溃 3 次，会自己切回出厂固件。

## 装新版（主会话做）

交付目录里镜像旁边要有构建出的装机包 `card-files/`（`build-s31/card-files/` 整个拷过去）：新阅读要的字体在里面，装好后由装机工具经 USB 串口自动放到存储卡上（D-138，见 [card-files.md](../system/card-files.md)），不用进 U 盘模式。

```bash
cd ~/Documents/Workspace/ink-hw && venv/bin/python ../ink/tools/device/trial_run.py --install --port '/dev/cu.usbmodem*' --backup backup-20261110/private/flash_full_1.bin --image trial-20261111/delivery-N/ink-s31.bin --fonts trial-20261111/delivery-N/ink-fonts.bin --out trial-20261111/install-N
```

- 没有 `card-files/` 时它在写任何东西之前停下（退出 2）；确实不要卡上文件时加 `--no-card-files`。
- 退出码 5：装好了、在运行，但卡上文件没有全部到位，看输出目录里的 `card_files.txt`。
- 装好以后字体被删了之类：`--card-files-only --card-files 交付目录/card-files [--reset]` 只服务装机包，不要备份和镜像，不写闪存（`--reset` 让设备重新开机核对）。

## 闪存怎么用的

见 [flash-layout.md](../hardware/flash-layout.md)。出厂固件、原厂设置（nvs）、引导程序和分区表都没动过。
