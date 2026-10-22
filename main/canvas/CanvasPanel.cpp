#include "CanvasPanel.h"

#include "CanvasEventHandler.h"

#include <wx/dcbuffer.h>
#include <wx/graphics.h>

#include <cmath>

namespace sf
{

CanvasPanel::CanvasPanel(wxWindow* parent, Services* services, wxWindowID id)
    : wxPanel(parent, id, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS | wxBORDER_NONE),
      m_services(services)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(palette::kCanvasBg);

    // 导线 / 指示灯的电平来源：仿真引擎算出来的值优先于 wire->signal
    m_options.sim = (services != nullptr) ? services->sim : nullptr;

    m_view.scale = 1.0;
    m_view.offset = Point(40, 40);
    m_view.clientSize = GetClientSize();

    m_handler = std::make_unique<CanvasEventHandler>(this);

    m_tools.SetChangeCallback(
        [this]
        {
            if (m_services != nullptr) m_services->NotifyToolState();
            Refresh(false);
        });

    BindEvents();
}

CanvasPanel::~CanvasPanel()
{
    // 先把指向 handler 内部成员的瞬态指针清掉，再销毁 handler
    m_options.previewComponent = nullptr;
    m_options.rubberBand = nullptr;
    m_options.selectionRect = nullptr;
    m_options.eraserRect = nullptr;
    m_options.previewShape = nullptr;

    m_handler.reset();
}

void CanvasPanel::BindEvents()
{
    Bind(wxEVT_PAINT, &CanvasPanel::OnPaint, this);
    Bind(wxEVT_ERASE_BACKGROUND, &CanvasPanel::OnEraseBackground, this);
    Bind(wxEVT_LEFT_DOWN, &CanvasPanel::OnMouseLeftDown, this);
    Bind(wxEVT_LEFT_UP, &CanvasPanel::OnMouseLeftUp, this);
    Bind(wxEVT_LEFT_DCLICK, &CanvasPanel::OnLeftDClick, this);
    Bind(wxEVT_RIGHT_DOWN, &CanvasPanel::OnMouseRightDown, this);
    Bind(wxEVT_RIGHT_UP, &CanvasPanel::OnMouseRightUp, this);
    Bind(wxEVT_MIDDLE_DOWN, &CanvasPanel::OnMouseMiddleDown, this);
    Bind(wxEVT_MIDDLE_UP, &CanvasPanel::OnMouseMiddleUp, this);
    Bind(wxEVT_MOTION, &CanvasPanel::OnMouseMove, this);
    Bind(wxEVT_LEAVE_WINDOW, &CanvasPanel::OnMouseLeave, this);
    Bind(wxEVT_MOUSEWHEEL, &CanvasPanel::OnMouseWheel, this);
    Bind(wxEVT_KEY_DOWN, &CanvasPanel::OnKeyDown, this);
    Bind(wxEVT_KEY_UP, &CanvasPanel::OnKeyUp, this);
    Bind(wxEVT_SIZE, &CanvasPanel::OnSize, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, &CanvasPanel::OnCaptureLost, this);
}

// -----------------------------------------------------------------------------
//  图纸
// -----------------------------------------------------------------------------
void CanvasPanel::SetSheet(CircuitSheet* sheet)
{
    m_sheet = sheet;
    ClearHover();
    m_options.previewComponent = nullptr;
    m_options.rubberBand = nullptr;
    m_options.selectionRect = nullptr;
    m_options.eraserRect = nullptr;
    m_options.previewShape = nullptr;
    Refresh(false);
}

// -----------------------------------------------------------------------------
//  视图
// -----------------------------------------------------------------------------
void CanvasPanel::ZoomAt(double newScale, const Point& deviceAnchor)
{
    const double clamped = std::clamp(newScale, kMinScale, kMaxScale);
    if (std::abs(clamped - m_view.scale) < 1e-9) return;

    // 锚点在画布坐标系下的位置保持不变
    const Point canvasAnchor = m_view.DeviceToCanvas(deviceAnchor);
    m_view.scale = clamped;
    m_view.offset.x = deviceAnchor.x - static_cast<int>(std::lround(canvasAnchor.x * clamped));
    m_view.offset.y = deviceAnchor.y - static_cast<int>(std::lround(canvasAnchor.y * clamped));

    Refresh(false);
}

void CanvasPanel::ZoomBy(double factor, const Point& deviceAnchor)
{
    ZoomAt(m_view.scale * factor, deviceAnchor);
}

void CanvasPanel::ZoomFit()
{
    const wxSize client = GetClientSize();
    m_view.clientSize = client;

    if (m_sheet == nullptr || m_sheet->IsEmpty())
    {
        m_view.scale = 1.0;
        m_view.offset = Point(40, 40);
        Refresh(false);
        return;
    }

    const Rect b = m_sheet->Bounds();
    if (b.width <= 0 || b.height <= 0)
    {
        m_view.scale = 1.0;
        m_view.offset = Point(40, 40);
        Refresh(false);
        return;
    }

    const int margin = 40;
    const double sx = static_cast<double>(client.x - margin * 2) / b.width;
    const double sy = static_cast<double>(client.y - margin * 2) / b.height;
    m_view.scale = std::clamp(std::min(sx, sy), kMinScale, kMaxScale);

    const Point center(b.x + b.width / 2, b.y + b.height / 2);
    CenterOn(center);
    Refresh(false);
}

void CanvasPanel::ZoomReset()
{
    const wxSize client = GetClientSize();
    const Point centerCanvas = m_view.DeviceToCanvas(Point(client.x / 2, client.y / 2));
    m_view.scale = 1.0;
    CenterOn(centerCanvas);
    Refresh(false);
}

void CanvasPanel::PanBy(const Point& deviceDelta)
{
    m_view.offset.x += deviceDelta.x;
    m_view.offset.y += deviceDelta.y;
    Refresh(false);
}

void CanvasPanel::CenterOn(const Point& canvasPoint)
{
    const wxSize client = GetClientSize();
    m_view.offset.x = client.x / 2 - static_cast<int>(std::lround(canvasPoint.x * m_view.scale));
    m_view.offset.y = client.y / 2 - static_cast<int>(std::lround(canvasPoint.y * m_view.scale));
}

// -----------------------------------------------------------------------------
//  吸附
// -----------------------------------------------------------------------------
Point CanvasPanel::Snap(const Point& canvasPoint, bool forceOff) const
{
    if (forceOff || !m_snapEnabled) return canvasPoint;
    return SnapPoint(canvasPoint, grid::kFine);
}

// -----------------------------------------------------------------------------
//  悬停
// -----------------------------------------------------------------------------
void CanvasPanel::SetHover(int compId, int pinIndex, int wireId)
{
    if (m_options.hoverComponentId == compId && m_options.hoverPinIndex == pinIndex &&
        m_options.hoverWireId == wireId)
        return;

    m_options.hoverComponentId = compId;
    m_options.hoverPinIndex = pinIndex;
    m_options.hoverWireId = wireId;
    Refresh(false);
}

void CanvasPanel::ClearHover()
{
    SetHover(-1, -1, -1);
}

// -----------------------------------------------------------------------------
//  状态广播
// -----------------------------------------------------------------------------
void CanvasPanel::NotifyContentChanged()
{
    if (m_services != nullptr) m_services->NotifySheetChanged();
    Refresh(false);
}

void CanvasPanel::NotifySelectionChanged()
{
    if (m_services != nullptr) m_services->NotifySelectionChanged();
    Refresh(false);
}

void CanvasPanel::NotifyUndoStateChanged()
{
    if (m_services != nullptr) m_services->NotifyUndoStateChanged();
}

void CanvasPanel::UpdateStatusText(const Point& canvasPos)
{
    if (m_services == nullptr) return;
    m_services->NotifyStatus(wxString::Format("缩放 %d%%  │  光标 (%d, %d)",
                                              static_cast<int>(std::lround(m_view.scale * 100)), canvasPos.x,
                                              canvasPos.y));
}

// -----------------------------------------------------------------------------
//  绘制
// -----------------------------------------------------------------------------
void CanvasPanel::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    wxAutoBufferedPaintDC dc(this);

    m_view.clientSize = GetClientSize();

    wxGraphicsContext* gc = wxGraphicsContext::Create(dc);
    if (gc == nullptr) return;

    gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
    gc->SetInterpolationQuality(wxINTERPOLATION_FAST);

    if (m_sheet != nullptr)
        m_renderer.Draw(gc, *m_sheet, m_view, m_options);
    else
    {
        gc->SetBrush(wxBrush(palette::kCanvasBg));
        gc->SetPen(*wxTRANSPARENT_PEN);
        gc->DrawRectangle(0, 0, m_view.clientSize.x, m_view.clientSize.y);
    }

    delete gc;
}

void CanvasPanel::OnEraseBackground(wxEraseEvent& WXUNUSED(event))
{
    // 交给 wxAutoBufferedPaintDC 整体重绘，避免闪烁
}

// -----------------------------------------------------------------------------
//  鼠标 / 键盘：一律转交事件处理器
// -----------------------------------------------------------------------------
void CanvasPanel::OnMouseLeftDown(wxMouseEvent& event)
{
    if (m_handler != nullptr) m_handler->OnLeftDown(event);
}

void CanvasPanel::OnMouseLeftUp(wxMouseEvent& event)
{
    if (m_handler != nullptr) m_handler->OnLeftUp(event);
}

void CanvasPanel::OnLeftDClick(wxMouseEvent& event)
{
    if (m_handler != nullptr) m_handler->OnLeftDoubleClick(event);
}

void CanvasPanel::OnMouseRightDown(wxMouseEvent& event)
{
    if (m_handler != nullptr) m_handler->OnRightDown(event);
}

void CanvasPanel::OnMouseRightUp(wxMouseEvent& event)
{
    if (m_handler != nullptr) m_handler->OnRightUp(event);
}

void CanvasPanel::OnMouseMiddleDown(wxMouseEvent& event)
{
    if (m_handler != nullptr) m_handler->OnMiddleDown(event);
}

void CanvasPanel::OnMouseMiddleUp(wxMouseEvent& event)
{
    if (m_handler != nullptr) m_handler->OnMiddleUp(event);
}

void CanvasPanel::OnMouseMove(wxMouseEvent& event)
{
    const Point canvasPos = DeviceToCanvas(Point(event.GetX(), event.GetY()));
    UpdateStatusText(canvasPos);

    if (m_handler != nullptr) m_handler->OnMotion(event);
}

void CanvasPanel::OnMouseLeave(wxMouseEvent& event)
{
    ClearHover();
    if (m_handler != nullptr) m_handler->OnLeave(event);
}

void CanvasPanel::OnMouseWheel(wxMouseEvent& event)
{
    const int delta = event.GetWheelDelta() == 0 ? 120 : event.GetWheelDelta();
    const int notches = event.GetWheelRotation() / delta;
    if (notches == 0)
    {
        event.Skip();
        return;
    }

    const Point mouse(event.GetX(), event.GetY());

    if (event.ControlDown())
        ZoomBy(std::pow(1.15, notches), mouse);
    else if (event.ShiftDown())
        PanBy(Point(-notches * 60, 0));
    else
        PanBy(Point(0, -notches * 60));

    const Point canvasPos = DeviceToCanvas(mouse);
    UpdateStatusText(canvasPos);
}

void CanvasPanel::OnKeyDown(wxKeyEvent& event)
{
    // 空格临时平移
    if (event.GetKeyCode() == WXK_SPACE)
    {
        if (m_handler != nullptr) m_handler->SetSpacePanning(true);
        SetCursor(wxCursor(wxCURSOR_HAND));
        return;
    }

    if (m_handler != nullptr && m_handler->OnKeyDown(event)) return;

    event.Skip();
}

void CanvasPanel::OnKeyUp(wxKeyEvent& event)
{
    if (event.GetKeyCode() == WXK_SPACE)
    {
        if (m_handler != nullptr) m_handler->SetSpacePanning(false);
        SetCursor(wxCursor(wxCURSOR_ARROW));
        return;
    }
    event.Skip();
}

void CanvasPanel::OnSize(wxSizeEvent& event)
{
    m_view.clientSize = GetClientSize();
    Refresh(false);
    event.Skip();
}

void CanvasPanel::OnCaptureLost(wxMouseCaptureLostEvent& WXUNUSED(event))
{
    if (m_handler != nullptr) m_handler->OnCaptureLost();
}

}  // namespace sf
