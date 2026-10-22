#pragma once

// =============================================================================
//  SiliFlow · app/MainMenuBar.h
//  菜单栏构造。菜单项 ID 集中在这里定义，MainFrame 负责绑定处理函数。
// =============================================================================

#include <wx/defs.h>

class wxMenuBar;

namespace sf
{

enum MenuId
{
    // ---- 文件（标准 ID 之外的部分）----
    ID_FILE_OPEN_VCD = wxID_HIGHEST + 2000,
    ID_FILE_RECENT_CLEAR,

    // ---- 视图 ----
    ID_VIEW_ZOOM_IN,
    ID_VIEW_ZOOM_OUT,
    ID_VIEW_ZOOM_FIT,
    ID_VIEW_ZOOM_RESET,
    ID_VIEW_SHOW_GRID,
    ID_VIEW_SHOW_PIN_NAMES,
    ID_VIEW_SHOW_LABELS,

    // ---- 面板显隐 ----
    ID_PANEL_TOOLBOX,
    ID_PANEL_PROPERTY,
    ID_PANEL_TREE,
    ID_PANEL_WAVE,
    ID_PANEL_CONSOLE,

    // ---- 工具 ----
    ID_TOOL_DRAG,
    ID_TOOL_SELECT,
    ID_TOOL_TEXT,
    ID_TOOL_COMPONENT,
    ID_TOOL_WIRE,
    ID_TOOL_DRAWING,
    ID_TOOL_ERASER,

    ID_DRAW_LINE,
    ID_DRAW_RECTANGLE,
    ID_DRAW_CIRCLE,

    ID_TOOL_ROTATE_CW,
    ID_TOOL_ROTATE_CCW,
    ID_TOOL_MIRROR,

    // ---- 图纸 ----
    ID_SHEET_NEW,
    ID_SHEET_CLOSE,

    // ---- 仿真 ----
    ID_SIM_RUN,
    ID_SIM_STOP,
    ID_SIM_STEP,
    ID_SIM_RESET,
    ID_SIM_FREQ_1,
    ID_SIM_FREQ_2,
    ID_SIM_FREQ_5,
    ID_SIM_FREQ_10,
    ID_SIM_FREQ_20,
    ID_SIM_TRUTH_TABLE,

    // ---- Verilog ----
    ID_VERILOG_EXPORT,
    ID_VERILOG_IMPORT,
    ID_VERILOG_SYNC_STATUS,
    ID_VERILOG_OPEN_FILE,

    // ---- 帮助 ----
    ID_HELP_SHORTCUTS,
    ID_HELP_SELFTEST
};

/// 构造完整菜单栏（调用方负责 SetMenuBar 与事件绑定）
wxMenuBar* CreateMainMenuBar();

}  // namespace sf
