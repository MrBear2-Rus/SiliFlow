#pragma once

// =============================================================================
//  SiliFlow · canvas/CanvasPanel.h
//  画布窗口。职责严格限定为：
//    窗口/滚动/缩放/重绘调度 · 坐标系转换 · 持有图纸与选中集
//
//  明确不负责：
//    具体绘制细节（交 CanvasRenderer）
//    手势语义（交 CanvasEventHandler）
//    工具状态（交 ToolStateMachine）
//    模型序列化（交 core）
// =============================================================================

#include "CanvasRenderer.h"
#include "ToolStateMachine.h"
#include "core/Circuit.h"
#include "core/Services.h"
#include "core/Types.h"

#include <wx/panel.h>

#include <algorithm>
#include <memory>

class wxMouseEvent;
class wxKeyEvent;
class wxPaintEvent;
class wxSizeEvent;
class wxMouseWheelEvent;
class wxEraseEvent;
class wxMouseCaptureLostEvent;

namespace sf
{

class CanvasEventHandler;

class CanvasPanel : public wxPanel
{
public:
    CanvasPanel(wxWindow* parent, Services* services, wxWindowID id = wxID_ANY);
    ~CanvasPanel() override;

    // ---- 图纸 ----
    void SetSheet(CircuitSheet* sheet);
    CircuitSheet* Sheet() const { return m_sheet; }

    // ---- 服务与工具 ----
    Services* GetServices() const { return m_services; }
    ToolStateMachine& Tools() { return m_tools; }
    const ToolStateMachine& Tools() const { return m_tools; }
    CanvasEventHandler* Handler() const { return m_handler.get(); }

    // ---- 视图 ----
    const ViewInfo& View() const { return m_view; }
    double Scale() const { return m_view.scale; }
    void SetScale(double scale) { m_view.scale = std::clamp(scale, kMinScale, kMaxScale); }
    Point Offset() const { return m_view.offset; }

    static constexpr double kMinScale = 0.25;
    static constexpr double kMaxScale = 4.0;

    /// 全工程唯一的两个坐标换算入口（实际实现转发到 ViewInfo）
    Point DeviceToCanvas(const Point& p) const { return m_view.DeviceToCanvas(p); }
    Point CanvasToDevice(const Point& p) const { return m_view.CanvasToDevice(p); }

    void ZoomAt(double newScale, const Point& deviceAnchor);
    void ZoomBy(double factor, const Point& deviceAnchor);
    void ZoomFit();
    void ZoomReset();
    void PanBy(const Point& deviceDelta);
    void CenterOn(const Point& canvasPoint);

    // ---- 网格吸附 ----
    bool SnapEnabled() const { return m_snapEnabled; }
    void SetSnapEnabled(bool on) { m_snapEnabled = on; }
    /// 按当前吸附设置处理落点（Shift 临时关闭由事件处理器传 false）
    Point Snap(const Point& canvasPoint, bool forceOff = false) const;

    // ---- 悬停 ----
    void SetHover(int compId, int pinIndex, int wireId);
    void ClearHover();
    int HoverComponentId() const { return m_options.hoverComponentId; }
    int HoverPinIndex() const { return m_options.hoverPinIndex; }
    int HoverWireId() const { return m_options.hoverWireId; }

    // ---- 绘制选项（预览层由事件处理器写入）----
    RenderOptions& Options() { return m_options; }
    const RenderOptions& Options() const { return m_options; }

    // ---- 状态广播 ----
    /// 图纸内容变了：刷新画布 + 通知树面板/标题栏
    void NotifyContentChanged();
    /// 选中集变了
    void NotifySelectionChanged();
    /// 更新状态栏（光标 + 缩放）
    void UpdateStatusText(const Point& canvasPos);

    /// 上下文档位（撤销/重做）发生变化
    void NotifyUndoStateChanged();

    /// 让画布获得键盘焦点（右键菜单等场景）
    void FocusCanvas() { SetFocus(); }

    // ---- 事件（由 CanvasEventHandler 调用，也可从外部手动触发）----
    void OnPaint(wxPaintEvent& event);
    void OnEraseBackground(wxEraseEvent& event);
    void OnMouseLeftDown(wxMouseEvent& event);
    void OnMouseLeftUp(wxMouseEvent& event);
    void OnMouseRightDown(wxMouseEvent& event);
    void OnMouseRightUp(wxMouseEvent& event);
    void OnMouseMiddleDown(wxMouseEvent& event);
    void OnMouseMiddleUp(wxMouseEvent& event);
    void OnMouseMove(wxMouseEvent& event);
    void OnMouseLeave(wxMouseEvent& event);
    void OnMouseWheel(wxMouseEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    void OnKeyUp(wxKeyEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnLeftDClick(wxMouseEvent& event);
    void OnCaptureLost(wxMouseCaptureLostEvent& event);

private:
    void BindEvents();

    Services* m_services = nullptr;
    CircuitSheet* m_sheet = nullptr;

    ViewInfo m_view;
    RenderOptions m_options;
    ToolStateMachine m_tools;
    CanvasRenderer m_renderer;

    std::unique_ptr<CanvasEventHandler> m_handler;

    bool m_snapEnabled = true;
};

}  // namespace sf
