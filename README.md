# 东方妖妖梦 Android 移植版

《东方妖妖梦 ~ Perfect Cherry Blossom》的 Android 非官方移植（体验版）。
以社区反编译源码为基础，由 **DeepSeek 辅助完成**移植：渲染迁移到 SDL3 + OpenGL ES，适配手机触控。

## 下载

APK 与游戏数据文件见 [Releases](https://github.com/qingqian-qing/th07-android/releases)：

- `th07-android-*.apk` — 游戏本体
- `th07.dat` — 主数据包
- `thbgm.dat` — BGM
- `msgothic.ttc` — 字体（开源替代）

## 安装

1. 安装 APK（Android 8.0+，arm64）
2. 将 `th07.dat`、`thbgm.dat`、`msgothic.ttc` 放入：
   `Android/data/com.zun.th07/files/`
3. 打开游戏

## 功能

- 触控操作（手势 / 虚拟按键）
- 练习器：无敌、锁残机、无限符卡、自动符卡、锁火力
- 内置中文字体
- 崩溃日志（crash_report.txt）

## 操作

- 拖动：移动自机
- 双指点击：符卡
- 四指：暂停
- 练习器面板：游戏画面内左上角

## 致谢

- [some100/th07](https://github.com/some100/th07) — 反编译源码基础
- [CNTianQi233/th06-sdl2](https://github.com/CNTianQi233/th06-sdl2) — Android 移植参考

## 版权

《东方妖妖梦》版权归 ZUN / 上海爱丽丝幻乐团所有。本为非官方爱好者工程，仅供学习交流，禁止商用。请支持正版（Steam 有售）。
