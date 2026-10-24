# 从源码编译

## 固件

历史固件用 ESP-IDF 5.5.3 构建。推荐同版本，ESP32-S3，16MB Flash / 8MB PSRAM。先按 Espressif 官方文档安装并进入 ESP-IDF 环境，再运行：

```sh
python tools/build-firmware.py
```

Windows 建议把仓库克隆到简短的英文路径，例如 `C:/dev/SparkBot-XiaoBai`，避免 GNU 工具在非 ASCII 路径下的问题。入口使用当前 ESP-IDF 环境，应用 `sdkconfig.defaults`、`sdkconfig.defaults.esp32s3` 和 `sdkconfig.sparkbot`，不会改全局配置。

产物默认在 `firmware/build/`。组件管理器需要下载锁定依赖；本地 Wi-Fi 修订组件在 `firmware/components/esp-wifi-connect`，不使用开发者电脑的绝对路径。

同样的源码在其他 IDF/编译器版本下不保证产生逐字节一致的二进制。本次发布包保留历史已刷入固件，新的便携编译入口没有做全量重新编译验收。

## Android

原生 Java + WebView，Android min SDK 26 / target SDK 35。需要 JDK 17、Android SDK Platform 35、Build Tools 35.0.0。设置 `JAVA_HOME`、`ANDROID_SDK_ROOT` 后运行：

```sh
python tools/build-android.py
```

首次本地构建会在 `android/.signing/` 创建本地签名材料，已在 .gitignore 排除。不要上传密钥和密码。自己的新签名不能直接覆盖发布包签名；可换应用 ID 或先自行卸载旧包，卸载可能清掉手机本地设置。共享设备名称/配色保存在机器人上。

发布包的 APK 仍使用既有签名，本仓库不分发该私钥。便携脚本只验证语法及调用参数，未在干净 SDK 环境重建验收。

## 公共测试

需要 C++17 编译器、Node.js 和 Python 3。在仓库根目录运行：

```sh
python tools/run-host-tests.py
node tools/remote-input-test.cjs
python tools/ui-options-test.py
```

测试通过不等同于真实电极、摄像头、音响或 Android 实机体验均验证通过。
