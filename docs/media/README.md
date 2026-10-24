# 演示媒体范围

- `device-apps-touch-demo.gif`、`device-expressions-demo.gif` 及对应封面来自使用者提供的两段实机视频；完整 H.264/AAC MP4 放在 v0.9.1 Release。视频只做兼容性压缩和附加元数据移除，原声保留。
- 实机视频反映拍摄时状态，未额外核实拍摄时固件哈希，不能把它当作本次重新进行的完整验收。
- `screens/` 的 34 组 GIF 为实际固件绘制函数在 PC 上的 LVGL 9.3 重放；25fps 是文件编码频率，非面板或 ESP32 测量值。
- `phone/` 使用真实设备网页，在浏览器里操作控件并截图；设备状态由离线示例数据提供，没有连接实际机器人。
- `render-provenance.json` 记录绘制源文件 SHA256；具体方法见 [动态画廊](../GALLERY.md) 与 [重放工具](../../tools/visual-preview/README.md)。

视频及视频截取得到的 GIF、封面和摄像头样例由使用者授权在本项目展示，未另行指定开放许可，不属于代码的 MIT 授权范围。需要转载或另作使用时，请联系项目发起者。屏幕渲染中包含上游字体、Factory 木鱼资源和本项目图标，其来源许可继续适用，详见 [来源说明](../../THIRD_PARTY_NOTICES.md)。
