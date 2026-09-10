# SiliFlow

VS2026 + wxWidgets 3.2.9 工程脚手架。

## 框架组成

```
SiliFlow-2026/
├── SiliFlow.sln                 VS2026 (Format Version 18) 解决方案
├── README.md                    本文件
├── .gitignore
├── .gitattributes
├── 3rd/
│   └── wxWidgets-3.2.9/         本仓库不包含，clone 后需自行恢复（见下）
└── main/
    ├── main.vcxproj             4 配置：Debug/Release × Win32/x64，Toolset v145
    ├── main.vcxproj.filters
    ├── main.vcxproj.user
    ├── GlobalConfig.props       include/lib 路径、OutDir/IntDir
    ├── cMain.cpp                wxIMPLEMENT_APP 入口
    ├── MainFrame.h / MainFrame.cpp    示例主窗口（菜单 + 按钮 + About）
    └── res/                     资源占位
```

## 与 SigFlow_FPGA 对齐的关键项

| 项 | 配置 |
| --- | --- |
| Visual Studio | Version 18 (VS2026) |
| Platform Toolset | v145 |
| C++ 标准 | C++20 |
| 字符集 | Unicode |
| wxWidgets | 3.2.9（DLL 版本 `vc_x64_dll`，使用 `mswud` 头） |
| 宏 | `WXUSINGDLL;__WXMSW__;NOPCH;_CRT_SECURE_NO_WARNINGS` |
| 路径约定 | 全部 `$(SolutionDir)` 相对路径，**不依赖 WXWIN 环境变量** |
| PostBuildEvent | 自动把 `3rd\wxWidgets-3.2.9\lib\vc_x64_dll\*.dll` 拷到 `$(OutDir)` |

## clone 后第一步：恢复 wxWidgets

本仓库没有把 `wxWidgets-3.2.9` 提交进来（111 MB）。克隆后用下面任一方式恢复：

### 方式 A：从同机 SigFlow_FPGA 软链（推荐，零拷贝）

```cmd
mklink /D 3rd\wxWidgets-3.2.9 E:\EDA_Race\Cangku\new\SigFlow_FPGA\3rd\wxWidgets-3.2.9
```

### 方式 B：复制整个目录

```cmd
xcopy /E /I /Y E:\EDA_Race\Cangku\new\SigFlow_FPGA\3rd\wxWidgets-3.2.9 3rd\wxWidgets-3.2.9
```

### 方式 C：重新从 wxWidgets 官网下载

https://www.wxwidgets.org/downloads/

下载 `wxWidgets-3.2.9.7z` 解压到 `3rd/wxWidgets-3.2.9/`，然后用 `3rd/wxWidgets-3.2.9/build/msw/` 下的 `makefile.vc` 自行构建 `vc_x64_dll`。

## 编译

1. 双击 `SiliFlow.sln` 用 VS2026 打开
2. 选 `main` 项目 + 配置 `Debug | x64`
3. `Ctrl+Shift+B`
4. 产物在 `bin\x64\Debug\main.exe`，同目录会有 wxWidgets DLL，可双击运行

## 扩展 wxWidgets 子模块

默认只链接 `wxmsw32ud_core / wxbase32ud / wxbase32ud_xml`（Debug）。后续要用 AUI / GL / STC 等模块时，到 `main/main.vcxproj` 的 `AdditionalDependencies` 添加对应的 `wxmsw32ud_xxx.lib`（Release 则是无 `d` 后缀）。

## 平台建议

Win32 (x86) 配置目前只配了头文件路径，`3rd/wxWidgets-3.2.9` 没有 `vc_x86_dll`，所以 x86 编译会缺库。建议用 Configuration Manager 删除 Win32 配置保持清爽。

## 推送到 GitHub

```cmd
git remote add origin https://github.com/MrBear2-Rus/SiliFlow.git
git push -u origin main
```

如果是空仓库（含 `.gitattributes` 的初始 commit），可能需要：

```cmd
git push --force-with-lease origin main
```
