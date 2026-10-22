#pragma once

// =============================================================================
//  SiliFlow · canvas/CanvasRenderer.h
//  只负责「画什么、按什么顺序画」，不处理任何输入事件、也不改模型。
//
//  绘制顺序（从下到上）：
//    背景 → 网格 → 导线 → 元件 → 标签 → 注释 → 选中高亮 → 预览层 → 悬停提示
// =============================================================================

#include "core/Circuit.h"
#include "core/Shape.h"
#include "core/Types.h"

#include <wx/gdicmn.h>

class wxGraphicsContext;

namespace sf
{

namespace sim
{
class SimEngine;
}

// -----------------------------------------------------------------------------
//  视图变换
// -----------------------------------------------------------------------------
//  device = canvas * scale + offset
//  这是全工程**唯一**的坐标换算实现；CanvasPanel 的 DeviceToCanvas /
//  CanvasToDevice 只是转发到这里，其他任何地方都不得自己算。
// -----------------------------------------------------------------------------
struct ViewInfo
{
    double scale = 1.0;
    Point offset = Point(0, 0);
    wxSize clientSize = wxSize(0, 0);

    Point CanvasToDevice(const Point& p) const
    {
        return Point(static_cast<int>(std::lround(p.x * scale)) + offset.x,
                     static_cast<int>(std::lround(p.y * scale)) + offset.y);
    }

    Point DeviceToCanvas(const Point& p) const
    {
        const double s = (scale <= 0.0) ? 1.0 : scale;
        return Point(static_cast<int>(std::lround((p.x - offset.x) / s)),
                     static_cast<int>(std::lround((p.y - offset.y) / s)));
    }

    Rect CanvasToDevice(const Rect& r) const
    {
        const Point tl = CanvasToDevice(Point(r.x, r.y));
        const Point br = CanvasToDevice(Point(r.x + r.width, r.y + r.height));
        return NormalizeRect(tl, br);
    }

    /// 当前视口对应的画布范围（用于可见区裁剪）
    Rect VisibleCanvasRect() const
    {
        return Rect(DeviceToCanvas(Point(0, 0)), DeviceToCanvas(Point(clientSize.x, clientSize.y)));
    }
};

// -----------------------------------------------------------------------------
//  绘制选项
// -----------------------------------------------------------------------------
struct RenderOptions
{
    bool showGrid = true;
    bool showPinNames = false;
    bool showLabels = true;
    bool drawWireHandles = false;

    // ---- 预览层（跟随鼠标的半透明内容）----
    const ComponentInstance* previewComponent = nullptr;  ///< 待放置元件（画布坐标）
    const std::vector<Point>* rubberBand = nullptr;       ///< 拉线预览折线
    const Rect* selectionRect = nullptr;                  ///< 框选矩形
    const Rect* eraserRect = nullptr;                      ///< 框擦矩形
    const Shape* previewShape = nullptr;                  ///< 绘图工具预览

    // ---- 悬停 ----
    int hoverComponentId = -1;
    int hoverPinIndex = -1;
    int hoverWireId = -1;

    /// 仿真引擎。非空且对某根导线有结果时，导线颜色用仿真值，
    /// 否则退回 `Wire::signal`（手工指定的那个）。
    const sim::SimEngine* sim = nullptr;
};

// -----------------------------------------------------------------------------
//  渲染器
// -----------------------------------------------------------------------------
class CanvasRenderer
{
public:
    void Draw(wxGraphicsContext* gc, const CircuitSheet& sheet, const ViewInfo& view,
              const RenderOptions& opts) const;

private:
    void DrawGrid(wxGraphicsContext* gc, const ViewInfo& view) const;
    void DrawWires(wxGraphicsContext* gc, const CircuitSheet& sheet, const ViewInfo& view,
                   const RenderOptions& opts) const;
    void DrawComponents(wxGraphicsContext* gc, const CircuitSheet& sheet, const ViewInfo& view,
                        const RenderOptions& opts) const;
    void DrawOneComponent(wxGraphicsContext* gc, const ComponentInstance& comp, const ViewInfo& view,
                          const RenderOptions& opts, bool preview) const;
    void DrawAnnotations(wxGraphicsContext* gc, const CircuitSheet& sheet) const;
    void DrawOverlays(wxGraphicsContext* gc, const ViewInfo& view, const RenderOptions& opts) const;
    /// 设备坐标叠层（不随缩放变化）：信号图例胶囊。必须在 PopState 之后调用。
    void DrawChrome(wxGraphicsContext* gc, const ViewInfo& view) const;
};

}  // namespace sf
