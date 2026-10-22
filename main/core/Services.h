#pragma once

// =============================================================================
//  SiliFlow · core/Services.h
//  服务定位器：把「工程 / 元件库 / 撤销栈」以及一组回调打包，供 L3 各模块取用。
//
//  放在 core（L2）而不是 app（L4）是为了守住依赖方向：
//  L3 的 canvas/ panels/ 只能向下依赖 L2，不能反过来 include L4 的 AppContext。
//  真正的 AppContext（L4）继承本结构，并额外挂载 wxAUI 相关的东西。
// =============================================================================

#include <wx/string.h>

#include <functional>

namespace sf
{

class Library;
class Project;
class UndoStack;

namespace sim
{
class SimEngine;
}

struct Services
{
    virtual ~Services() = default;

    Project* project = nullptr;
    Library* library = nullptr;
    UndoStack* undo = nullptr;

    /// 仿真引擎。L3（画布 / 面板）从这里**只读**查询电平；控制走下面的 cmdSim*。
    sim::SimEngine* sim = nullptr;

    /// 写一条消息到「消息」面板
    std::function<void(const wxString&)> log;

    /// 图纸内容发生变化（供树面板、标题栏刷新）
    std::function<void()> onSheetChanged;

    /// 选中集发生变化（供属性面板刷新）
    std::function<void()> onSelectionChanged;

    /// 请求重绘画布
    std::function<void()> onRequestRefresh;

    /// 工具状态变化（供状态栏显示）
    std::function<void()> onToolStateChanged;

    /// 状态栏第二栏（光标坐标 / 缩放比例）
    std::function<void(const wxString&)> onStatusText;

    /// 当前活动图纸切换
    std::function<void()> onActiveSheetChanged;

    /// 需要「撤销/重做」菜单项状态刷新
    std::function<void()> onUndoStateChanged;

    /// 仿真状态变化：重绘导线颜色、刷新真值表 / 属性面板
    std::function<void()> onSimStateChanged;

    /// 仿真状态文本（状态栏第三栏）
    std::function<void(const wxString&)> onSimStatus;

    // -------------------------------------------------------------------------
    //  编辑命令：由 MainFrame 注入并转发到「当前活动图纸」的画布。
    //  panels/ 通过这里下命令，从而不必横向依赖 canvas/。
    // -------------------------------------------------------------------------

    /// 修改元件属性：prop ∈ { "label", "value", "x", "y", "rotation", "mirrored" }
    std::function<void(int compId, const wxString& prop, const wxString& value)> editComponent;

    /// 修改导线属性：prop ∈ { "signal" }
    std::function<void(int wireId, const wxString& prop, const wxString& value)> editWire;

    std::function<void()> cmdRotate;
    std::function<void()> cmdMirror;
    std::function<void()> cmdDelete;
    std::function<void()> cmdSelectAll;
    std::function<void(const wxString& type)> cmdBeginPlacement;

    /// 把某个网络对应的导线高亮并滚动到视野中央（波形 ↔ 画布联动）
    std::function<void(const wxString& netName)> cmdHighlightNet;

    /// 选中某个元件并滚动到视野中央（树面板双击定位）
    std::function<void(int compId)> cmdFocusComponent;

    /// 切换活动图纸
    std::function<void(const wxString& sheetName)> cmdActivateSheet;

    // ---- 仿真命令（同样由 MainFrame 注入）----
    std::function<void()> cmdSimRun;
    std::function<void()> cmdSimStop;
    std::function<void()> cmdSimStep;
    std::function<void()> cmdSimReset;
    /// 翻转一个输入端口的电平（画布上单击 input 端口时用）
    std::function<void(int compId)> cmdSimTogglePin;

    /// 设定强制电平。level 为空表示取消强制；否则是 "0" / "1" / "X" / "Z"。
    /// 不进撤销栈 —— 撤销只该撤图纸编辑。
    std::function<void(int compId, const wxString& level)> cmdSimForcePin;

    // ---- 便捷包装 ----
    void EditComponent(int compId, const wxString& prop, const wxString& value) const
    {
        if (editComponent) editComponent(compId, prop, value);
    }
    void EditWire(int wireId, const wxString& prop, const wxString& value) const
    {
        if (editWire) editWire(wireId, prop, value);
    }
    void Rotate() const
    {
        if (cmdRotate) cmdRotate();
    }
    void Mirror() const
    {
        if (cmdMirror) cmdMirror();
    }
    void Delete() const
    {
        if (cmdDelete) cmdDelete();
    }
    void SelectAll() const
    {
        if (cmdSelectAll) cmdSelectAll();
    }
    void BeginPlacement(const wxString& type) const
    {
        if (cmdBeginPlacement) cmdBeginPlacement(type);
    }
    void HighlightNet(const wxString& netName) const
    {
        if (cmdHighlightNet) cmdHighlightNet(netName);
    }
    void FocusComponent(int compId) const
    {
        if (cmdFocusComponent) cmdFocusComponent(compId);
    }
    void ActivateSheet(const wxString& sheetName) const
    {
        if (cmdActivateSheet) cmdActivateSheet(sheetName);
    }
    void SimRun() const
    {
        if (cmdSimRun) cmdSimRun();
    }
    void SimStop() const
    {
        if (cmdSimStop) cmdSimStop();
    }
    void SimStep() const
    {
        if (cmdSimStep) cmdSimStep();
    }
    void SimReset() const
    {
        if (cmdSimReset) cmdSimReset();
    }
    void SimTogglePin(int compId) const
    {
        if (cmdSimTogglePin) cmdSimTogglePin(compId);
    }
    void SimForcePin(int compId, const wxString& level) const
    {
        if (cmdSimForcePin) cmdSimForcePin(compId, level);
    }

    // ---- 便捷包装（回调为空时静默）----
    void Log(const wxString& msg) const
    {
        if (log) log(msg);
    }
    void NotifySheetChanged() const
    {
        if (onSheetChanged) onSheetChanged();
    }
    void NotifySelectionChanged() const
    {
        if (onSelectionChanged) onSelectionChanged();
    }
    void Refresh() const
    {
        if (onRequestRefresh) onRequestRefresh();
    }
    void NotifyToolState() const
    {
        if (onToolStateChanged) onToolStateChanged();
    }
    void NotifyStatus(const wxString& text) const
    {
        if (onStatusText) onStatusText(text);
    }
    void NotifyActiveSheetChanged() const
    {
        if (onActiveSheetChanged) onActiveSheetChanged();
    }
    void NotifyUndoStateChanged() const
    {
        if (onUndoStateChanged) onUndoStateChanged();
    }
    void NotifySimStateChanged() const
    {
        if (onSimStateChanged) onSimStateChanged();
    }
    void NotifySimStatus(const wxString& text) const
    {
        if (onSimStatus) onSimStatus(text);
    }
};

}  // namespace sf
