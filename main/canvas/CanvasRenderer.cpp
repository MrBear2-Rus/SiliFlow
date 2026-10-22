#include "CanvasRenderer.h"

#include "core/Types.h"
#include "core/sim/SimEngine.h"

#include <wx/brush.h>
#include <wx/font.h>
#include <wx/graphics.h>
#include <wx/pen.h>

#include <algorithm>
#include <cmath>

namespace sf
{

namespace
{

constexpr int kGridStep = grid::kFine;      // 10
constexpr int kGridMajor = grid::kCoarse;   // 50
constexpr int kMaxGridLines = 600;          // 超过就不画细网格，避免卡顿

/// 画布文字统一走 ui::Font（跨平台字体回退在那一处处理）。
wxFont MakeFont(int pointSize = ui::fontsize::kBody)
{
    return ui::Font(pointSize);
}

}  // namespace

// -----------------------------------------------------------------------------
//  主入口
// -----------------------------------------------------------------------------
void CanvasRenderer::Draw(wxGraphicsContext* gc, const CircuitSheet& sheet, const ViewInfo& view,
                          const RenderOptions& opts) const
{
    if (gc == nullptr) return;

    const wxSize client = view.clientSize;

    // 背景（设备坐标，不受变换影响）
    gc->SetBrush(wxBrush(palette::kCanvasBg));
    gc->SetPen(*wxTRANSPARENT_PEN);
    gc->DrawRectangle(0, 0, client.x, client.y);

    // 进入画布坐标系
    gc->PushState();
    gc->Translate(view.offset.x, view.offset.y);
    gc->Scale(view.scale, view.scale);

    if (opts.showGrid) DrawGrid(gc, view);

    DrawWires(gc, sheet, view, opts);
    DrawComponents(gc, sheet, view, opts);
    DrawAnnotations(gc, sheet);
    DrawOverlays(gc, view, opts);

    gc->PopState();

    // 设备坐标叠层：不随缩放变化，所以在变换之外画。
    DrawChrome(gc, view);
}

// -----------------------------------------------------------------------------
//  设备坐标叠层（原型 .canvas-badge：左下角的电平图例胶囊）
// -----------------------------------------------------------------------------
void CanvasRenderer::DrawChrome(wxGraphicsContext* gc, const ViewInfo& view) const
{
    if (gc == nullptr || view.clientSize.x < 240 || view.clientSize.y < 120) return;

    struct Legend
    {
        wxColour colour;
        const char* label;
    };
    const Legend items[] = { { palette::kLogicZero, "0" },
                             { palette::kLogicOne, "1" },
                             { palette::kLogicX, "X" },
                             { palette::kLogicZ, "Z" } };

    const int padX = 11;
    const int padY = 3;
    const int itemGap = 10;   // .canvas-badge{gap:10px}
    const int dotGap = 5;     // .legend{gap:5px}
    const int dot = 8;        // .legend .dot{width:8px;height:8px}
    const int marginL = 12;
    const int marginB = 10;

    gc->SetFont(ui::Font(ui::fontsize::kCaption), ui::kTertiary);

    double total = padX * 2.0;
    double textW[4] = { 0, 0, 0, 0 };
    double textH = 0;
    for (int i = 0; i < 4; ++i)
    {
        double w = 0, h = 0;
        gc->GetTextExtent(wxString::FromUTF8(items[i].label), &w, &h);
        textW[i] = w;
        textH = std::max(textH, h);
        total += dot + dotGap + w + (i < 3 ? itemGap : 0);
    }

    const double height = std::max<double>(textH + padY * 2.0, dot + 6);
    const double x = marginL;
    const double y = view.clientSize.y - marginB - height;

    gc->SetPen(wxPen(ui::kHairline, 1));
    gc->SetBrush(wxBrush(wxColour(255, 255, 255, 214)));  // surface 84%
    gc->DrawRoundedRectangle(x, y, total, height, height / 2.0);

    double cx = x + padX;
    for (int i = 0; i < 4; ++i)
    {
        gc->SetPen(*wxTRANSPARENT_PEN);
        gc->SetBrush(wxBrush(items[i].colour));
        gc->DrawRoundedRectangle(cx, y + (height - dot) / 2.0, dot, dot, 2);
        cx += dot + dotGap;
        gc->SetFont(ui::Font(ui::fontsize::kCaption), ui::kTertiary);
        gc->DrawText(wxString::FromUTF8(items[i].label), cx, y + (height - textH) / 2.0);
        cx += textW[i] + itemGap;
    }
}

// -----------------------------------------------------------------------------
//  网格
// -----------------------------------------------------------------------------
void CanvasRenderer::DrawGrid(wxGraphicsContext* gc, const ViewInfo& view) const
{
    const Rect vis = view.VisibleCanvasRect();
    gc->SetPen(*wxTRANSPARENT_PEN);
    gc->SetBrush(*wxTRANSPARENT_BRUSH);

    auto drawLines = [&](int step, const wxColour& color)
    {
        const int x0 = static_cast<int>(std::floor(static_cast<double>(vis.x) / step)) * step;
        const int y0 = static_cast<int>(std::floor(static_cast<double>(vis.y) / step)) * step;
        const int nx = vis.width / step + 2;
        const int ny = vis.height / step + 2;
        if (nx + ny > kMaxGridLines) return;

        wxGraphicsPath path = gc->CreatePath();
        for (int i = 0; i < nx; ++i)
        {
            const int x = x0 + i * step;
            path.MoveToPoint(x, vis.y);
            path.AddLineToPoint(x, vis.y + vis.height);
        }
        for (int i = 0; i < ny; ++i)
        {
            const int y = y0 + i * step;
            path.MoveToPoint(vis.x, y);
            path.AddLineToPoint(vis.x + vis.width, y);
        }

        gc->SetPen(wxPen(color, 1));
        gc->StrokePath(path);
    };

    // 细网格：缩得太小时不画
    if (view.scale >= 0.5) drawLines(kGridStep, palette::kGrid);
    // 粗网格始终画
    drawLines(kGridMajor, palette::kGridMajor);
}

// -----------------------------------------------------------------------------
//  导线
// -----------------------------------------------------------------------------
void CanvasRenderer::DrawWires(wxGraphicsContext* gc, const CircuitSheet& sheet, const ViewInfo& view,
                               const RenderOptions& opts) const
{
    const Rect vis = view.VisibleCanvasRect();

    for (const auto& w : sheet.wires)
    {
        if (!vis.Intersects(w->Bounds())) continue;  // 可见区裁剪

        // 电平来源：仿真值优先；没有仿真结果（未求值 / 这根线不在任何网络里）
        // 时退回 wire->signal —— 也就是手工指定的那个值。
        LogicSignal sig = w->signal;
        int bits = 1;

        if (opts.sim != nullptr)
        {
            LogicVector v;
            if (opts.sim->TryWireValue(w->id, v))
            {
                bits = v.Width();
                // 多位总线不按某一位上色：全确定就画成普通导线色，含 X 才标红
                sig = v.AllKnown() ? LogicSignal::Z : LogicSignal::X;
            }
        }

        // 位宽越大画得越粗：1 位用默认，4 位 4px，8 位以上 6px
        const int busPen = (bits > 1) ? (2 + (bits >= 8 ? 4 : (bits >= 4 ? 2 : 1))) : 0;
        const bool hovered = (w->id == opts.hoverWireId);

        w->DrawStyled(gc, hovered, w->selected, opts.drawWireHandles, 3, sig,
                      busPen > 0 ? busPen : w->strokeWidth);

        // 位宽标注（>1 位才画）
        if (bits > 1 && !w->pts.empty())
        {
            const Point mid = w->pts[w->pts.size() / 2].pos;
            gc->SetFont(MakeFont(7), palette::kTextDim);
            gc->DrawText(wxString::Format("%d", bits), mid.x + 3, mid.y - 10);
        }
    }
}

// -----------------------------------------------------------------------------
//  元件
// -----------------------------------------------------------------------------
void CanvasRenderer::DrawComponents(wxGraphicsContext* gc, const CircuitSheet& sheet, const ViewInfo& view,
                                    const RenderOptions& opts) const
{
    const Rect vis = view.VisibleCanvasRect();

    for (const auto& c : sheet.components)
    {
        if (!vis.Intersects(c->Bounds())) continue;
        DrawOneComponent(gc, *c, view, opts, false);
    }

    // 待放置的元件作为预览层
    if (opts.previewComponent != nullptr) DrawOneComponent(gc, *opts.previewComponent, view, opts, true);
}

void CanvasRenderer::DrawOneComponent(wxGraphicsContext* gc, const ComponentInstance& comp,
                                      const ViewInfo& view, const RenderOptions& opts, bool preview) const
{
    const bool hovered = (comp.id == opts.hoverComponentId);

    if (preview) gc->BeginLayer(0.5);

    // 图元：整体平移过去画，避免每帧复制形状
    gc->PushState();
    gc->Translate(comp.pos.x, comp.pos.y);
    for (const Shape& s : comp.xShapes) DrawShape(gc, s);
    gc->PopState();

    // 引脚
    const double pinRadius = 2.5;
    for (int i = 0; i < static_cast<int>(comp.pins.size()); ++i)
    {
        const Pin& pin = comp.pins[i];
        const Point pp(comp.pos.x + pin.pos.x, comp.pos.y + pin.pos.y);

        const bool hot = hovered && (i == opts.hoverPinIndex);
        gc->SetPen(wxPen(hot ? palette::kPinHot : palette::kPin, hot ? 2 : 1));
        gc->SetBrush(wxBrush(hot ? palette::kPinHot : palette::kPin));
        gc->DrawEllipse(pp.x - pinRadius, pp.y - pinRadius, pinRadius * 2, pinRadius * 2);

        if (opts.showPinNames)
        {
            gc->SetFont(MakeFont(7), palette::kTextDim);
            gc->DrawText(pin.identifier, pp.x + 4, pp.y - 10);
        }
    }

    // 指示灯：按仿真值点亮。灯罩是模板里唯一的那个圆，填上它就行。
    if (!preview && opts.sim != nullptr && comp.def != nullptr && comp.def->kind == "led")
    {
        const LogicSignal v = opts.sim->PinSignal(comp.id, 0);

        for (const Shape& s : comp.xShapes)
        {
            const CircleShape* lamp = std::get_if<CircleShape>(&s);
            if (lamp == nullptr) continue;

            const Point cc(comp.pos.x + lamp->center.x, comp.pos.y + lamp->center.y);
            const double r = std::max(1.0, static_cast<double>(lamp->radius) - 1.5);

            gc->SetPen(*wxTRANSPARENT_PEN);
            gc->SetBrush(wxBrush(LogicSignalColour(v)));
            gc->DrawEllipse(cc.x - r, cc.y - r, r * 2, r * 2);
            break;
        }
    }

    // 选中 / 悬停外框
    if (comp.selected || hovered)
    {
        const Rect b = comp.Bounds().Inflate(2, 2);
        wxPen pen(comp.selected ? palette::kSelect : palette::kWireHover, comp.selected ? 2 : 1);
        pen.SetStyle(comp.selected ? wxPENSTYLE_SOLID : wxPENSTYLE_DOT);
        gc->SetPen(pen);
        gc->SetBrush(*wxTRANSPARENT_BRUSH);
        gc->DrawRectangle(b.x, b.y, b.width, b.height);
    }

    // 标签
    if (opts.showLabels && !comp.label.IsEmpty())
    {
        gc->SetFont(MakeFont(10), palette::kText);
        double tw = 0, th = 0;
        gc->GetTextExtent(comp.label, &tw, &th);
        gc->DrawText(comp.label, comp.pos.x + (comp.boxSize.x - tw) / 2.0, comp.pos.y - th - 2);
    }

    if (preview) gc->EndLayer();
}

// -----------------------------------------------------------------------------
//  注释图元
// -----------------------------------------------------------------------------
void CanvasRenderer::DrawAnnotations(wxGraphicsContext* gc, const CircuitSheet& sheet) const
{
    for (const Shape& s : sheet.annotations) DrawShape(gc, s);
}

// -----------------------------------------------------------------------------
//  叠加层：框选 / 框擦 / 橡皮筋 / 绘图预览
// -----------------------------------------------------------------------------
void CanvasRenderer::DrawOverlays(wxGraphicsContext* gc, const ViewInfo& view, const RenderOptions& opts) const
{
    gc->SetBrush(*wxTRANSPARENT_BRUSH);

    if (opts.selectionRect != nullptr && opts.selectionRect->width > 0 && opts.selectionRect->height > 0)
    {
        const Rect& r = *opts.selectionRect;
        wxPen pen(palette::kSelect, 1);
        pen.SetStyle(wxPENSTYLE_SHORT_DASH);
        gc->SetPen(pen);
        gc->SetBrush(wxBrush(wxColour(palette::kSelect.Red(), palette::kSelect.Green(), palette::kSelect.Blue(), 28)));
        gc->DrawRectangle(r.x, r.y, r.width, r.height);
        gc->SetBrush(*wxTRANSPARENT_BRUSH);
    }

    if (opts.eraserRect != nullptr && opts.eraserRect->width > 0 && opts.eraserRect->height > 0)
    {
        const Rect& r = *opts.eraserRect;
        wxPen pen(palette::kLogicOne, 1);
        pen.SetStyle(wxPENSTYLE_SHORT_DASH);
        gc->SetPen(pen);
        gc->DrawRectangle(r.x, r.y, r.width, r.height);
    }

    if (opts.rubberBand != nullptr && opts.rubberBand->size() >= 2)
    {
        wxPen pen(palette::kWireSelect, 2);
        pen.SetStyle(wxPENSTYLE_SHORT_DASH);
        gc->SetPen(pen);

        wxGraphicsPath path = gc->CreatePath();
        path.MoveToPoint((*opts.rubberBand)[0].x, (*opts.rubberBand)[0].y);
        for (size_t i = 1; i < opts.rubberBand->size(); ++i)
            path.AddLineToPoint((*opts.rubberBand)[i].x, (*opts.rubberBand)[i].y);
        gc->StrokePath(path);
    }

    if (opts.previewShape != nullptr) DrawShape(gc, *opts.previewShape);
}

}  // namespace sf
