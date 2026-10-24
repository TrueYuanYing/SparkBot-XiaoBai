# 离线屏幕重放

这套工具用于生成文档中的屏幕动图，不参与固件构建或设备运行。

`prepare.py` 从仓库固件源码提取双眼绘制、经典小智布局、应用页面和缓动函数，使用同版本 LVGL 9.3 及 78/xiaozhi-fonts 字体编译成 PC 程序。硬件服务替换为本地桩，演示驱动设置确定的触摸、对话和游戏状态；时钟改接本地演示时钟。不会连接网络服务、驱动电机或扫描真实触摸电极。

类字段仅在重放程序中开放给演示输入。图形、布局、文字、字体、图像和过渡来自固件；这仍是 PC 渲染预览，不能据此测量 ESP32 帧率或确认云端识别能力。

## 重放

先按固件构建说明下载依赖，使 `firmware/managed_components/lvgl__lvgl` 和 `78__xiaozhi-fonts` 存在。需要 CMake、C++17 编译器，推荐 Ninja。在 Windows 上使用 Developer Command Prompt，并把仓库放在英文路径，避免旧 Ninja 对中文依赖路径的限制。

```sh
python tools/visual-preview/prepare.py
cmake -S visual-preview-output/renderer -B visual-preview-output/renderer/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build visual-preview-output/renderer/build
```

在 `visual-preview-output/` 下新建 `frames/`，进入 `renderer/` 后运行（Windows 使用 `.exe`）：

```sh
build/screen-replay ../../firmware/main/boards/esp-sparkbot/assets ../frames ../../tools/visual-preview/camera-sample.rgb565
```

程序输出 `240×240` 的 RGB565 little-endian 帧，每 40ms 保存一次，共 34 个场景及 `scenes.tsv`。用 FFmpeg 转成 GIF 的例子：

```sh
ffmpeg -f rawvideo -pixel_format rgb565le -video_size 240x240 -framerate 25 -i visual-preview-output/frames/eyes-idle.rgb565 -vf "split[a][b];[a]palettegen=max_colors=128[p];[b][p]paletteuse=dither=bayer" -loop 0 eyes-idle.gif
```

`camera-sample.rgb565` 是用户授权演示视频中截取的一帧，供摄像头页展示预录输入，属于演示媒体范围，不作为实际相机测试。音量、Wi-Fi 名称、时钟、对话文字等均为示例值。

已完成 Windows MSVC 构建与全部 34 场景输出；其他平台的重建尚未实测。
