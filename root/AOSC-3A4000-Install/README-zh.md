# 龙芯 3A4000：安同离线桌面安装

这个安装包适用于龙芯 3A4000 的 MIPS `loongson3`。它参考原 Ubuntu 配置生成账号和网络设置，再调用安同自带的安装器；原 `autoinstall.yaml` 不需要修改，也不能直接交给安同执行。

按你的选择，**整盘重装最后确认的系统盘，保留其他磁盘，默认启用桌面**。脚本先检查环境、列出磁盘；只有你明确选盘并输入清盘确认后，才开始自动安装。

## 1. 准备启动盘和安装包

本方案核对的官方镜像是 `aosc-os_installer_20261017_loongson3.iso`，SHA-256 为：

```text
52c6fa45923f187b4827aabbed8754c961c540c02354b2bdad65da58d36b6898
```

该镜像已包含 Desktop 离线系统，安装后体积约 15.1 GB。脚本要求系统盘至少 32 GiB。基础系统从启动介质安装，全程无需下载系统镜像。

使用官方写盘工具制作启动盘。这一步尚未替你操作。AOSCMediaWriter 写盘后是否还存在可供 Windows 写入的分区尚未验证，建议用**第二只 U 盘**保存完整安装文件夹；如果已有 Ventoy，可以把完整文件夹放在其数据分区的 ISO 旁边。不要只复制 `install.py`。

安装文件夹应同时包含：

```text
install.py
postinstall.py
finish-online.py
settings.json
README-zh.md
```

`settings.json` 含原账号的密码 hash 和 Wi-Fi 密码，**从不进版本库**（真实文件在 NAS）；仓库里的 `settings.example.json` 是同结构模板，装机前复制为 `settings.json` 并填入真实值。生成时放在 Downloads 下限制权限的私密目录；复制到普通 U 盘后，原 Windows 权限可能不再有效。请保管好介质，不要分享这个文件或整个安装包。安装成功后可以删除 U 盘里的 `settings.json`；已安装系统仍会保留正常的账号和无线连接配置。

## 2. 进入 LiveKit，先检查

在龙芯上从官方启动盘进入 LiveKit，打开安装文件夹，在这个文件夹里打开终端，执行：

```sh
sudo python3 install.py
```

这条命令**只检查和列磁盘，不清盘**。它会核对 MIPS 架构、本地 Desktop 载荷、固件接口和安装器版本：`dkcli 0.4.9`、`deploykit-backend 0.10.1`。龙芯 LEFI 必须被安装器识别；版本或固件识别不符合要求时，会在清盘前停止。

查看输出的磁盘路径、型号、序列号和容量，找到要重装的系统盘。脚本不会自动选择最大磁盘。USB／可移动介质、当前 LiveKit 启动介质、已挂载或使用中的盘，以及 LVM、RAID、加密、Btrfs 等复杂存储布局会被拒绝。

如果被拒绝，请把这条预检命令的输出发来核对，不要改脚本或绕过保护，也不要发送 `settings.json`。

## 3. 选定系统盘，再开始安装

把下面的 `/dev/实际磁盘` 替换为预检中已经核对的**整块系统盘**路径，不是其中的分区：

```sh
sudo python3 install.py --disk /dev/实际磁盘 --install
```

脚本会再次显示唯一将被清空的磁盘。最后必须输入终端要求的完整确认文字，例如目标确实为 `/dev/sda` 时输入：

```text
ERASE /dev/sda
```

请以现场显示的磁盘路径为准，不要直接照抄示例。输入其他内容会取消。确认后自动完成分区、离线系统复制、引导和账号配置，**不会自动重启**。

有无线设备和驱动时，会限时尝试连接已保存的 `Deco_BD80`；没有 Wi-Fi 驱动、没有网络或连接失败，就跳过联网尝试，继续离线安装。无线设置也会尽可能预置到新系统，有线 DHCP 优先、无线作为备用。硬件驱动不支持时，可以安装完成后从桌面处理。

只有看到下面这行，才表示脚本完成了离线安装和必要检查：

```text
OFFLINE DESKTOP INSTALLATION COMPLETE. No automatic reboot was requested.
```

随后可以执行 `sudo reboot`，并选择系统盘启动。若显示 `STOPPED`，先保留现场和错误信息；磁盘、引导或核心系统失败不会被当作可选联网步骤忽略。

## 4. 首次启动和后续补装

新系统使用镜像自带桌面，默认进入图形登录界面。账号为 `fairchild`，密码沿用原配置对应的密码，**仍需输入密码登录桌面**。SSH 使用原公钥，关闭 SSH 密码登录；管理员免密码 sudo。时区为悉尼、键盘为 `us`，保留原区域设置，并禁用睡眠和休眠。

首次开机只在后台尝试一次补装开发／诊断工具，桌面启动不等待下载。更新软件索引和下载有时间上限；没网或某个包不可用就记为 `pending`。下载完整且核对通过后才安装该批软件，不会为了超时强制杀死正在配置软件包的进程。

以后有网络时可以手动重试：

```sh
sudo /usr/local/sbin/aosc-finish-online
```

补装状态保存在 `/var/lib/aosc-autoinstall/online-status.json`，服务日志可查看：

```sh
sudo journalctl -u aosc-finish-online.service --no-pager
```

这个步骤只补选定工具，不进行整机升级，也不部署 Tailscale 或 Grafana。

这些脚本已通过本地静态检查和模拟测试；**尚未在你的 3A4000 上完成实际安装验收**。第一次运行的预检仍需以机器现场为准。
