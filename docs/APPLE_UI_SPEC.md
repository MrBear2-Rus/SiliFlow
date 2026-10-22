# SiliFlow · Apple 视觉改造规范（AI 执行清单）

> 用途：给后续接手改界面的 AI / 工程师一份**可执行清单**。目标是把 SiliFlow 的界面
> 做成 Apple HIG 风格的**近似**（中性色、Apple 蓝、圆角、单线条矢量图标），保留跨平台，
> **不动**数据层与仿真内核。所有条目都可以逐条勾选验收。

---

## 0. 目标与边界

- **风格目标**：Apple 现代风「风格化」，不是像素级 macOS 复刻。
- **平台**：Windows 为主（自绘近似），兼容 macOS / Linux。
- **不改变**：功能行为、快捷键、菜单命令、`core/` 数据模型与 `core/sim` 仿真内核。
- **依赖方向（硬约束）**：`app → canvas/panels → ui → core → 3rd`。`ui/` 是叶子层，
  **不许**依赖 `canvas` / `panels` / `app`。

---

## 1. 架构与「唯一真源」

| 层 | 位置 | 职责 |
| --- | --- | --- |
| 设计令牌 | `main/core/Types.h` | `palette::`（画布/元件/连线/逻辑电平）、`ui::`（外壳/文字/语义/间距/圆角/字号/控件尺寸）、`ui::UIFace()/MonoFace()` |
| 视觉组件 | `main/ui/UiKit.h` / `UiKit.cpp` | `Glyph`、`DrawGlyph`、`Button`、`SegmentedGroup`、`MenuBar`、`PanelHeader`、`Chip`、`MakeDockArt`、`MakeTabArt`、`MakeSeparator`、`DrawEmptyState` |
| 外壳 | `main/app/MainFrame.cpp`（+`.h`） | `AppTitleBar` / `AppMenuBar` / `AppToolBar` / `SetupUi` / `SetupAuiLayout` / `SetupAppChrome` |
| 画布 | `main/canvas/` | `CanvasRenderer`、`Wire`、`CanvasNoteBook`（用 `ui::MakeTabArt()`）、`CanvasPanel` |
| 面板 | `main/panels/` | `ToolboxPanel`、`PropertyPanel`、`TreePanel`、`WavePanel`、`ConsolePanel` |

**自绘/原生开关**：`SF_CUSTOM_CHROME`（在 `MainFrame.cpp` 与工程宏里控制）。
- `ON`：去掉原生标题栏，用自绘标题栏 + 菜单 + 工具栏（Apple 观感，截图版本）。
- `OFF`：保留系统标题栏，只自绘工具栏（窗口拖动/缩放/吸附交给系统，最稳）。

---

## 2. 硬性规则

**必须（Do）**
1. 颜色 / 字号 / 间距 / 圆角**一律**取自 `ui::` 或 `palette::`，不写死十六进制。
2. 所有像素尺寸过 `window->FromDIP(...)`（`MainFrame.cpp` 内有 `Dip(win, v)` 辅助）。
3. 图标用 `ui::Glyph` + `ui::DrawGlyph()`（矢量、单线条，stroke 与圆角一致），零位图。
4. 通用控件先用 `ui::Button / SegmentedGroup / MenuBar / PanelHeader / Chip / MakeSeparator`；
   缺的**加到 `ui/UiKit`**，不允许在模块里再画一套。
5. 面板头用 `ui::PanelHeader`，面板内小工具条用 `ui::ToolStrip`。
6. 保持命令 ID（`MainMenuBar.h` 的 `MenuId`）与 `wxEVT_MENU` / `wxEVT_UPDATE_UI` 绑定不变。

**禁止（Don't）**
- 硬编码颜色 / `FaceName`；位图图标；emoji 当功能图标。
- 渐变、玻璃、霓虹、超大圆角、无意义的卡片堆叠。
- 改 `core/` 与 `core/sim/`；改依赖方向；在 `ui/` 里 `#include` 上层模块。
- 顺手删/注释已有功能。

---

## 3. 设计令牌速查

**画布 / 逻辑电平 —— `palette::`（浅色）**
`kCanvasBg #FBFBFC` · `kGrid #ECECEE` · `kGridMajor #DBDBDF` · `kComponentStroke #33373D` ·
`kComponentFill #FFFFFF` · `kText #1D1D1F` · `kTextDim #6E6E73` · `kSelect/kWireSelect/kPinHot #007AFF` ·
`kPin #8E8E93` · 逻辑电平：`kLogicZero #8A8A90` · `kLogicOne #34C759` · `kLogicX #FF3B30` · `kLogicZ #C9C9CE`

**外壳 / 语义 —— `ui::`**
强调 `kAccent #007AFF`（按下 `kAccentHot #0068D6`，淡底 `kAccentWeak`）；
底：`kChrome #E9E9EC`（标题栏/状态栏）· `kChromeAlt #F7F7F9`（工具栏/工具条）· `kContent #EFEFF2`（缝）·
`kPanel #FFFFFF` · `kPanelAlt #FAFAFC`；线：`kSeparator #E2E2E6` · `kBorderStrong #D4D4DA`；
字：`kTitleText #1D1D1F` · `kBodyText #3A3A3C` · `kSecondary #6E6E73` · `kTertiary` · `kOnAccent #FFFFFF`；
语义：`kGreen #34C759` · `kDanger #FF3B30` · `kWarn #FF9500`；交互底：`kHover #ECECF0` · `kPress #E3E3E8`。

**节奏 —— `ui::space`**：`kXxs 2 / kXs 4 / kSm 6 / kMd 8 / kLg 12 / kXl 14`
**圆角 —— `ui::radius`**：`kXs 5 / kSm 6 / kMd 7 / kLg 8 / kXl 9 / k2Xl 12`
**字号(pt) —— `ui::fontsize`**：`kCaption 8 / kBody 9 / kHead 10 / kDisplay 12`
**控件尺寸 —— `ui::metric`**：标题栏 38、菜单栏 30、工具栏 46、工具按钮 32、窗口按钮 34、
面板头 32、状态栏 26、标签页 35、左栏 264、右栏 316、底栏 224 …（详见 `Types.h`）

**字体**：`ui::UIFace()`（中文优先的族名探测）、`ui::MonoFace()`。**不要**再写死 `"Microsoft YaHei UI"`。

---

## 4. 组件 ↔ 原型 CSS 对照（`ui/UiKit.h` 顶部注释）

`ui::Button(Style::Text)↔.btn` · `Icon↔.tbtn` · `IconLabel↔.tbtn+.lbl` · `SegItem↔.seg button` ·
`Primary↔.tbtn.primary` · `Window↔.titlebar .tbtn` · `SegmentedGroup↔.seg` · `MenuBar↔.menubar` ·
`PanelHeader↔.panel-head` · `DrawEmptyState↔.wave-center/.empty-note`。

---

## 5. 待办清单（逐条：现状 → 目标 → 文件 → 验收）

### 5.1 标题栏菜单完整性 / 顺序（含「文件」）
- **现状**：截图里顶层菜单缺「文件」，只剩 编辑/视图/工具/仿真/图纸/Verilog/帮助。
- **目标**：8 个顶层菜单齐全且顺序与 `CreateMainMenuBar()` 一致；展开态用 `MenuBar::SetOpen` 高亮。
- **文件**：`app/MainFrame.cpp` `SetupAppChrome()`（自定义外壳分支）、`app/MainMenuBar.cpp`、`ui/UiKit.*`(`MenuBar`)。
- **要点**：检查从 `wxMenuBar` 取菜单的循环（`GetMenuLabel(0)` + `Remove(0)`）与 `StripAmp()`；
  确认取满 8 条、标签不因 `&`/`\t` 截断丢掉首项。更稳的做法：直接在 `MainMenuBar` 暴露
  `std::vector<std::pair<wxString,wxMenu*>> CreateMainMenuList()`，两处（原生/自绘）共用同一顺序。
- **验收**：顶部依次出现 文件/编辑/视图/工具/仿真/图纸/Verilog/帮助，点击都能弹出且命令正常。

### 5.2 波形空状态重影
- **现状**：右下占位文字重叠（“解析与绘制”出现两遍）。
- **目标**：`ui::DrawEmptyState` 一次画两行（主文案 + 说明），垂直留距、不重叠；面板显式设 `m_placeholder`。
- **文件**：`panels/WavePanel.cpp`（`WaveCanvas::OnPaint` + `WavePanel` 构造）、`ui/UiKit.cpp`(`DrawEmptyState`)。
- **验收**：空状态下居中两行清晰，无重影；缩放/改尺寸不残留。

### 5.3 工具栏右端多余图标
- **现状**：工具栏最右端多出一个游离的 ⟳。
- **目标**：工具栏内容与 `AppToolBar` 定义严格一致，末尾只有 `AddStretchSpacer`，无多余按钮。
- **文件**：`app/MainFrame.cpp`(`AppToolBar`)、`SetupAuiLayout()`。
- **要点**：核对 AUI 顶部面板未显示面板按钮 → `.CloseButton(false).PinButton(false).MaximizeButton(false).MinimizeButton(false).Gripper(false)`。
- **验收**：工具栏最右无游离图标。

### 5.4 面板 Apple 化（元件库 / 属性 / 层次树 / 消息 / 波形）
- **目标**：底色 `ui::kPanel`（次要区 `kPanelAlt`）；字体 `ui::Font(fontsize::kBody)`；行高与圆角对齐令牌；
  选中高亮用 `kAccentWeak`；分组/标题用 `ui::PanelHeader`。
- **文件**：`panels/*.cpp`。
- **要点**：
  - `wxTreeCtrl`：`SetBackgroundColour(kPanel)`、`SetForegroundColour(kTextTitle)`、`SetFont(...)`、
    `SetItemBackgroundColour`/`SetItemTextColour`（选中态）；行距留白。
  - `wxPropertyGrid`：`SetMarginColour(kPanelAlt)`、`SetCaptionBackgroundColour(kPanelAlt)`、
    `SetCaptionTextColour(kSecondary)`、`SetCellBackgroundColour(kPanel)`、`SetCellTextColour(kTextTitle)`、
    `SetLineColour(kSeparator)`、`SetSelectionBackgroundColour(kAccentWeak)`。
  - `wxSearchCtrl` / `wxTextCtrl`：底色/字色 / `SetFont(ui::MonoFace())`（消息区用等宽）。
- **验收**：四个面板与标题栏/工具栏视觉统一；选中项清晰；无系统灰。

### 5.5 标签页（CanvasNoteBook）
- **现状**：已接 `ui::MakeTabArt()`（`canvas/CanvasNoteBook.cpp`）。
- **目标**：高度 35、激活页白底 + 圆角、未激活透明、关闭按钮悬停红。
- **验收**：多图纸切换时页签符合规范。

### 5.6 状态栏
- **目标**：底 `ui::kChrome`、字 `ui::kSecondary`、段间发丝分隔、图标/数值用等宽、左对齐。
- **文件**：`app/MainFrame.cpp` `SetupStatusBar()`。
- **验收**：四段（工具 / 缩放 / 计数 / 仿真）对齐清晰。

### 5.7 窗口按钮：最大化 / 还原
- **现状**：最大化图标不随状态变化；无边框最大化可能压住任务栏。
- **目标**：`IsMaximized()` 时 `Max`→`Restore` 图标；最大化后仍能在任务栏上方正常显示。
- **文件**：`app/MainFrame.cpp`（`AppTitleBar`），绑定 `wxEVT_MAXIMIZE` / `wxEVT_SIZE` 更新 `m_winMax` 的 `Glyph`。
- **验收**：双击标题栏 / 点最大化后图标变「还原」，再点复原。

### 5.8 画布细节
- **目标**：选中环 `palette::kSelect`、引脚 hover `kPinHot`、网格 `kGrid`/`kGridMajor`、
  绘制图形默认描边与 `palette::kComponentStroke` 一致（均已集中，核对即可）。
- **文件**：`canvas/CanvasRenderer.cpp`、`canvas/Wire.cpp`（如需）。
- **验收**：浅色下无刺眼对比；选中/LED/电平着色正确。

### 5.9 滚动条
- **目标**：细、圆头、低对比（`kBorderStrong`），尽量隐藏到悬停才明显。
- **文件**：`panels/*`、`canvas/*`（承载 `wxScrolled`/树的窗口）。
- **验收**：拖动区够用、观感纤细；**不牺牲**可用性。

---

## 6. 深色模式实现规范（尚未落地，按此实现）

1. **令牌层**（`core/Types.h`）：把 `palette::` / `ui::` 里的 `inline const wxColour` 改成**可写**
   `inline wxColour`；新增 `namespace theme { inline bool dark = false; }` 与
   `inline void ApplyTheme(bool dark)`，在函数内按浅 / 深两套一次性赋值全部令牌。
   深色取值建议：`kChrome #262628`、`kChromeAlt #2A2A2D`、`kContent #1C1C1E`、`kPanel #2C2C2E`、
   `kPanelAlt #232325`、`kSeparator #3A3A3C`、`kTitleText #F5F5F7`、`kSecondary/kTertiary #9A9A9F`、
   `kAccent #0A84FF`；画布 `kCanvasBg #1B1B1D`、`kGrid #26262A`、`kGridMajor #313138`、
   `kComponentStroke #DCDCE0`、`kComponentFill #2C2C2E`、`kText #F5F5F7`、
   逻辑电平 `0 #98989D / 1 #30D158 / X #FF453A / Z #48484A`。
2. **入口**：标题栏 `Glyph::ThemeMoon` 按钮 + 「视图 → 深色外观」勾选项，复用**同一命令 ID**。
3. **切换动作**：`ApplyTheme(dark)` → 重设 `m_aui.SetArtProvider(ui::MakeDockArt())`（用新令牌重刷）→
   递归重设各面板与原生控件的 `SetBackgroundColour/SetForegroundColour` → `Refresh()` 全部面板与画布。
4. **已知限制**：`wxTreeCtrl`/`wxPropertyGrid`/`wxTextCtrl`/`wxChoice` 是系统控件，只能设底色/字色，
   接受「近似」；不要为此引入第三方皮肤库。
5. **验收**：切换后标题栏 / 工具栏 / 面板 / 画布 / 状态栏整体变深，文字对比 ≥ 4.5:1；
   （可选）把选择写入配置，重启记忆。

---

## 7. 构建与验收

- 编译：交给 **workbuddy** 或 VS2026（`main` 项目，`Debug|x64`，`SF_CUSTOM_CHROME` 按需）。本环境无工具链，
  改动以「可审阅源码」交付。
- 逐条走第 5、6 节清单并截图复核：标题栏菜单齐全 → 工具栏无游离图标 → 波形空状态干净 →
  各面板统一 → 深色切换 → 最大/还原 → 状态栏 → 滚动条。
- 每完成一项，更新本文件的勾选状态（建议在标题前加 `[x]`）。

## 8. 反模式（再次强调）

硬编码颜色/字体 · 位图/emoji 图标 · 渐变/玻璃/霓虹 · 模块内重复造按钮 · 改 `core`/依赖方向 ·
顺手删功能 · 用 `border:1px + radius:24` 假装原生控件。
