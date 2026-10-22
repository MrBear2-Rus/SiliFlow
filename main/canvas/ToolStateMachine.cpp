#include "ToolStateMachine.h"

namespace sf
{

wxString ToolDisplayName(ToolType t)
{
    switch (t)
    {
    case ToolType::Drag:      return "拖拽";
    case ToolType::Select:    return "选择";
    case ToolType::Text:      return "文本";
    case ToolType::Component: return "元件";
    case ToolType::Wire:      return "导线";
    case ToolType::Drawing:   return "绘图";
    case ToolType::Eraser:    return "擦除";
    }
    return "选择";
}

wxString ToolTypeName(ToolType t)
{
    switch (t)
    {
    case ToolType::Drag:      return "drag";
    case ToolType::Select:    return "select";
    case ToolType::Text:      return "text";
    case ToolType::Component: return "component";
    case ToolType::Wire:      return "wire";
    case ToolType::Drawing:   return "drawing";
    case ToolType::Eraser:    return "eraser";
    }
    return "select";
}

ToolType ToolTypeFromName(const wxString& s, ToolType def)
{
    if (s == "drag") return ToolType::Drag;
    if (s == "select") return ToolType::Select;
    if (s == "text") return ToolType::Text;
    if (s == "component") return ToolType::Component;
    if (s == "wire") return ToolType::Wire;
    if (s == "drawing") return ToolType::Drawing;
    if (s == "eraser") return ToolType::Eraser;
    return def;
}

// -----------------------------------------------------------------------------
//  状态机
// -----------------------------------------------------------------------------
void ToolStateMachine::SetTool(ToolType t)
{
    m_tool = t;
    // 切工具时所有子状态回 Idle
    m_drag = DragState::Idle;
    m_select = SelectState::Idle;
    m_text = TextState::Idle;
    m_component = ComponentState::Idle;
    m_wire = WireState::Idle;
    m_drawing = DrawingState::Idle;
    m_eraser = EraserState::Idle;

    Notify();
}

void ToolStateMachine::ResetToIdle()
{
    m_drag = DragState::Idle;
    m_select = SelectState::Idle;
    m_text = TextState::Idle;
    m_component = ComponentState::Idle;
    m_wire = WireState::Idle;
    m_drawing = DrawingState::Idle;
    m_eraser = EraserState::Idle;

    Notify();
}

void ToolStateMachine::SetDrag(DragState s)
{
    if (m_drag == s) return;
    m_drag = s;
    Notify();
}

void ToolStateMachine::SetSelect(SelectState s)
{
    if (m_select == s) return;
    m_select = s;
    Notify();
}

void ToolStateMachine::SetText(TextState s)
{
    if (m_text == s) return;
    m_text = s;
    Notify();
}

void ToolStateMachine::SetComponent(ComponentState s)
{
    if (m_component == s) return;
    m_component = s;
    Notify();
}

void ToolStateMachine::SetWire(WireState s)
{
    if (m_wire == s) return;
    m_wire = s;
    Notify();
}

void ToolStateMachine::SetDrawing(DrawingState s)
{
    if (m_drawing == s) return;
    m_drawing = s;
    Notify();
}

void ToolStateMachine::SetEraser(EraserState s)
{
    if (m_eraser == s) return;
    m_eraser = s;
    Notify();
}

wxString ToolStateMachine::DisplayText() const
{
    wxString sub;

    switch (m_tool)
    {
    case ToolType::Drag:
        switch (m_drag)
        {
        case DragState::CanvasDragging: sub = "平移中"; break;
        default: break;
        }
        break;
    case ToolType::Select:
        switch (m_select)
        {
        case SelectState::ClickSelect:     sub = "点选"; break;
        case SelectState::RectangleSelect: sub = "框选"; break;
        case SelectState::DragSelect:      sub = "拖动选中物"; break;
        default: break;
        }
        break;
    case ToolType::Text:
        switch (m_text)
        {
        case TextState::CanvasTextEditing:      sub = "编辑注释"; break;
        case TextState::ComponentLabelEditing:  sub = "改标签"; break;
        default: break;
        }
        break;
    case ToolType::Component:
        switch (m_component)
        {
        case ComponentState::ComponentPreview: sub = "放置预览"; break;
        default: break;
        }
        break;
    case ToolType::Wire:
        switch (m_wire)
        {
        case WireState::WireDrawing: sub = "拉线中"; break;
        case WireState::WireEditing: sub = "改拐点"; break;
        default: break;
        }
        break;
    case ToolType::Drawing:
        switch (m_drawing)
        {
        case DrawingState::DrawingRectangle: sub = "画矩形"; break;
        case DrawingState::DrawingLine:      sub = "画直线"; break;
        case DrawingState::DrawingCircle:    sub = "画圆"; break;
        default: break;
        }
        break;
    case ToolType::Eraser:
        switch (m_eraser)
        {
        case EraserState::ClickEraser:     sub = "点擦"; break;
        case EraserState::RectangleEraser: sub = "框擦"; break;
        default: break;
        }
        break;
    }

    return sub.IsEmpty() ? ToolDisplayName(m_tool) : ToolDisplayName(m_tool) + " · " + sub;
}

bool ToolStateMachine::IsBusy() const
{
    switch (m_tool)
    {
    case ToolType::Drag:      return m_drag != DragState::Idle;
    case ToolType::Select:    return m_select != SelectState::Idle;
    case ToolType::Text:      return m_text != TextState::Idle;
    case ToolType::Component: return m_component != ComponentState::Idle;
    case ToolType::Wire:      return m_wire != WireState::Idle;
    case ToolType::Drawing:   return m_drawing != DrawingState::Idle;
    case ToolType::Eraser:    return m_eraser != EraserState::Idle;
    }
    return false;
}

void ToolStateMachine::Notify()
{
    if (m_onChange) m_onChange();
}

}  // namespace sf
