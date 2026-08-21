# 东方妖妖梦 Android 移植版

> 将《东方妖妖梦 ~ Perfect Cherry Blossom》（th07）移植到 Android 的非官方项目。

## 项目简介

本项目将 ZUN 原作的《东方妖妖梦》（Perfect Cherry Blossom 1.00b）从 Windows / Direct3D 8 移植到 **Android**，技术栈替换为 **SDL3 + OpenGL ES**，并针对手机触控重新设计了操作方式。

**移植方式**：本项目以社区反编译源码为基础，**移植过程绝大部分（约 95%）由 DeepSeek 辅助完成**——包括渲染后端迁移、触控适配、练习器、字体与稳定性修复等。

## 功能特点

- **完整游戏内容**：与 PC 原版一致的全部剧情、弹幕、符卡与音乐（需自备游戏数据）
- **触控操作**：点按 / 滑动移动自机，双指点击释放符卡，支持手势与虚拟按键两种方案
- **练习器（作弊）**：无敌、锁残机、无限符卡、自动符卡、锁火力等开关，随开随关
- **中文字体**：内置简体中文字体，界面文字正常显示
- **崩溃日志**：崩溃时自动生成 crash_report.txt，便于定位问题
- **宽屏适配**：横屏游玩，游戏画面等比缩放

## 下载

最新 APK 请前往 **Releases** 页面直接下载：

👉 [Releases 下载](https://github.com/qingqian-qing/th07-android/releases)

## 安装与运行

### 1. 安装 APK

下载 `th07-android-*.apk` 后直接安装（Android 8.0+，arm64 设备）。

### 2. 放置游戏数据

游戏本体数据不随 APK 分发（版权原因），请从正版游戏提取以下文件，放入：

```
Android/data/com.zun.th07/files/
```

需要的数据文件：

| 文件 | 说明 |
|---|---|
| `th07.dat` | 主数据包（剧情、贴图、音乐索引等） |
| `thbgm.dat` | BGM 音乐包（WAV 格式，有它就自动有音乐） |

### 3. 启动

打开应用即可游玩。首次运行会自动创建存档与配置。

## 操作说明

| 操作 | 效果 |
|---|---|
| 手指拖动 | 移动自机 |
| 双击 / 双指点击 | 释放符卡（Bomb） |
| 屏幕右下 | 符卡按钮 |
| 四指点击 | 暂停 |
| 练习器面板 | 无敌 / 锁残 / 无限符卡等开关 |

## 构建

Android 构建需要 Android SDK + NDK（arm64-v8a）。

```bash
cd android && ./gradlew assembleDebug
```

或直接使用仓库自带的 GitHub Actions（推送 main 自动构建 APK）。

## 致谢

本项目建立在以下社区成果之上：

- **[some100/th07](https://github.com/some100/th07)** —— 妖妖梦 PC 版反编译源码，本项目的代码基础
- **[CNTianQi233/th06-sdl2](https://github.com/CNTianQi233/th06-sdl2)** —— 红魔乡 SDL2 Android 移植，本项目触控与渲染的主要参考

## 版权声明

- 《东方妖妖梦》及其全部素材版权归 **ZUN / 上海爱丽丝幻乐团** 所有
- 本项目为非官方爱好者工程，与原作者无任何隶属或授权关系，**仅供学习交流，禁止用于商业用途**
- 请支持正版：游戏可在 Steam 等平台购买

---

Made with ❤️ and DeepSeek.
