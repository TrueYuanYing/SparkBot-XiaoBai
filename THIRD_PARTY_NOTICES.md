# 来源与许可证

- 小智固件基础：`https://github.com/78/xiaozhi-esp32`，tag `v1.9.4`，commit `3ced7709c65a39494f5684e99111854a5bcbd8c7`。原始许可证位于 `firmware/LICENSE`。
- 木鱼图像和声音、底座协议、Factory 功能参考：`https://gitee.com/esp-friends/esp_sparkbot`。原始许可证一并保存在 `firmware/main/boards/esp-sparkbot/assets/FACTORY_LICENSE`。
- BMI270：Bosch Sensortec `https://github.com/boschsensortec/BMI270_SensorAPI`，许可证和版权通知保存在 `firmware/components/bmi270/LICENSE` 与原代码文件头。
- BMI220：Bosch Sensortec v2.47.1 初始化数据，来自 `https://github.com/coreboot/chrome-ec/tree/main/third_party/bmi220`，原文件 Git blob `88b4b08e08c974471c129b8ee8629b9387d00121`；BSD-3-Clause 许可证位于 `firmware/components/bmi270/bmi220/LICENSE`。仅修改 C++ 常量声明/编译断言，8192 字节数据保持原样。初始化流程参考 Chromium EC `driver/accelgyro_bmi260.c`。
- ESP-IDF、ESP-SR、esp32-camera、LVGL、音频和网络组件的版本锁定于 `firmware/dependencies.lock`，许可证位于各下载组件目录。

本地新增内容：设备网页、统一应用管理、白色双眼动画、共享摄像头访问、控制时限和测试脚本。原有项目的许可证和版权声明继续适用。

0.4 应用图标：通过内置 image_gen 生成原创圆角彩色图集，按用户要求裁剪为独立 PNG 与机身 RGB565A8。没有使用小米原始图标或商标；原图及提示词保存在 `firmware/main/boards/esp-sparkbot/assets/icons`。
- Wi-Fi 连接库：78/esp-wifi-connect 2.6.2，https://github.com/78/esp-wifi-connect ，MIT，Copyright (c) 2024 Terrence。本地副本位于 `firmware/components/esp-wifi-connect`；仅修正两处扫描重试定时器的微秒单位。许可证随固件包附带。


0.8–0.9 UI 交互参考：https://github.com/AstraThreshold/oled-ui-astra 及其官方 Wiki 的分层图标菜单、选择器与动画思路。本次继续使用 LVGL，自行实现布局和缓动，未复制 Astra 的源代码或资源。
