# 小白的表情、页面与切换

[返回主页](../README.md) · [小智功能详解](XIAOZHI.md)

以下 **34 组机身 GIF** 用 LVGL 9.3 执行当前发布源码中的布局、字体、眼睛绘制及切屏动画生成。它们是源码渲染预览；小智状态、触摸、游戏操作、时间和摄像头输入由本地演示数据驱动，没有在此轮重新运行 ESP32 或云端模型。GIF 以 25fps 编码，不代表实物帧率。点击每张图可打开原图。

摄像头页使用用户录像中的一帧作预录示例；时钟是按演示时间推进的示例时钟；设置中的 `SparkBot-DEMO` 与音量是示例值。机甲渲染现有的待机、对话与触摸反馈，不虚构尚未实现的七套额外神态。

## 白色双眼的日常神态

<table>
<tr>
<td align="center"><b>常态/眨眼</b><br><a href="media/screens/eyes-idle.gif"><img src="media/screens/eyes-idle.gif" width="220" alt="常态/眨眼源码渲染预览"></a></td>
<td align="center"><b>开心</b><br><a href="media/screens/eyes-happy.gif"><img src="media/screens/eyes-happy.gif" width="220" alt="开心源码渲染预览"></a></td>
<td align="center"><b>好奇</b><br><a href="media/screens/eyes-curious.gif"><img src="media/screens/eyes-curious.gif" width="220" alt="好奇源码渲染预览"></a></td>
</tr>
<tr>
<td align="center"><b>困倦</b><br><a href="media/screens/eyes-sleepy.gif"><img src="media/screens/eyes-sleepy.gif" width="220" alt="困倦源码渲染预览"></a></td>
<td align="center"><b>惊讶</b><br><a href="media/screens/eyes-surprised.gif"><img src="media/screens/eyes-surprised.gif" width="220" alt="惊讶源码渲染预览"></a></td>
<td align="center"><b>眨单眼</b><br><a href="media/screens/eyes-winking.gif"><img src="media/screens/eyes-winking.gif" width="220" alt="眨单眼源码渲染预览"></a></td>
</tr>
<tr>
<td align="center"><b>害羞</b><br><a href="media/screens/eyes-shy.gif"><img src="media/screens/eyes-shy.gif" width="220" alt="害羞源码渲染预览"></a></td>
<td align="center"><b>坚定</b><br><a href="media/screens/eyes-confident.gif"><img src="media/screens/eyes-confident.gif" width="220" alt="坚定源码渲染预览"></a></td>
</tr>
</table>

## 触摸陪伴与对话

短碰有方向反馈，单一区域持续约 650ms 进入摸头/揉脸，松手后渐渐恢复。

<table>
<tr>
<td align="center"><b>左侧受击</b><br><a href="media/screens/eyes-hit-2.gif"><img src="media/screens/eyes-hit-2.gif" width="220" alt="左侧受击源码渲染预览"></a></td>
<td align="center"><b>右侧受击</b><br><a href="media/screens/eyes-hit-3.gif"><img src="media/screens/eyes-hit-3.gif" width="220" alt="右侧受击源码渲染预览"></a></td>
<td align="center"><b>顶部受击</b><br><a href="media/screens/eyes-hit-1.gif"><img src="media/screens/eyes-hit-1.gif" width="220" alt="顶部受击源码渲染预览"></a></td>
</tr>
<tr>
<td align="center"><b>摸摸头</b><br><a href="media/screens/eyes-pet-1.gif"><img src="media/screens/eyes-pet-1.gif" width="220" alt="摸摸头源码渲染预览"></a></td>
<td align="center"><b>左侧揉脸</b><br><a href="media/screens/eyes-pet-2.gif"><img src="media/screens/eyes-pet-2.gif" width="220" alt="左侧揉脸源码渲染预览"></a></td>
<td align="center"><b>右侧揉脸</b><br><a href="media/screens/eyes-pet-3.gif"><img src="media/screens/eyes-pet-3.gif" width="220" alt="右侧揉脸源码渲染预览"></a></td>
</tr>
<tr>
<td align="center"><b>聆听/说话与字幕</b><br><a href="media/screens/eyes-dialogue.gif"><img src="media/screens/eyes-dialogue.gif" width="220" alt="聆听/说话与字幕源码渲染预览"></a></td>
</tr>
</table>

## 机甲双眼的配套反馈

<table>
<tr>
<td align="center"><b>机甲待机</b><br><a href="media/screens/mecha-idle.gif"><img src="media/screens/mecha-idle.gif" width="220" alt="机甲待机源码渲染预览"></a></td>
<td align="center"><b>机甲对话光色</b><br><a href="media/screens/mecha-dialogue.gif"><img src="media/screens/mecha-dialogue.gif" width="220" alt="机甲对话光色源码渲染预览"></a></td>
<td align="center"><b>左侧受击</b><br><a href="media/screens/mecha-hit-2.gif"><img src="media/screens/mecha-hit-2.gif" width="220" alt="左侧受击源码渲染预览"></a></td>
</tr>
<tr>
<td align="center"><b>右侧受击</b><br><a href="media/screens/mecha-hit-3.gif"><img src="media/screens/mecha-hit-3.gif" width="220" alt="右侧受击源码渲染预览"></a></td>
<td align="center"><b>顶部受击</b><br><a href="media/screens/mecha-hit-1.gif"><img src="media/screens/mecha-hit-1.gif" width="220" alt="顶部受击源码渲染预览"></a></td>
<td align="center"><b>摸摸头</b><br><a href="media/screens/mecha-pet-1.gif"><img src="media/screens/mecha-pet-1.gif" width="220" alt="摸摸头源码渲染预览"></a></td>
</tr>
<tr>
<td align="center"><b>左侧揉脸</b><br><a href="media/screens/mecha-pet-2.gif"><img src="media/screens/mecha-pet-2.gif" width="220" alt="左侧揉脸源码渲染预览"></a></td>
<td align="center"><b>右侧揉脸</b><br><a href="media/screens/mecha-pet-3.gif"><img src="media/screens/mecha-pet-3.gif" width="220" alt="右侧揉脸源码渲染预览"></a></td>
</tr>
</table>

## 所有机身页面

<table>
<tr>
<td align="center"><b>双眼主页</b><br><a href="media/screens/eyes-idle.gif"><img src="media/screens/eyes-idle.gif" width="220" alt="双眼主页源码渲染预览"></a><br><small>常态眨眼和视线移动</small></td>
<td align="center"><b>机甲主页</b><br><a href="media/screens/mecha-idle.gif"><img src="media/screens/mecha-idle.gif" width="220" alt="机甲主页源码渲染预览"></a><br><small>光刃与呼吸光效</small></td>
<td align="center"><b>经典小智</b><br><a href="media/screens/classic-xiaozhi.gif"><img src="media/screens/classic-xiaozhi.gif" width="220" alt="经典小智源码渲染预览"></a><br><small>切回原有表情/消息显示</small></td>
</tr>
<tr>
<td align="center"><b>应用列表</b><br><a href="media/screens/page-menu.gif"><img src="media/screens/page-menu.gif" width="220" alt="应用列表源码渲染预览"></a><br><small>单击切换，双击进入</small></td>
<td align="center"><b>木鱼</b><br><a href="media/screens/page-muyu.gif"><img src="media/screens/page-muyu.gif" width="220" alt="木鱼源码渲染预览"></a><br><small>点击计数与缩放反馈</small></td>
<td align="center"><b>骰子</b><br><a href="media/screens/page-dice.gif"><img src="media/screens/page-dice.gif" width="220" alt="骰子源码渲染预览"></a><br><small>摇动/点击掷骰</small></td>
</tr>
<tr>
<td align="center"><b>2048</b><br><a href="media/screens/page-2048.gif"><img src="media/screens/page-2048.gif" width="220" alt="2048源码渲染预览"></a><br><small>倾斜或网页方向操作</small></td>
<td align="center"><b>摄像头</b><br><a href="media/screens/page-camera.gif"><img src="media/screens/page-camera.gif" width="220" alt="摄像头源码渲染预览"></a><br><small>本地预览；此处输入为预录示例</small></td>
<td align="center"><b>时钟</b><br><a href="media/screens/page-clock.gif"><img src="media/screens/page-clock.gif" width="220" alt="时钟源码渲染预览"></a><br><small>联网校时；此处为示例时钟</small></td>
</tr>
<tr>
<td align="center"><b>海上航行</b><br><a href="media/screens/page-voyage.gif"><img src="media/screens/page-voyage.gif" width="220" alt="海上航行源码渲染预览"></a><br><small>三航道躲礁收集金币</small></td>
<td align="center"><b>设备设置</b><br><a href="media/screens/page-settings.gif"><img src="media/screens/page-settings.gif" width="220" alt="设备设置源码渲染预览"></a><br><small>网址、音量、样式和字幕</small></td>
<td align="center"><b>底座连接弹窗</b><br><a href="media/screens/page-dock.gif"><img src="media/screens/page-dock.gif" width="220" alt="底座连接弹窗源码渲染预览"></a><br><small>连接反馈与双眼保持</small></td>
</tr>
<tr>
<td align="center"><b>页面切换</b><br><a href="media/screens/page-transitions.gif"><img src="media/screens/page-transitions.gif" width="220" alt="页面切换源码渲染预览"></a><br><small>主页、列表与应用间滑动</small></td>
</tr>
</table>

## 手机页面

手机部分直接加载设备内置网页，在 Edge 中切换真实控件并截图；设备状态使用本地示例数据，没有连接机器人。横屏截图展示相机与摇杆并排的布局。

<table>
<tr>
<td align="center"><b>遥控</b><br><a href="media/phone/phone-remote.gif"><img src="media/phone/phone-remote.gif" width="270" alt="遥控源码渲染预览"></a></td>
<td align="center"><b>应用与玩法</b><br><a href="media/phone/phone-apps.gif"><img src="media/phone/phone-apps.gif" width="270" alt="应用与玩法源码渲染预览"></a></td>
</tr>
<tr>
<td align="center"><b>造型与底盘</b><br><a href="media/phone/phone-look.gif"><img src="media/phone/phone-look.gif" width="270" alt="造型与底盘源码渲染预览"></a></td>
<td align="center"><b>主题与设置</b><br><a href="media/phone/phone-settings.gif"><img src="media/phone/phone-settings.gif" width="270" alt="主题与设置源码渲染预览"></a></td>
</tr>
</table>

![横屏遥控页面](media/phone/phone-landscape.png)

## 生成依据

- [渲染来源及源文件 SHA256](media/render-provenance.json)
- [机身 GIF 场景索引](media/screens/index.json)
- [手机页面来源说明](media/phone/provenance.json)
- [重放工具说明](../tools/visual-preview/README.md)

本次只补充演示媒体和说明，没有改变固件二进制、触摸参数或设备设置。
