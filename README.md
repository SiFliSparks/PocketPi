# PocketPi

[English](README_EN.md) | [中文](README.md)

一个基于 sf32lb52 模组的嵌入式游戏/演示工程（PocketPi）。此仓库包含源代码、构建脚本与运行所需的资源目录。

## 支持的开发板
- [立创·黄山派SF32LB52-ULP开发板](https://lckfb.com/project/detail/lckfb-hspi-sf32lb52-ulp?param=baseInfo)

## 快速开始

### 克隆仓库

```
git clone https://github.com/SiFliSparks/PocketPi.git
```

### 编译和烧录

切换到project目录，运行scons命令执行编译：
```
scons --board=sf32lb52-lchspi-ulp -j32
```

运行`build_sf32lb52-lchspi-ulp_hcpu\uart_download.bat`，按提示选择端口即可进行下载：

```
build_sf32lb52-lchspi-ulp_hcpu\uart_download.bat

Uart Download

please input the serial port num:5
```

### 运行游戏

烧录并启动设备后，固件会自动扫描设备上的 `disk/` 目录，列出目录中的可运行文件。请注意以下要点：

- 在构建或调试时，用户需自行准备要运行的 ROM/二进制文件并把它们放入仓库根的 `disk/` 目录中（请勿将受版权保护的 ROM 提交到仓库）。
- 启动后，设备会扫描 `disk/` 并在界面上显示文件列表。你可以在列表中选择某个文件并运行它。

示例流程：

1. 在本地把测试用 ROM 拷贝到 `disk/` 目录。
2. 启动设备，等待主界面扫描完成并显示列表。
3. 在界面中选择要运行的文件并确认，设备会载入并运行该文件。

### 运行效果
游戏列表：
![image](assets/game_list.jpg)
游戏运行：
![image](assets/game_running.jpg)

## 目录结构
仓库中主要文件和目录说明：

- `disk/`：运行时扫描的资源目录，放置本地测试用的 ROM/二进制文件，请勿提交受版权保护的 ROM。
- `project/`：构建脚本、板级配置和生成的构建产物。
- `src/`：工程源代码（C 源文件、第三方库子目录等）。
- `assets/`：示例图片和运行截图。
- `.gitignore`：忽略规则，已配置为忽略构建输出和常见 ROM 文件扩展名。
- `README.md`：工程介绍与使用说明（本文件）。

通常流程：编辑 `disk/`（放入本地测试文件）→ 运行 `scons` 构建 `project/` → 通过下载脚本烧录并在设备上选择运行。

## 硬件按键与 IO

本工程把游戏按键通过外部引出的 GPIO 实现，相关实现位于 `src/video_audio.c`（部分行为在 `src/main.c` 中被调用），下面是要点说明：

- 按键 GPIO 定义：
	- `int key_pin_def[] = {30,24,25,20,10,11,27,28,29};` （共 9 个按键，板上使用上拉输入）。
- 扫描与去抖：
	- 使用一个周期为 200 Hz 的软件定时器调用 `key_scan()` 进行轮询。
	- 每个按键维护一个 8-bit 的移位缓冲，需连续读到 `0xFF` 才判定为按下，`0x00` 判定为释放，从而实现去抖。
- 按键索引到游戏/系统事件的映射（参考 `ConvertGamepadInput()`）：
	- 索引 0：Select（当按下并配合索引 5 或 8 的释放时，会调整音频的 shift 值）
	- 索引 2：Start
	- 索引 3：B
	- 索引 4：A
	- 索引 5：Up
	- 索引 6：Right
	- 索引 7：Left
	- 索引 8：Down
	- 索引 1 当前未使用

- 接线建议：按键一端接相应 GPIO（如上数组中的编号），另一端接地。GPIO 配置为上拉，按下时引脚拉低为有效按下。

- 扩展说明：如需改变 GPIO 引脚或按键映射，请修改 `src/video_audio.c` 中的 `key_pin_def` 或 `ConvertGamepadInput()`，然后重新编译固件。

- 与 UI 的关系：主程序（`main.c`）使用 LVGL 显示文件列表并响应触摸/按键事件；物理按键主要在运行的 NES 仿真中作为手柄输入，也可用于系统级快捷操作（例如调整音频，目前音量可以使用select键+Up/Down键进行调整）。

## 参考链接
- [Sifli 官方仓库](https://github.com/OpenSiFli/SiFli-SDK/tree/main)
- [esplay-retro-emulation](https://github.com/pebri86/esplay-retro-emulation?tab=readme-ov-file)
- [LVGL 官方文档](https://docs.lvgl.io/latest/en/html/index.html)

## 常见问题
Q: 游戏列表中没有文件？
A: 请确保你已经把测试用的 ROM/二进制文件放入了 `disk/` 目录，并且这些文件的扩展名是受支持的（例如 `.nes`）。如果目录为空或没有受支持的文件，列表将不会显示任何内容。

如果你遇到任何问题或有改进建议，请在仓库的 Issues 页提交问题： https://github.com/SiFliSparks/PocketPi/issues
