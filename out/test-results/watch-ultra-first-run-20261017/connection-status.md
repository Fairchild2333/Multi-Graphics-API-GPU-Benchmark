# Apple Watch Ultra 初次连接状态 · 2026-10-17

## 当前状态

开发者模式已开启，DDI可用，实际Watch已加入5设备签名profile。
Release App安装成功，手表亮屏时远程启动成功；实体双核arm64_32已执行混合内核。
**完整测试现已完成**：用户调整系统亮屏／返回时钟设置并佩戴解锁后，
test-without-building实际点击一键，2/2任务、3条成绩、六条曲线126点，UI1/1通过，
自然退出0，约88秒含启动和截图。单线程53.672、双核116.499 MWork/s；
页面／存储已测，内存5.614GB/s仍为未确认平台的参考值，整体review。
此前三次实际按钮运行分别约7/10/15秒后取消，后两次生命周期日志均明确记录
inactive随后background。自动轻触进度文字也未阻止后台转换。
已撤回无效保活循环；App仅在background取消，记录状态与取消原因。
标准UI测试已去掉强制中文；此前中文来自测试参数，App正常启动跟随系统语言。
测试程序先完整预编译（TEST BUILD SUCCEEDED），避免解锁后等待编译。
watch-final完整导出及verify_archive.py校验通过；五台其他设备主历史与Mac live未变。
ARM64_32元数据标签识别已修正并安装，旧三条原始JSON仍逐字节保留。
详见[完整报告](report.md)。本轮成功不保证离腕、主动退出或任何系统状态下仍继续运行。

## 连接准备过程

用户确认Ultra配对当前有线iPhone13Pro，watchOS26.5，手表未显示开发者模式。
最初Xcode27.1/DeviceHub/命令行只发现iPhone；19:59再次检查已发现实体Watch。
实体为Apple Watch Ultra / Watch6,18 / arm64_32，watchOS26.5(23T570)，
通过localNetwork连接，配对状态paired、已解锁。当时Developer Mode仍为Disabled，
devicectl因此仅返回部分设备信息；尚不能运行开发测试。见watch-device-details.json。
已打开真实Watch工程、核对部署最低watchOS10、启动DeviceHub附近配对(iOS/watchOS)，
并打开iPhone的Watch App以检查连接。当时尚未安装实体Watch或启动任何手表测试。
签名Release构建和codesign完整性验证通过；最初profile仅包含此前注册设备，
已尝试以实际Watch为destination注册/更新签名，但destination准备超时、退出70，
未进入编译；见watch-registered-build.log。不能把generic签名构建当作已授权该Watch安装。
已停止先前为排查连接启用的iPhone屏幕共享；用户报告手机左上角白框，画面确认
为时钟附近白色边框，但来源未确认，等待用户反馈停止共享后是否消失。
用户说明此前已在Watch确认信任；不要求重置信任或抹除/重新配对Watch。
原始构建/签名/设备发现记录均保留；不会重置或抹掉手表。

官方说明：首次设备配对开始前开发者模式可能不显示；先连接配对iPhone并确认双方信任。
https://developer.apple.com/documentation/xcode/enabling-developer-mode-on-a-device
https://developer.apple.com/documentation/xcode/managing-your-simulated-and-physical-devices-in-device-hub
