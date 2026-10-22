#pragma once

// =============================================================================
//  SiliFlow · canvas/ToolStateMachine.h
//  工具状态机：7 个主工具，每个工具有自己的子状态。
//
//  约定：
//   * 切换主工具时一律把所有子状态重置为 Idle；
//   * Esc 从任意子状态回 Idle；
//   * 任何状态变化都会触发回调，供状态栏与工具面板同步显示。
// =============================================================================

#include <wx/string.h>

#include <functional>

namespace sf
{

// -----------------------------------------------------------------------------
//  主工具
// -----------------------------------------------------------------------------
enum class ToolType
{
    Drag,       ///< 拖拽画布（默认可用中键替代）
    Select,     ///< 选择（默认工具）
    Text,       ///< 文本注释 / 改标签
    Component,  ///< 放置元件
    Wire,       ///< 拉导线 / 改拐点
    Drawing,    ///< 画装饰图形
    Eraser      ///< 擦除
};

wxString ToolDisplayName(ToolType t);
wxString ToolTypeName(ToolType t);
ToolType ToolTypeFromName(const wxString& s, ToolType def = ToolType::Select);

// -----------------------------------------------------------------------------
//  各工具的子状态
// -----------------------------------------------------------------------------
enum class DragState
{
    Idle,
    CanvasDragging
};

enum class SelectState
{
    Idle,
    ClickSelect,
    RectangleSelect,
    DragSelect
};

enum class TextState
{
    Idle,
    CanvasTextEditing,
    ComponentLabelEditing
};

enum class ComponentState
{
    Idle,
    ComponentPreview
};

enum class WireState
{
    Idle,
    WireDrawing,
    WireEditing
};

enum class DrawingState
{
    Idle,
    DrawingRectangle,
    DrawingLine,
    DrawingCircle
};

enum class EraserState
{
    Idle,
    ClickEraser,
    RectangleEraser
};

// -----------------------------------------------------------------------------
//  状态机
// -----------------------------------------------------------------------------
class ToolStateMachine
{
public:
    using ChangeCallback = std::function<void()>;

    void SetChangeCallback(ChangeCallback cb) { m_onChange = std::move(cb); }

    ToolType Tool() const { return m_tool; }
    void SetTool(ToolType t);

    /// 所有子状态回到 Idle（并通知）。
    void ResetToIdle();

    // ---- 子状态读写 ----
    DragState Drag() const { return m_drag; }
    void SetDrag(DragState s);

    SelectState Select() const { return m_select; }
    void SetSelect(SelectState s);

    TextState Text() const { return m_text; }
    void SetText(TextState s);

    ComponentState Component() const { return m_component; }
    void SetComponent(ComponentState s);

    WireState Wire() const { return m_wire; }
    void SetWire(WireState s);

    DrawingState Drawing() const { return m_drawing; }
    void SetDrawing(DrawingState s);

    EraserState Eraser() const { return m_eraser; }
    void SetEraser(EraserState s);

    // ---- 展示 ----
    /// 例如「选择 · 框选」
    wxString DisplayText() const;

    /// 是否有正在进行的子状态（用于判断 Esc 是否需要先取消）
    bool IsBusy() const;

private:
    void Notify();

    ToolType m_tool = ToolType::Select;

    DragState m_drag = DragState::Idle;
    SelectState m_select = SelectState::Idle;
    TextState m_text = TextState::Idle;
    ComponentState m_component = ComponentState::Idle;
    WireState m_wire = WireState::Idle;
    DrawingState m_drawing = DrawingState::Idle;
    EraserState m_eraser = EraserState::Idle;

    ChangeCallback m_onChange;
};

}  // namespace sf
