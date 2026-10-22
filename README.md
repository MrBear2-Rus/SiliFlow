# SiliFlow

自研 EDA 原理图工具（VS2026 + wxWidgets 3.2.9）。当前进度：**界面框架跑通，组合逻辑实时仿真（S1）可用**。

## 现在能做什么

- wxAUI 工作台：左侧元件库 / 中间多标签画布 / 右侧属性 + 层次树 / 底部波形 + 消息
- 内置元件库（代码构造，25 个条目，零资源依赖）
- 画布：缩放平移、网格、7 种图元绘制、选择/框选/移动、7 类工具的状态机
- 元件放置、旋转 90°、水平镜像（保持中心不动）
- 导线：Z 形布线、引脚吸附、控制点拖拽、四值逻辑着色（0/1/X/Z）
- **组合逻辑实时仿真**：改一处电路立刻看到电平；单击输入端口翻转；组合环与驱动冲突判 `X`
- 快照式撤销/重做（图纸级 JSON 前后快照）
- 工程序列化（`.sfproj`，纯 JSON）、并查集网络推导、层次树定位
- `--selfcheck` 命令行自检：51 项 core 断言

**还没做**：时序 tick（`clock` / `dff` 尚未接入锁存状态，S2）、真值表（S3）、Verilog 双向同步（M3/M4，菜单已占位）、VCD 波形（M5）。**明确不做**：门级延迟 / 时序分析、高速连续仿真、三态强度仲裁、子电路层次 —— 见设计方案第 2 章。

## 仿真怎么用（S1）

1. 从元件库拖出 `input` 输入引脚、`and` 与门、`output` 输出引脚或 `led` 指示灯
2. `W` 切导线工具，依次点击要连的引脚
3. `S` 切选择工具，**单击输入端口** → 电平在 0 / 1 之间翻转，下游导线颜色立刻跟着变

| 操作 | 快捷键 |
| --- | --- |
| 运行 / 停止自动节拍 | `F5` / `F6` |
| 单步（走一个时钟拍） | `Ctrl+T` |
| 复位（清空强制电平与 tick 计数） | `Ctrl+Shift+T` |
| 节拍频率 | 菜单「仿真 → 节拍频率」，1~20 Hz |

导线颜色：`0` 深灰、`1` 亮绿、`X` 红、`Z`（没人驱动）淡灰。

状态栏最右一栏显示「元件数 / 网络数 / tick」，以及组合环、驱动冲突的提示。

属性面板里，**单引脚元件**（输入输出引脚、常量、接地、时钟、指示灯）会多出一组「仿真」，显示当前电平；输入端口还能勾「强制高电平」。

> **两条不变量**：翻转电平和单步 tick **不记入撤销栈**（撤销只撤图纸编辑）；仿真值只存在引擎内部，绝不写回图纸数据 —— 否则撤销会变成「往回撤电平」。

## 目录结构

```
SiliFlow-2026/
├── SiliFlow.sln                 VS2026 (Format Version 18)
├── docs/                        设计方案
├── 3rd/
│   ├── wxWidgets-3.2.9/         本仓库不含，clone 后需恢复（见下）
│   ├── nlohmann/json.hpp
│   └── vcd/                     VCD 解析（M5 用）
└── main/
    ├── main.vcxproj             4 配置：Debug/Release × Win32/x64，Toolset v145
    ├── GlobalConfig.props       include/lib 路径、OutDir/IntDir
    ├── cMain.cpp                入口 + 命令行（--selfcheck）
    ├── core/                    L2 数据层（纯数据，不碰 wx 控件）
    │   Types / Json / Shape / Component / Wire / Circuit / Library
    │   Project / Command / SelfCheck / Services / TextConv
    ├── core/sim/                L2 仿真内核
    │   SimTypes（四值运算）/ GateEval（按 gate·kind 分派）/ SimEngine
    ├── canvas/                  L3 画布子系统
    │   ToolStateMachine / CanvasRenderer / CanvasPanel
    │   CanvasEventHandler / CanvasNoteBook
    ├── panels/                  L3 面板（元件库 / 属性 / 层次树 / 波形 / 消息）
    ├── app/                     L4 装配层（AppContext / MainFrame / MainMenuBar）
    ├── verilog/                 预留（M3 / M4）
    └── res/
```

依赖方向是硬约束：**只能向下**（app → canvas/panels → core → 3rd）。`core/` 里不允许出现
`wxPanel` / `wxMouseEvent` 这类控件类型；L3 模块之间不横向依赖，跨模块走 `core::Services`
的回调。

## 与 SigFlow_FPGA 对齐的关键项

| 项 | 配置 |
| --- | --- |
| Visual Studio | Version 18 (VS2026) |
| Platform Toolset | v145 |
| C++ 标准 | C++20 |
| 字符集 | Unicode |
| wxWidgets | 3.2.9（DLL 版本 `vc_x64_dll`，使用 `mswud` 头） |
| 链接模块 | core / aui / adv / propgrid + base / base_xml |
| 宏 | `WXUSINGDLL;__WXMSW__;UNICODE;_UNICODE;NOPCH;_CRT_SECURE_NO_WARNINGS` |
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

下载 `wxWidgets-3.2.9.7z` 解压到 `3rd/wxWidgets-3.2.9/`，然后用 `build/msw/` 下的
`makefile.vc` 自行构建 `vc_x64_dll`。

## 编译

1. 双击 `SiliFlow.sln` 用 VS2026 打开
2. 选 `main` 项目 + 配置 `Debug | x64`
3. `Ctrl+Shift+B`
4. 产物在 `bin\x64\Debug\SiliFlow.exe`，同目录会有 wxWidgets DLL，可双击运行

## 命令行

```cmd
SiliFlow.exe                 启动 GUI
SiliFlow.exe --selfcheck     只跑 core 数据层自检，打印结果后退出（不建窗口）
```

自检结果同时写一份 `selfcheck.log` 到 exe 同目录（GUI 子系统程序从终端重定向
stdout 不一定拿得到，脚本里读这个文件更稳）。全通过时退出码 0。

## 两条容易踩的坑

**1. 中文字面量必须按 UTF-8 解释。**
源码用 `/utf-8` 编译，窄字符串字面量是 UTF-8 字节；而 wxWidgets 默认用「当前区域编码」
（简体中文 Windows 上是 CP936）去解释 `const char*`，中文会解成空串，接着
`wxString::Format` 检测到占位符与参数对不上而触发断言。`core/TextConv.h` 里的
`UseUtf8NarrowStrings()` 把 `wxConvLibcPtr` / `wxConvCurrent` 切到 UTF-8，
**必须在任何中文字面量被用到之前调用**，现在挂在 `SiliFlowApp::OnInit()` 最前面。
之后再加的程序入口（比如单独的控制台工具）也要记得调。

**2. 命令行开关要在 `OnInitCmdLine()` 里登记。**
`wxApp::OnInit()` 内部会解析命令行，遇到没声明的选项会直接返回 `false`，程序连窗口都不会
建。加新开关时到 `cMain.cpp` 的 `OnInitCmdLine()` 里 `AddSwitch`。

## 平台建议

Win32 (x86) 配置目前只配了头文件路径，`3rd/wxWidgets-3.2.9` 没有 `vc_x86_dll`，所以 x86
编译会缺库。建议用 Configuration Manager 删除 Win32 配置保持清爽。

## 推送到 GitHub

```cmd
git remote add origin https://github.com/MrBear2-Rus/SiliFlow.git
git push -u origin main
```

如果是空仓库（含 `.gitattributes` 的初始 commit），可能需要：

```cmd
git push --force-with-lease origin main
```
