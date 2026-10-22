#pragma once

// =============================================================================
//  SiliFlow · app/MainFrame.h
//  主窗口（L4 装配层）：
//    · 用 wxAUI 把各面板拼成工作台
//    · 持有 AppContext（工程 / 元件库 / 撤销栈）
//    · 把 Services 的命令回调接到「当前活动图纸」的画布上
//    · 菜单路由与文件生命周期
// =============================================================================

#include "AppContext.h"

#include <wx/aui/aui.h>
#include <wx/frame.h>
#include <wx/menu.h>
#include <wx/timer.h>

#include <vector>

class wxCloseEvent;
class wxCommandEvent;
class wxTimerEvent;
class wxUpdateUIEvent;

namespace sf
{

class CanvasNoteBook;
class CanvasPanel;
class ConsolePanel;
class PropertyPanel;
class ToolboxPanel;
class TreePanel;
class WavePanel;
class CircuitSheet;

// Apple 风格自定义外壳（定义见 MainFrame.cpp）
class AppTitleBar;
class AppToolBar;

class MainFrame : public wxFrame
{
public:
    explicit MainFrame(const wxString& title);
    ~MainFrame() override;

    /// 命令行 --selfcheck 时把结果打到「消息」面板
    void ShowSelfCheckReport(const wxString& report, int exitCode);

private:
    // ---- 装配 ----
    void SetupUi();
    void SetupAppChrome();
    void SetupAuiLayout();
    void SetupStatusBar();
    void BindMenuHandlers();
    void WireServices();

    // ---- Apple 外壳同步 ----
    void SyncToolBar();     ///< 把当前活动画布的工具同步到工具栏高亮
    void SyncSimButtons();  ///< 同步运行 / 停止按钮状态

    // ---- 辅助 ----
    CanvasPanel* ActiveCanvas() const;
    CircuitSheet* ActiveSheet() const;
    void UpdateTitle();
    void UpdateStatusBar();
    void UpdateUndoMenuState();
    void RefreshAllPanels();

    /// 有未保存改动时询问是否保存。返回 false 表示用户取消了当前操作。
    bool ConfirmDiscardChanges();

    void RebuildNotebook();

    // ---- 仿真 ----
    /// 把引擎挂到当前活动图纸并重算一次
    void RebindSim();
    /// 让画布与面板按新的电平重绘
    void RefreshSimViews();
    void SetSimClockHz(int hz);
    void ApplySimRunning(bool on);

    void OnSimRun(wxCommandEvent& event);
    void OnSimStop(wxCommandEvent& event);
    void OnSimStep(wxCommandEvent& event);
    void OnSimReset(wxCommandEvent& event);
    void OnSimFreq(wxCommandEvent& event);
    void OnSimTruthTable(wxCommandEvent& event);
    void OnSimTimer(wxTimerEvent& event);
    void OnUpdateSim(wxUpdateUIEvent& event);

    // ---- 文件 ----
    void OnFileNew(wxCommandEvent& event);
    void OnFileOpen(wxCommandEvent& event);
    void OnFileSave(wxCommandEvent& event);
    void OnFileSaveAs(wxCommandEvent& event);
    void OnFileOpenVcd(wxCommandEvent& event);
    void OnFileExit(wxCommandEvent& event);

    // ---- 编辑 ----
    void OnEditUndo(wxCommandEvent& event);
    void OnEditRedo(wxCommandEvent& event);
    void OnEditDelete(wxCommandEvent& event);
    void OnEditSelectAll(wxCommandEvent& event);
    void OnEditNotImplemented(wxCommandEvent& event);

    // ---- 视图 ----
    void OnViewZoomIn(wxCommandEvent& event);
    void OnViewZoomOut(wxCommandEvent& event);
    void OnViewZoomFit(wxCommandEvent& event);
    void OnViewZoomReset(wxCommandEvent& event);
    void OnViewToggleGrid(wxCommandEvent& event);
    void OnViewTogglePinNames(wxCommandEvent& event);
    void OnViewToggleLabels(wxCommandEvent& event);
    void OnViewTogglePanel(wxCommandEvent& event);
    void OnUpdateView(wxUpdateUIEvent& event);

    // ---- 工具 ----
    void OnToolSelect(wxCommandEvent& event);
    void OnDrawKindSelect(wxCommandEvent& event);
    void OnToolRotate(wxCommandEvent& event);
    void OnToolMirror(wxCommandEvent& event);
    void OnUpdateTool(wxUpdateUIEvent& event);

    // ---- 图纸 ----
    void OnSheetNew(wxCommandEvent& event);
    void OnSheetClose(wxCommandEvent& event);

    // ---- Verilog ----
    void OnVerilogExport(wxCommandEvent& event);
    void OnVerilogImport(wxCommandEvent& event);
    void OnVerilogSyncStatus(wxCommandEvent& event);
    void OnVerilogOpenFile(wxCommandEvent& event);

    // ---- 帮助 ----
    void OnHelpShortcuts(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    void OnHelpSelfCheck(wxCommandEvent& event);

    void OnClose(wxCloseEvent& event);

    AppContext m_ctx;
    wxAuiManager m_aui;

    CanvasNoteBook* m_notebook = nullptr;
    ToolboxPanel* m_toolbox = nullptr;
    PropertyPanel* m_property = nullptr;
    TreePanel* m_tree = nullptr;
    WavePanel* m_wave = nullptr;
    ConsolePanel* m_console = nullptr;
    wxPanel* m_bottomHost = nullptr;

    // ---- Apple 自定义外壳 ----
    AppTitleBar* m_titleBar = nullptr;
    AppToolBar* m_toolBar = nullptr;
    std::vector<wxMenu*> m_menus;  ///< 自绘标题栏里承载的弹出菜单（本窗口持有）

    /// 自动节拍。放在这里而不是 core —— 定时器是界面层的事。
    wxTimer m_simTimer;
    int m_simHz = 2;
};

}  // namespace sf
