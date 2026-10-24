# 下载、刷写与连接

## 选对包

`SparkBot-XiaoBai-v0.9.1.zip` 适用于 ESP-SparkBot 的 ESP32-S3 头部，16MB Flash、8MB PSRAM、240×240 屏幕。不要给 ESP32-C2 履带底座刷这个包。

发布标签是 v0.9.1，设备中显示的整合版本仍是 XiaoBai 0.9，基础固件描述为 XiaoZhi 1.9.4。本次只对现有固件打包，没有重新编译或改变灵敏度。

包内含应用、引导程序、分区表、OTA 初始化数据、语音模型、签名 Android APK、各文件 SHA256 和许可。没有设备 NVS、个人联网信息或原机完整备份。

## 先备份

安装 Python 和 esptool 后，按实际端口运行（`COM4` 只是例子）：

```sh
python -m pip install "esptool>=5,<6"
python -m esptool --chip esp32s3 --port COM4 read-flash 0x0 0x1000000 my-sparkbot-private-backup.bin
```

这份备份可能含个人配置，自己保存，不上传到 GitHub。首次安装需要确认硬件和分区兼容；已有其他定制固件时不要只凭型号盲刷。

## 校验与刷写

解压固件包进入目录：

```sh
python flash.py --verify-only
```

已经使用本整合版、分区相同，只更新应用并保留 NVS：

```sh
python flash.py --port COM4
```

首次安装，在完成备份和兼容性确认后：

```sh
python flash.py --port COM4 --initial
```

脚本先核对 manifest 中的 SHA256，不执行全片擦除。首次安装会写引导程序、分区表、OTA 初始化数据、应用和模型；“只更新应用”仅写 0x100000。OTA 初始化数据不是个人 NVS，但会重设启动分区选择。

| 地址 | 文件 |
| --- | --- |
| 0x0 | bootloader.bin |
| 0x8000 | partition-table.bin |
| 0xd000 | ota_data_initial.bin |
| 0x100000 | xiaozhi.bin |
| 0xd00000 | srmodels.bin |

## 连上小白

1. 开机放稳约 5 秒，等待触摸校准。
2. 手机连接 `SparkBot-XXXX`，默认密码 `sparkbot2026`。
3. 浏览器打开 `http://192.168.4.1/`，注意使用 http。
4. 如果显示急停，先点击“解除急停”；摇杆按住移动，松手停车。
5. 设置里可配机器人上网使用的 2.4GHz Wi-Fi。语音对话需要用户自己的小智服务配置及互联网。

热点网址在重启后固定。同一个家庭网络中，也可使用设备设置页的 LAN IP，但 DHCP 地址可能变化。校园/访客网络若隔离客户端，发现和控制可能不可用。

## 手机客户端

Android 8.0+ 可安装 `XiaoBai-0.9.apk`，也可连接小白热点后访问 `http://192.168.4.1/xiaobai.apk`。按系统提示允许该次来源安装。首次打开连接相同 Wi-Fi，扫描或手动填写 IP。APK 的手机实装兼容性仍需设备反馈。

iPhone 用 Safari 打开遥控网页后“分享 → 添加到主屏幕”。当前未提供 iOS 原生安装包。

## 遇到问题

- 网页打不开：确认手机真的连接目标 Wi-Fi，热点下 IP 应是 192.168.4.x；不要把家庭 LAN IP 与热点固定 IP 混用。
- 点击不切页：检查是否启用双眼保持；保持时仍可摸头揉脸。触摸总开关可以关闭全部扫描。
- 对话不回复：先确认机器人上网和小智账户配置，再记录状态；本地遥控可在没有互联网时使用。
- 单个位置触摸差异：先注明具体位置和硬件批次，避免同时捏住多个触摸区；用户反馈优先于自动测试通过。
