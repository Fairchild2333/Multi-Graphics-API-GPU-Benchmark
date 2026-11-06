# 黑鲨 5 Pro 对应固件查找结果

核对日期：2026-10-11。目标为目前连接的 SHARK KTUS-A0（katyusha）。以下材料用于离线分析，尚未刷入手机。

手机当前版本：

- 系统：V11.0.4.0.JOYUI
- Build ID：KTUS2210310CN00MP6
- Android：12 / API 31
- 安全补丁：2022-10-01
- 内核：5.10.101-android12-9-gece0cc04d1a5

## 找到的 MP6 OTA

包名：`20221114150403-katyusha-deltaota_2022-10-31_2022-08-11_475bbf5ab3.zip`

包大小：1,274,288,168 字节，约 1.27 GB / 1.19 GiB。

可用下载地址（已验证 HTTP 206，并读取 ZIP 元数据和 payload 清单）：

- [腾讯云文件地址](https://ota-1256119282.file.myqcloud.com/KTUS-A0/20221114150403-katyusha-deltaota_2022-10-31_2022-08-11_475bbf5ab3.zip)
- [腾讯云广州对象地址](https://ota-1256119282.cos.ap-guangzhou.myqcloud.com/KTUS-A0/20221114150403-katyusha-deltaota_2022-10-31_2022-08-11_475bbf5ab3.zip)

链接发现来源：[ROMProvider 的 KTUS-A0 固件索引](https://romprovider.com/black-shark-ktus-a0-flash-file-stock-rom/)。其原始链接使用 `cdn-ota.blackshark.com`，当前解析至 127.0.0.1，无法下载。备用腾讯云域名来自手机原厂 `com.blackshark.ota` 更新器 APK 中的公开 URL；保持已发表的文件路径不变后成功读取。

OTA 内部 metadata 确认：

```text
post-build=blackshark/katyusha/katyusha:12/KTUS2210310CN00MP6/buildfarm10311509:user/release-keys
post-build-incremental=V11.0.0.0.SKTUSCN
post-security-patch-level=2022-10-01
pre-build=blackshark/katyusha/katyusha:12/KTUS2208110CN00MP5/buildfarm08110024:user/release-keys
```

目标 Build ID 与安全补丁日期均对应当前手机。OTA 写入的 incremental/fingerprint 字段与手机顶层属性并非逐字相同。后续已经还原 boot 镜像并验证目标哈希，内核的完整版本、编译时间和编译器信息均与手机 `/proc/version` 一致。

## 当前限制：这是 MP5 → MP6 增量包

它不是独立的全量固件。boot 更新包含：1 个 BROTLI_BSDIFF、441 个 SOURCE_COPY、90 个 ZERO 操作。必须提供正确的 MP5 原始 `boot.img`，才能准确还原 MP6 的完整 `boot.img`。直接忽略 SOURCE_COPY 或用其他版本底包会得到错误镜像。

清单要求的 MP5 源 boot：

```text
size=201326592
sha256=3d334299f223460222a66b9dd5f641d3f6c4099c5ed6660c88063468af7fe203
```

应还原得到的 MP6 目标 boot：

```text
size=201326592
sha256=2db977b918e1b3af0ecb37d9c77f89816db9f7e2e65495cbdfd94b88cfcc550b
```

已下载 boot 差分数据：`MP5-to-MP6-boot.brotli-bsdiff`，3,891,125 字节，SHA-256 与 payload 清单一致：

```text
d98e65bc44bd7be67bde42aad37af7a7306765a9fde7f8a52f4da53fa655773d
```

同时保存了 OTA metadata、payload_properties、manifest 和解析清单。payload metadata 哈希核对通过；没有下载整个 ZIP，也没有声称完成整包哈希或 OTA 签名验证。

公开目录查询返回 AccessDenied，没有继续查询受限目录。

## 后续进展：两个 MP6 镜像已还原并传至手机

找到公开固件备份仓库：[dumps/blackshark/katyusha](https://dumps.tadiphone.dev/dumps/blackshark/katyusha)。MP5 分支提交为 `0b3d76ba324b315f5e552221cd07def832c54bb3`。

[该提交的 MP5 原始 boot.img](https://dumps.tadiphone.dev/api/v4/projects/4705/repository/files/boot.img/raw?ref=0b3d76ba324b315f5e552221cd07def832c54bb3) 已下载至 `MP5-images/boot.img`；文件大小和本地计算的 SHA-256 与 MP6 增量包要求的源镜像完全一致。

使用这个经过验证的底包还原 `MP6-images/boot.img`，目标 SHA-256 为上文的 `2db977...550b`，完全匹配 MP6 清单。内核为：

```text
5.10.101-android12-9-gece0cc04d1a5
#1 SMP PREEMPT Thu Sep 8 14:22:13 CST 2022
```

`xbl_config` 有独立的还原路径：MP1 包 `xbl_config.elf` 的前 131,072 字节哈希恰好等于增量清单要求的 MP5 源分区哈希，再加上 MP6 的 REPLACE_XZ 数据即可还原完整目标。这不是把旧文件改名替代当前版本。

还原的 `MP6-images/xbl_config.img` 大小 135,168 字节，SHA-256：

```text
743553af722a9ebad0856cf352653e36db15d2b0141706c9d2297a9f36354ec3
```

以下文件已经复制至连接的黑鲨 5 Pro 的内部存储根目录，手机端 `sha256sum` 再次校验通过：

- `/sdcard/boot.img`，201,326,592 字节。
- `/sdcard/xbl_config.img`，135,168 字节。

这里只复制到共享存储，没有写入启动分区。

## 仍缺 UEFI

MP5 公开备份仓库没有保存 `uefi.img`。MP1 的 UEFI 底包不满足 MP6 清单要求；离线应用补丁的实验结果也未通过目标哈希，因此没有保存或传输这个错误候选镜像。

`MP5-to-MP6-uefi.brotli-bsdiff` 已下载并通过差分数据哈希校验，仍需要正确源镜像：

```text
MP5 uefi source size=2756608
MP5 uefi source sha256=21dbc5dd98c43f5cfd00f15b035f6b6593c0b37862e9cb768dd07e5e98446d0f
MP6 uefi target size=2760704
MP6 uefi target sha256=e1b2e415567fad4e4e7e70dd04b220519bc1b856329fb121dd09d1e80af0fbeb
```

手机当前 `uefi_b` 为 `/dev/block/sde30`，仅 root 可读；普通 ADB 的只读尝试返回 Permission denied。

用户已有的 `flash_signed_by_xiaomi_0080de01618842364d98743045ba419c` 是 MP1；其 boot 内核为 `5.10.43-android12-9-gbe29a6a3e449`，不对应当前内核，也不满足增量包要求的 MP5 源 boot 哈希。

下一步只缺上述 MP5 源 UEFI，或者直接获得 MP6 的原始 UEFI。尚未找到公开可核验的来源。固件材料齐全后仍需核对 GhostLock 对该内核的适配情况；找到 ROM 本身不等于临时 root 已验证成功。

## 继续搜索的核验结果（2026-10-11）

已找到并读取国际版 MP1 → MP2 的官方 OTA：

`https://ota-oversea-1256119282.file.myqcloud.com/KTUS-H0/20220815183807-katyusha-deltaota_2022-08-10_2022-06-17_ae7449d089.zip`

这个对象可通过公开 HTTP Range 读取，ZIP 总大小 496,341,030 字节。metadata 和 payload manifest 已保存到 `Global-MP2-OTA/`；并未下载整个 ZIP。该包更新前为 `KTUS2206170OS00MP1`，更新后为 `KTUS2208100OS00MP2`。

其 UEFI 仍依赖旧镜像，包含 SOURCE_COPY、BROTLI_BSDIFF、ZERO 操作，没有独立的完整 REPLACE 镜像。旧 UEFI 大小 2,756,608 字节，SHA-256 为 `58b8fd8219fad9793ce821d044e90222691680724b022619ff2ae46ba86bf236`；新 UEFI 大小 2,760,704 字节，SHA-256 为 `f737a420553faa423e05c656ee0d74b3b7ef14da9be46e3396a4978149203857`。均不等于国行 MP5/MP6 所需值，因此没有把国际版当作当前系统镜像传给手机。

公开候选来源的当前状态：

- [东海论坛 MP2 线刷包帖子](https://www.donghairom.com/thread-18702-1-1.html)：标注 KTUS2204240CN00MP2，下载地址保存在收费 10 金币的附件里，未获得包内容。
- [VN-GSM 黑鲨 5 Pro 分类](https://vngsmservices.com/index.php?a=downloads&b=folder&id=155488)：列出 MP3 修复包和 MP4 包；MP4 的公开 Download 按钮跳转到登录页，没有可匿名读取的文件链接。UEFI 是否符合所需哈希尚未核验。
- [FiimeROM MP3/MP4 合集](https://mi.fiime.cn/enginerom/62.html)：下载标注 20000 积分，未获得包内容。页面标题中的工程包或 ROOT 标签不代表其中 UEFI 一定修改或一定可用，必须在拿到候选后核验。
- [BlackShark-5Pro-TWRP 的备份说明](https://github.com/S7ruc7ureV01d/BlackShark-5Pro-TWRP/blob/main/docs/BACKUPS.md)：作者备份了 UEFI，但明确说明原始镜像被 git-ignore，仓库只提交校验值。检查递归文件树，没有 UEFI 二进制可下载。
- QLMFlasherPro 列有国行和国际版文件；国行具体构建版本未标注，文件详情读取被拒绝或超时，未获取镜像，也没有声称该包匹配。
- 查询 GitLab 与 GitHub 的公开 katyusha 固件项目，没有找到另一份包含 MP5/MP6 UEFI 的可下载备份；sm8450-mainline 项目提供设备树或其他机型的运行时固件，不提供本机原厂 UEFI 镜像。
- 已知本地 Downloads 和 BaiduNetdiskDownload 里没有新增的黑鲨 5 Pro UEFI 候选；用户确认没有上述站点的现成下载权限。

当前 ADB 设备列表为空；搜索和离线核验可继续，但目前无法复制新文件到手机。

### 哈希和“payload 中有 UEFI”的含义

MP6 的增量 OTA 已找到，缺的是可以还原完整 UEFI 的正确旧镜像。目标哈希来自 `payload.bin` 内 manifest 的 `PartitionUpdate.new_partition_info`，旧镜像要求来自 `old_partition_info`。读取这个清单不需要事先拿到完整 UEFI。

MP6 的 UEFI 清单具体有 7 个操作：5 个 SOURCE_COPY、1 个 BROTLI_BSDIFF、1 个 ZERO。因此 payload 中确实有 UEFI 更新条目及差分数据，但不能脱离匹配的 MP5 原始 UEFI 单独恢复完整目标镜像。

原始解析证据已另存 `UEFI-manifest-evidence.json`，包含官方对象 URL、manifest SHA-256、完整 UEFI old/new info 和操作列表。

## 链接可访问性复核（2026-10-11）

用户本机浏览器访问 VN-GSM 报 `DNS_PROBE_FINISHED_NXDOMAIN`。随后本机 Python DNS 解析同样失败（getaddrinfo 11001），直接请求文件页也因解析失败而无法连接。Google 公共 DNS 的查询返回 Status 2 / SERVFAIL，说明 `Name servers did not respond [167.172.88.57]`。因此当前不能将这个页面线索描述为本机可用下载入口，也不能仅归因为用户网络设置。先前网页工具返回过带有抓取时间的页面文字，但本轮网页工具重读文件页超时；先前抓取文字不证明网站当前在线或文件可下载。没有修改本机 DNS 设置。

同轮复核中，MP5 boot 的公开 GitLab 元数据仍返回 HTTP 200，源 SHA-256 和 201,326,592 字节大小仍符合 MP6 清单要求；MP6 国行官方 OTA 的腾讯 COS 对象仍返回 HTTP 206，Range 响应总大小 1,274,288,168 字节，文件头为 ZIP 的 `PK`。两者属于实际可读取并已有本地材料的来源。

准确状态：已找到 MP5 的公开分区备份及 MP6 的官方增量 OTA；仍未找到可公开下载并核验的 MP5/MP6 全量固件或匹配的完整 UEFI。不能据此断言这些文件在互联网上完全不存在。

## 扩大到 MP4 后的新增结果（2026-10-11）

### LiveOnServer：找到另一处本机实际可打开的 MP4 文件页

[MP4 文件详情](https://liveonserver.com/index.php?a=downloads&b=file&id=11816)

页面名称为 `Black.shark5Pro_V13.22.07.10_KTUS2207100CN00MP4_MIUI_CN`，标注 5.40 GB，发布时间 2026-04-18。本机 Python HTTPS 请求实际返回 HTTP 200；分类页 `folder&id=6582` 也实际返回 HTTP 200，列有该 MP4 文件和更旧的 2022-03 版本。

点击其公开 Download 对应地址 `https://liveonserver.com/index.php?a=downloads&b=file&c=download&id=11816` 后，实际跳转至 Login 页面。尚未获得匿名下载地址；未核实登录后的免费/套餐/付款要求，也未获得包内容。原始网页和跳转后的页面保存在 `web-probes/liveon-mp4.txt`、`liveon-folder.txt`、`liveon-download.txt`。该站目前可访问，不等同于 VN-GSM 的 DNS 故障，也不等同于已下载 MP4 ROM。

重新直接读取 FiimeROM 工程包 62 的页面，仍标注 MP3/MP4 合集和高速下载 20000 积分；没有取得原始固件。

### 原厂 OTA 应用：静态解析并测试正式更新查询接口

下载了官方开源 JADX v1.5.6，用于离线读取此前从本机提取的 `OTA-system-app.apk`；未运行 APK。主要源码保存到 `OTA-app-decompiled/`，资源保存到 `OTA-app-resources/`。源码反编译报告 16 个错误，但本次使用的服务器配置资源、请求构造和版本读取方法均可读取；资源解析成功。

正式国行接口来自 APK 的 `OTA_SERVER_EXTERNAL`：

`https://ota-api.blackshark.com/upgrade_check/`

该域名本机解析到公网地址，GET 实际返回 HTTP 200 和 `Code=603, SIGN_ERROR`，说明接口仍在线。随后依据原厂应用的正常 POST 请求格式进行有限的固件版本查询，使用 APK 自身的公开客户端常量；未使用真实 IMEI、未注册设备、未请求测试/内部服务器、未执行安装或更新。

解析源码确认 `Special` 为整数而非布尔值。纠正该字段类型后，查询下列实际已知版本均得到 HTTP 200、`Code=100`、`Message=版本无须更新`、`Data=null`：

- KTUS2204040CN00MP1
- KTUS2205250CN00MP3
- KTUS2207100CN00MP4
- KTUS2208110CN00MP5
- KTUS2210310CN00MP6

查询脚本为 `query_public_ota.py`，响应分别保存为 `ota-query-KTUS*.json`。原厂应用通过 `ro.build.bs_id`，缺失时通过 `ro.build.id` 获取版本；公开 MP5 dump 的 `system/system/build.prop` 确认两者均为 KTUS2208110CN00MP5，产品名 KTUS-A0。没有真实设备标识的查询未返回下载地址，不能据此证明旧包已被删除，或证明连接真实手机的官方检查一定也是相同结果。

国际正式 OTA 查询接口两个已知域名本机能解析，但 HTTPS 请求超时。没有使用内部或测试接口。

### 其他新增检查

- [联珀论坛的黑鲨 5 Pro 资料页](https://bbs.lianpo.cn/thread-5055-1-137.html)：本机实际返回 HTTP 200，但正文资料内容被 VIP 权限隐藏，没有看到下载链接或具体 MP 版本。没有注册账户、购买会员或尝试绕过权限。
- Internet Archive 公开软件索引没有找到黑鲨 5 Pro ROM；名为 Katyusha Patch 的条目发布于 2021 年，早于本机上市，排除。Wayback 对已知国行 OTA 路径的公开归档索引返回空列表。
- BlackShark-5Pro-TWRP 的公开 Releases 仅有 TWRP recovery 镜像，Issues 为空，没有新增原厂 UEFI 二进制来源。
- MP5 GitLab dump 根目录再次核对，仍没有 UEFI 分区镜像；没有重复下载 boot。

本轮没有获得新的 MP4/MP5/MP6 UEFI 二进制，因此尚不能核验 MP4 UEFI 是否可以作为还原 MP6 的源候选。真正需要的源 UEFI 仅 2,756,608 字节；不必先取得整套数 GB 的 ROM，只要可公开取得这个原始分区镜像即可继续离线核验。


## 新增海豚 ROM 候选（2026-10-11）

使用当前域名 `www.haitunrom.cn`，首页、黑鲨 5 Pro 分类和以下三个正文页均由本机 Python HTTPS 请求实际取得 HTTP 200。旧 `haitunrom.com` 地址不能代替当前地址。

- [2022-07-10 候选，对应 MP4 日期](https://www.haitunrom.cn/romxiazai/heisha/heisha5pro/12563.html)：标题 JOYUI V13.22.07.10。
- [2022-08-11 候选，对应 MP5 日期](https://www.haitunrom.cn/romxiazai/heisha/heisha5pro/12564.html)：标题 JOYUI V13.2208110。
- [2022-10-31 候选，对应 MP6 日期](https://www.haitunrom.cn/romxiazai/heisha/heisha5pro/12566.html)：标题 JOYUI V13.2210310。

三页均标注「需解 BL」「完美解锁包」「官方系统修改而成」，下载区显示「需要金币：19.9」「本资源需要登录后下载」。这里的价格单位为金币，未验证其与人民币的兑换关系。页面发布日期为 2024-04-05。

这些是日期对应的修改固件候选；标题没有完整 KTUS 构建号，尚未取得固件文件、未确认包含 UEFI、未核验 UEFI 是否原始及是否匹配 MP5/MP6 所需 SHA-256。因此不应称为已找到并验证的官方 MP6 全量包。没有购买、注册、绕过付费下载或执行包内刷机脚本。

原始正文保存在 `web-probes/haitun-mp4.html`、`haitun-mp5.html`、`haitun-mp6.html`，精简证据为 `web-probes/haitun-candidates.json`。公开正文包含的「屏幕压感修复.7z」不是所需 ROM，未下载或作为 UEFI 候选。

用户反馈先前网站注册验证邮件未收到；邮件未发/延迟/过滤的具体原因尚未查明，不能据此声称账户已可下载，也没有代发联系站方的邮件。继续用完整构建号、固件标题和 UEFI 名称搜索，没有获得新的匿名固件直链或匹配的 UEFI 二进制。


## 用户提供 MP6 包：三份镜像核验通过（2026-10-11）

源目录：`C:\Users\lzz28\Downloads\New folder\Black.shark5Pro_V13.2210310_KTUS22310CN00MP6_cn`。目录实际包含 `images/boot.img`、`images/xbl_config.elf`、`images/uefi.elf` 和单独的 Root 子目录。未执行任何刷机脚本或 Root 子目录文件。

- boot.img：201,326,592 字节，完整 SHA-256 等于 MP6 官方增量 OTA 的目标哈希 `2db977b918e1b3af0ecb37d9c77f89816db9f7e2e65495cbdfd94b88cfcc550b`；Linux banner 为 `5.10.101-android12-9-gece0cc04d1a5`，对应之前读取的手机内核。
- xbl_config.elf：文件总长 524,288 字节；前 135,168 字节 SHA-256 等于 MP6 目标哈希 `743553af722a9ebad0856cf352653e36db15d2b0141706c9d2297a9f36354ec3`。
- uefi.elf：文件总长 5,242,880 字节；前 2,760,704 字节 SHA-256 等于 MP6 目标哈希 `e1b2e415567fad4e4e7e70dd04b220519bc1b856329fb121dd09d1e80af0fbeb`。此前缺失的正确 UEFI 数据现在已获得。

按官方 payload 的目标数据长度整理到 `MP6-images/boot.img`、`MP6-images/xbl_config.img`、`MP6-images/uefi.img`；已有 boot/xbl_config 与新包相同，没有重新覆盖。原始包文件未修改。核验报告为 `MP6-local-ROM-verification.json`。本次仅证明这三个镜像数据匹配，不等于整个修改 ROM 都是官方原版，也不等于 GhostLock 已成功或一定兼容。

当前 `adb devices -l` 为空，没有复制新 UEFI 到手机、没有刷写分区、没有解锁或触发提权。


## 补传到手机并回读核验（2026-10-11）

Windows 识别到序列号 9893218B 的 SHARK KTUS-A0，内部存储 MTP 可访问，Android Composite ADB Interface 的 WinUSB 驱动状态 OK。当前 ADB 35.0.2 的设备列表为空。重启 ADB 服务未恢复；临时尝试 libusb 后端服务启动失败，已恢复默认后端，没有持久修改环境变量或驱动。

通过 Windows MTP 文件传输，将已核验的 `MP6-images/uefi.img` 复制到手机内部存储根目录。该目录已有 `boot.img` 和 `xbl_config.img`，未重传或覆盖它们。随后把手机这三份文件复制回电脑至 `MP6-phone-readback-20261011-160453/` 并重新计算 SHA-256，三份均匹配 MP6 官方 manifest 的目标哈希。回读证据为该目录的 `verification.json`。

当前手机内部存储根目录三份镜像已齐且回读通过，尚未通过 ADB 建立调试连接；没有执行 GhostLock、刷写分区、解锁 bootloader 或安装系统。


## 当前 GhostLock 界面和配置检查（2026-10-11）

ADB 已恢复，9893218b 状态为 device。截屏显示 GhostLock 主页「配置需要修正」「请修正标记的配置项」，执行按钮灰色。当前已加载 `5.10.101-android12-9-gece0cc04d1a5.conf`。只打开了高级选项、参数配置和编辑查看页，没有保存修改或执行提权。

通过应用导出当前配置至内部存储根目录 `ghostlock-config-audit-20261011-1610.conf` 并读取到本目录同名文件。该文件的 `task_struct` 全部字段为 null，`cred` 全部字段为 null，`execution.routes` 为空；部分符号地址和 kernel_phys_load 有数值。表明当前导出的配置还缺结构布局参数，不能仅凭内核名对应和镜像哈希匹配称为完整可执行配置。本次没有验证提权成功。查看后已返回 GhostLock 主页。
