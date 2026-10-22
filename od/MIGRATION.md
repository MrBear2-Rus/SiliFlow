# SiliFlow · Apple 现代风改造（源码级）

本次把 `E:\EDA_Race\Cangku\SiliFlow-2026\main` 下的 wxWidgets 应用原地改成 Apple 现代风格：
Apple 中性配色 + 统一字体 + 自绘标题栏（内含菜单）+ 自绘工具栏 + 扁平 wxAUI 工作台。
`index.html` / `styles.css` / `app.js` 是同一风格的浏览器原型，作为**设计参照**保留。

> 说明：本环境没有 VS2026 / wxWidgets 工具链，改动未做编译验证（按你要求，由 workbuddy 编译）。
> 改动停留在「可审阅源码」层面，遇到编译点请按下方“已知风险”微调。

## 改动的文件（源工程内）

| 文件 | 改动 |
| --- | --- |
| `main/core/Types.h` | 重写 `palette` 为 Apple 中性配色；新增 `namespace ui`（外壳取色 + 统一字体） |
| `main/app/MainFrame.h` | 新增 `SetupAppChrome / SyncToolBar / SyncSimButtons`、外壳成员、前置声明 |
| `main/app/MainFrame.cpp` | 新增自绘外壳（标题栏 / 工具栏 / 按钮 / 图标）；去掉原生标题栏与菜单栏；扁平 AUI 外观；状态栏配色 |

改动文件已镜像到本项目 `restyle/` 目录，便于审阅与对比。

## 设计令牌（来自原型 Design Spec）

- 强调色 Apple 蓝 `#007AFF`；危险色 `#FF3B30`
- 底：标题栏/状态栏 `#E9E9EC`、工具栏 `#F1F1F4`、内容 `#F5F5F7`、面板/画布白 `#FFFFFF` / `#FBFBFC`
- 文字：主 `#1D1D1F`、次 `#6E6E73`
- 逻辑电平：`0` 灰 `#8A8A90`、`1` 绿 `#34C759`、`X` 红 `#FF3B30`、`Z` 浅灰 `#C9C9CE`
- 字体：`Microsoft YaHei UI`（SF Pro 同族回退），9–10pt
- 圆角 5–8px，单线条矢量图标 stroke 1.5–1.6

## 主要实现点

1. **配色集中**：`core/Types.h` 的 `palette::` 是唯一取色源，画布渲染（`CanvasRenderer` / `Wire`）
   自动跟随，无需改动渲染代码。
2. **自定义标题栏**（`AppTitleBar`）：去掉原生 `wxCAPTION`，用自绘条承载
   App 标记 + “SiliFlow” + 菜单（文件/编辑/视图/工具/仿真/图纸/Verilog/帮助）+ 文档名 + 窗口按钮。
   支持拖动窗口、双击最大化。
3. **菜单迁移**：`SetupAppChrome()` 调 `CreateMainMenuBar()` 后逐个 `Remove()` 取出顶层菜单，
   交给标题栏文字按钮 `PopupMenu` 弹出 —— 菜单项、快捷键、`wxEVT_UPDATE_UI` 更新逻辑**保持不变**。
4. **自定义工具栏**（`AppToolBar`）：7 个工具 + 旋转/镜像 + 缩放 + 仿真（运行/停止/单步/复位），
   图标全部矢量自绘（零位图资源），点击发送与菜单相同的 `wxEVT_MENU` ID 复用现有处理函数。
5. **扁平 wxAUI**：无渐变、细分割线（`#E2E2E6`）、面板白底、标题使用次色。
6. **状态栏**：Apple 中性底 + 次级文字色。

## 编译

按原工程方式编译即可（无需新增文件、无需改 `.vcxproj`）：
1. VS2026 打开 `SiliFlow.sln`，选 `main` + `Debug | x64`
2. `Ctrl+Shift+B`；产物 `bin\x64\Debug\SiliFlow.exe`

## 已知风险 / 需你本机确认

- **无编译验证**：可能存在个别编译点（API 版本差异）。全部改动集中在 3 个文件，便于定位。
- **无边框标题栏**：去掉了原生 caption，拖动/缩放的边缘行为依赖平台；窗口缩放走 `wxRESIZE_BORDER`（保留）。
  若拖动体验不佳，把 `AppTitleBar::OnMotion` 换成平台原生拖拽即可。
- **原生控件**：`wxTreeCtrl` / `wxPropertyGrid` / `wxStaticText` 是系统控件，只能改配色 / 字体 / 间距，
  无法做到像素级 Apple 复刻（原计划即如此）。
- **菜单不再显示在窗口顶端菜单栏**：改为标题栏内的文字按钮弹出（VS Code 式）。若你更想保留原生菜单栏，
  删掉 `SetupAppChrome()` 里的取菜单逻辑、恢复 `SetMenuBar(CreateMainMenuBar())` 即可。

## 回退

三个文件均可用 git 直接还原：
```
git -C E:\EDA_Race\Cangku\SiliFlow-2026 checkout -- main/core/Types.h main/app/MainFrame.h main/app/MainFrame.cpp
```
