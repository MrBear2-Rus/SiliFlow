#include "Shape.h"

#include <wx/brush.h>
#include <wx/graphics.h>
#include <wx/pen.h>

#include <cmath>
#include <type_traits>

namespace sf
{
namespace
{

/// 把弧按 6° 一段采样成折线。用采样而不是原生 DrawArc，是为了让角度方向
/// 完全由我们控制（wx 的弧线方向在 y 轴向下的坐标系里容易搞反）。
void AppendArcPoints(std::vector<Point>& out, const ArcShape& arc)
{
    if (arc.radius <= 0) return;

    const double sweep = arc.endAngle - arc.startAngle;
    if (std::abs(sweep) < 1e-9) return;

    const int steps = std::max(8, static_cast<int>(std::ceil(std::abs(sweep) / 6.0)));
    out.reserve(out.size() + static_cast<size_t>(steps) + 1);

    for (int i = 0; i <= steps; ++i)
    {
        const double deg = arc.startAngle + sweep * i / steps;
        const double rad = Deg2Rad(deg);
        out.push_back(Point(arc.center.x + static_cast<int>(std::lround(arc.radius * std::cos(rad))),
                            arc.center.y - static_cast<int>(std::lround(arc.radius * std::sin(rad)))));
    }
}

std::vector<Point> ArcPolyline(const ArcShape& arc)
{
    std::vector<Point> pts;
    AppendArcPoints(pts, arc);
    return pts;
}

Rect BoundsOfPoints(const std::vector<Point>& pts)
{
    if (pts.empty()) return Rect(0, 0, 0, 0);

    int minX = pts[0].x, maxX = pts[0].x;
    int minY = pts[0].y, maxY = pts[0].y;
    for (const Point& p : pts)
    {
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
    }
    return Rect(minX, minY, maxX - minX, maxY - minY);
}

/// 把角度规约到 (-180, 180]，避免反复旋转后数值漂移。
double NormalizeAngle(double deg)
{
    while (deg > 180.0) deg -= 360.0;
    while (deg <= -180.0) deg += 360.0;
    return deg;
}

/// 设置描边（线宽至少 1，颜色无效时退回黑色）
void ApplyStroke(wxGraphicsContext* gc, const wxColour& color, int width)
{
    wxPen pen(color.IsOk() ? color : *wxBLACK, std::max(1, width));
    pen.SetCap(wxCAP_ROUND);
    pen.SetJoin(wxJOIN_ROUND);
    gc->SetPen(pen);
}

void ApplyFill(wxGraphicsContext* gc, const wxColour& color)
{
    if (color.IsOk() && color.Alpha() > 0)
        gc->SetBrush(wxBrush(color));
    else
        gc->SetBrush(*wxTRANSPARENT_BRUSH);
}

}  // namespace

// -----------------------------------------------------------------------------
//  绘制
// -----------------------------------------------------------------------------
void DrawShape(wxGraphicsContext* gc, const Shape& shape)
{
    if (gc == nullptr) return;

    std::visit(
        [gc](const auto& s)
        {
            using T = std::decay_t<decltype(s)>;

            if constexpr (std::is_same_v<T, LineShape>)
            {
                ApplyStroke(gc, s.color, s.width);
                gc->StrokeLine(s.a.x, s.a.y, s.b.x, s.b.y);
            }
            else if constexpr (std::is_same_v<T, RectShape>)
            {
                ApplyFill(gc, s.fill ? s.fillColor : wxColour());
                ApplyStroke(gc, s.color, s.width);
                gc->DrawRectangle(s.rect.x, s.rect.y, s.rect.width, s.rect.height);
            }
            else if constexpr (std::is_same_v<T, CircleShape>)
            {
                ApplyFill(gc, s.fill ? s.fillColor : wxColour());
                ApplyStroke(gc, s.color, s.width);
                gc->DrawEllipse(s.center.x - s.radius, s.center.y - s.radius, s.radius * 2, s.radius * 2);
            }
            else if constexpr (std::is_same_v<T, ArcShape>)
            {
                const std::vector<Point> pts = ArcPolyline(s);
                if (pts.size() < 2) return;

                wxGraphicsPath path = gc->CreatePath();
                path.MoveToPoint(pts[0].x, pts[0].y);
                for (size_t i = 1; i < pts.size(); ++i) path.AddLineToPoint(pts[i].x, pts[i].y);

                gc->SetBrush(*wxTRANSPARENT_BRUSH);
                ApplyStroke(gc, s.color, s.width);
                gc->StrokePath(path);
            }
            else if constexpr (std::is_same_v<T, BezierShape>)
            {
                wxGraphicsPath path = gc->CreatePath();
                path.MoveToPoint(s.p0.x, s.p0.y);
                path.AddCurveToPoint(s.p1.x, s.p1.y, s.p2.x, s.p2.y, s.p3.x, s.p3.y);

                gc->SetBrush(*wxTRANSPARENT_BRUSH);
                ApplyStroke(gc, s.color, s.width);
                gc->StrokePath(path);
            }
            else if constexpr (std::is_same_v<T, PolyShape>)
            {
                if (s.pts.size() < 2) return;

                wxGraphicsPath path = gc->CreatePath();
                path.MoveToPoint(s.pts[0].x, s.pts[0].y);
                for (size_t i = 1; i < s.pts.size(); ++i) path.AddLineToPoint(s.pts[i].x, s.pts[i].y);
                if (s.closed) path.CloseSubpath();

                ApplyFill(gc, s.fill ? s.fillColor : wxColour());
                ApplyStroke(gc, s.color, s.width);
                if (s.fill && s.closed)
                    gc->DrawPath(path);
                else
                {
                    gc->SetBrush(*wxTRANSPARENT_BRUSH);
                    gc->StrokePath(path);
                }
            }
            else if constexpr (std::is_same_v<T, TextShape>)
            {
                wxFont font = ui::MonoFont(std::max(6, s.pointSize));
                gc->SetFont(font, s.color.IsOk() ? s.color : palette::kText);

                double x = s.pos.x;
                double y = s.pos.y;
                if (s.centered)
                {
                    wxDouble tw = 0, th = 0, descent = 0, leading = 0;
                    gc->GetTextExtent(s.text, &tw, &th, &descent, &leading);
                    x -= tw / 2.0;
                    y -= th / 2.0;
                }
                gc->DrawText(s.text, x, y);
            }
        },
        shape);
}

// -----------------------------------------------------------------------------
//  包围盒
// -----------------------------------------------------------------------------
Rect ShapeBounds(const Shape& shape)
{
    return std::visit(
        [](const auto& s) -> Rect
        {
            using T = std::decay_t<decltype(s)>;

            if constexpr (std::is_same_v<T, LineShape>)
                return NormalizeRect(s.a, s.b).Inflate(std::max(1, s.width), std::max(1, s.width));
            else if constexpr (std::is_same_v<T, RectShape>)
                return s.rect.Inflate(std::max(1, s.width), std::max(1, s.width));
            else if constexpr (std::is_same_v<T, CircleShape>)
                return Rect(s.center.x - s.radius, s.center.y - s.radius, s.radius * 2, s.radius * 2)
                    .Inflate(std::max(1, s.width), std::max(1, s.width));
            else if constexpr (std::is_same_v<T, ArcShape>)
                return BoundsOfPoints(ArcPolyline(s)).Inflate(std::max(1, s.width), std::max(1, s.width));
            else if constexpr (std::is_same_v<T, BezierShape>)
            {
                std::vector<Point> pts{s.p0, s.p1, s.p2, s.p3};
                return BoundsOfPoints(pts).Inflate(std::max(1, s.width), std::max(1, s.width));
            }
            else if constexpr (std::is_same_v<T, PolyShape>)
                return BoundsOfPoints(s.pts).Inflate(std::max(1, s.width), std::max(1, s.width));
            else if constexpr (std::is_same_v<T, TextShape>)
            {
                // 不依赖 GC 度量：按字号估算，误差在可接受范围（仅用于合并包围盒）
                const int h = static_cast<int>(s.pointSize * 1.6);
                const int w = static_cast<int>(s.text.length() * s.pointSize * 0.62);
                const int x = s.centered ? s.pos.x - w / 2 : s.pos.x;
                const int y = s.centered ? s.pos.y - h / 2 : s.pos.y;
                return Rect(x, y, w, h);
            }
            return Rect(0, 0, 0, 0);
        },
        shape);
}

// -----------------------------------------------------------------------------
//  变换
// -----------------------------------------------------------------------------
void TranslateShape(Shape& shape, int dx, int dy)
{
    std::visit(
        [dx, dy](auto& s)
        {
            using T = std::decay_t<decltype(s)>;

            if constexpr (std::is_same_v<T, LineShape>)
            {
                s.a = Point(s.a.x + dx, s.a.y + dy);
                s.b = Point(s.b.x + dx, s.b.y + dy);
            }
            else if constexpr (std::is_same_v<T, RectShape>)
                s.rect.Offset(dx, dy);
            else if constexpr (std::is_same_v<T, CircleShape> || std::is_same_v<T, ArcShape>)
                s.center = Point(s.center.x + dx, s.center.y + dy);
            else if constexpr (std::is_same_v<T, BezierShape>)
            {
                s.p0 = Point(s.p0.x + dx, s.p0.y + dy);
                s.p1 = Point(s.p1.x + dx, s.p1.y + dy);
                s.p2 = Point(s.p2.x + dx, s.p2.y + dy);
                s.p3 = Point(s.p3.x + dx, s.p3.y + dy);
            }
            else if constexpr (std::is_same_v<T, PolyShape>)
                for (Point& p : s.pts) p = Point(p.x + dx, p.y + dy);
            else if constexpr (std::is_same_v<T, TextShape>)
                s.pos = Point(s.pos.x + dx, s.pos.y + dy);
        },
        shape);
}

void RotateShape90(Shape& shape, int quarters)
{
    const int q = ((quarters % 4) + 4) % 4;
    if (q == 0) return;

    std::visit(
        [q](auto& s)
        {
            using T = std::decay_t<decltype(s)>;

            if constexpr (std::is_same_v<T, LineShape>)
            {
                s.a = RotatePoint90(s.a, q);
                s.b = RotatePoint90(s.b, q);
            }
            else if constexpr (std::is_same_v<T, RectShape>)
            {
                // 轴对齐矩形旋转 90° 仍是轴对齐矩形：转对角两点再归一化。
                // 注意用「开区间」右下角（x+w, y+h）：GetBottomRight() 是含右下像素的，
                // 直接喂给 NormalizeRect 每转一次都会缩 1px。
                const Point tl = RotatePoint90(s.rect.GetTopLeft(), q);
                const Point br = RotatePoint90(Point(s.rect.x + s.rect.width, s.rect.y + s.rect.height), q);
                s.rect = NormalizeRect(tl, br);
            }
            else if constexpr (std::is_same_v<T, CircleShape>)
                s.center = RotatePoint90(s.center, q);
            else if constexpr (std::is_same_v<T, ArcShape>)
            {
                // 屏幕 y 向下的坐标系里，视觉顺时针 90° 对应角度减小 90°
                s.center = RotatePoint90(s.center, q);
                s.startAngle = NormalizeAngle(s.startAngle - 90.0 * q);
                s.endAngle = NormalizeAngle(s.endAngle - 90.0 * q);
            }
            else if constexpr (std::is_same_v<T, BezierShape>)
            {
                s.p0 = RotatePoint90(s.p0, q);
                s.p1 = RotatePoint90(s.p1, q);
                s.p2 = RotatePoint90(s.p2, q);
                s.p3 = RotatePoint90(s.p3, q);
            }
            else if constexpr (std::is_same_v<T, PolyShape>)
                for (Point& p : s.pts) p = RotatePoint90(p, q);
            else if constexpr (std::is_same_v<T, TextShape>)
                s.pos = RotatePoint90(s.pos, q);  // 字形不倒，只挪位置
        },
        shape);
}

void MirrorShapeX(Shape& shape)
{
    std::visit(
        [](auto& s)
        {
            using T = std::decay_t<decltype(s)>;

            if constexpr (std::is_same_v<T, LineShape>)
            {
                s.a = MirrorPointX(s.a);
                s.b = MirrorPointX(s.b);
            }
            else if constexpr (std::is_same_v<T, RectShape>)
            {
                const Point tl = MirrorPointX(s.rect.GetTopLeft());
                const Point br = MirrorPointX(Point(s.rect.x + s.rect.width, s.rect.y + s.rect.height));
                s.rect = NormalizeRect(tl, br);
            }
            else if constexpr (std::is_same_v<T, CircleShape>)
                s.center = MirrorPointX(s.center);
            else if constexpr (std::is_same_v<T, ArcShape>)
            {
                // 关于 y 轴镜像：θ -> 180° − θ，并交换首尾以保持扫掠方向
                s.center = MirrorPointX(s.center);
                const double a = NormalizeAngle(180.0 - s.startAngle);
                const double b = NormalizeAngle(180.0 - s.endAngle);
                s.startAngle = b;
                s.endAngle = a;
            }
            else if constexpr (std::is_same_v<T, BezierShape>)
            {
                s.p0 = MirrorPointX(s.p0);
                s.p1 = MirrorPointX(s.p1);
                s.p2 = MirrorPointX(s.p2);
                s.p3 = MirrorPointX(s.p3);
            }
            else if constexpr (std::is_same_v<T, PolyShape>)
                for (Point& p : s.pts) p = MirrorPointX(p);
            else if constexpr (std::is_same_v<T, TextShape>)
                s.pos = MirrorPointX(s.pos);
        },
        shape);
}

// -----------------------------------------------------------------------------
//  序列化
// -----------------------------------------------------------------------------
wxString ShapeTypeName(const Shape& shape)
{
    return std::visit(
        [](const auto& s) -> wxString
        {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, LineShape>) return "line";
            else if constexpr (std::is_same_v<T, RectShape>) return "rect";
            else if constexpr (std::is_same_v<T, CircleShape>) return "circle";
            else if constexpr (std::is_same_v<T, ArcShape>) return "arc";
            else if constexpr (std::is_same_v<T, BezierShape>) return "bezier";
            else if constexpr (std::is_same_v<T, PolyShape>) return "poly";
            else return "text";
        },
        shape);
}

Json ShapeToJson(const Shape& shape)
{
    return std::visit(
        [](const auto& s) -> Json
        {
            using T = std::decay_t<decltype(s)>;
            Json j;

            if constexpr (std::is_same_v<T, LineShape>)
            {
                j = Json{{"type", "line"}};
                PutPoint(j, "start", s.a);
                PutPoint(j, "end", s.b);
                j["color"] = ColourToHex(s.color);
                j["width"] = s.width;
            }
            else if constexpr (std::is_same_v<T, RectShape>)
            {
                j = Json{{"type", "rect"}};
                PutRect(j, "rect", s.rect);
                j["color"] = ColourToHex(s.color);
                j["fill"] = s.fill;
                if (s.fill) j["fillColor"] = ColourToHex(s.fillColor);
                j["width"] = s.width;
            }
            else if constexpr (std::is_same_v<T, CircleShape>)
            {
                j = Json{{"type", "circle"}};
                j["x"] = s.center.x;
                j["y"] = s.center.y;
                j["r"] = s.radius;
                j["color"] = ColourToHex(s.color);
                j["fill"] = s.fill;
                if (s.fill) j["fillColor"] = ColourToHex(s.fillColor);
                j["width"] = s.width;
            }
            else if constexpr (std::is_same_v<T, ArcShape>)
            {
                j = Json{{"type", "arc"}};
                PutPoint(j, "center", s.center);
                j["radius"] = s.radius;
                j["startAngle"] = s.startAngle;
                j["endAngle"] = s.endAngle;
                j["color"] = ColourToHex(s.color);
                j["width"] = s.width;
            }
            else if constexpr (std::is_same_v<T, BezierShape>)
            {
                j = Json{{"type", "bezier"}};
                PutPoint(j, "p0", s.p0);
                PutPoint(j, "p1", s.p1);
                PutPoint(j, "p2", s.p2);
                PutPoint(j, "p3", s.p3);
                j["color"] = ColourToHex(s.color);
                j["width"] = s.width;
            }
            else if constexpr (std::is_same_v<T, PolyShape>)
            {
                j = Json{{"type", "poly"}};
                Json pts = Json::array();
                for (const Point& p : s.pts) pts.push_back(Json{{"x", p.x}, {"y", p.y}});
                j["points"] = pts;
                j["color"] = ColourToHex(s.color);
                j["fill"] = s.fill;
                if (s.fill) j["fillColor"] = ColourToHex(s.fillColor);
                j["closed"] = s.closed;
                j["width"] = s.width;
            }
            else if constexpr (std::is_same_v<T, TextShape>)
            {
                j = Json{{"type", "text"}};
                PutPoint(j, "pos", s.pos);
                j["text"] = ToUtf8(s.text);
                j["pointSize"] = s.pointSize;
                j["color"] = ColourToHex(s.color);
                j["centered"] = s.centered;
            }
            return j;
        },
        shape);
}

Shape ShapeFromJson(const Json& j)
{
    const wxString type = GetStr(j, "type");

    if (type == "line")
        return MakeLine(GetPoint(j, "start"), GetPoint(j, "end"), GetColour(j, "color", palette::kComponentStroke),
                        GetInt(j, "width", 1));

    if (type == "rect")
    {
        RectShape s;
        s.rect = GetRect(j, "rect");
        s.color = GetColour(j, "color", palette::kComponentStroke);
        s.fill = GetBool(j, "fill", false);
        s.fillColor = GetColour(j, "fillColor", *wxWHITE);
        s.width = GetInt(j, "width", 1);
        return s;
    }

    if (type == "circle")
    {
        CircleShape s;
        s.center = Point(GetInt(j, "x", 0), GetInt(j, "y", 0));
        s.radius = GetInt(j, "r", GetInt(j, "radius", 1));
        s.color = GetColour(j, "color", palette::kComponentStroke);
        s.fill = GetBool(j, "fill", false);
        s.fillColor = GetColour(j, "fillColor", *wxWHITE);
        s.width = GetInt(j, "width", 1);
        return s;
    }

    if (type == "arc")
        return MakeArc(GetPoint(j, "center"), GetInt(j, "radius", 1), GetDouble(j, "startAngle", 0.0),
                       GetDouble(j, "endAngle", 0.0), GetColour(j, "color", palette::kComponentStroke),
                       GetInt(j, "width", 1));

    if (type == "bezier")
    {
        BezierShape s;
        s.p0 = GetPoint(j, "p0");
        s.p1 = GetPoint(j, "p1");
        s.p2 = GetPoint(j, "p2");
        s.p3 = GetPoint(j, "p3");
        s.color = GetColour(j, "color", palette::kComponentStroke);
        s.width = GetInt(j, "width", 1);
        return s;
    }

    if (type == "poly")
    {
        PolyShape s;
        if (Has(j, "points") && j["points"].is_array())
            for (const Json& p : j["points"])
                s.pts.push_back(Point(GetInt(p, "x", 0), GetInt(p, "y", 0)));
        s.color = GetColour(j, "color", palette::kComponentStroke);
        s.fill = GetBool(j, "fill", false);
        s.fillColor = GetColour(j, "fillColor", *wxWHITE);
        s.closed = GetBool(j, "closed", true);
        s.width = GetInt(j, "width", 1);
        return s;
    }

    // 兜底当文本处理，保证未知类型不丢数据也不崩
    TextShape s;
    s.pos = GetPoint(j, "pos");
    s.text = GetStr(j, "text");
    s.pointSize = GetInt(j, "pointSize", 10);
    s.color = GetColour(j, "color", palette::kText);
    s.centered = GetBool(j, "centered", false);
    return s;
}

// -----------------------------------------------------------------------------
//  构造助手
// -----------------------------------------------------------------------------
Shape MakeLine(Point a, Point b, const wxColour& color, int width)
{
    return LineShape{a, b, color, width};
}

Shape MakeRect(const Rect& r, const wxColour& color, int width)
{
    return RectShape{r, color, *wxWHITE, width, false};
}

Shape MakeFilledRect(const Rect& r, const wxColour& fillColor, const wxColour& color, int width)
{
    return RectShape{r, color, fillColor, width, true};
}

Shape MakeCircle(Point center, int radius, const wxColour& color, int width)
{
    return CircleShape{center, radius, color, *wxWHITE, width, false};
}

Shape MakeFilledCircle(Point center, int radius, const wxColour& fillColor, const wxColour& color, int width)
{
    return CircleShape{center, radius, color, fillColor, width, true};
}

Shape MakeArc(Point center, int radius, double startDeg, double endDeg, const wxColour& color, int width)
{
    return ArcShape{center, radius, startDeg, endDeg, color, width};
}

Shape MakeBezier(Point p0, Point p1, Point p2, Point p3, const wxColour& color, int width)
{
    return BezierShape{p0, p1, p2, p3, color, width};
}

Shape MakePoly(std::vector<Point> pts, bool closed, const wxColour& color, int width)
{
    return PolyShape{std::move(pts), color, *wxWHITE, width, closed, false};
}

Shape MakeFilledPoly(std::vector<Point> pts, const wxColour& fillColor, const wxColour& color, int width)
{
    return PolyShape{std::move(pts), color, fillColor, width, true, true};
}

Shape MakeText(Point pos, const wxString& text, int pointSize, const wxColour& color, bool centered)
{
    return TextShape{pos, text, pointSize, color, centered};
}

}  // namespace sf
