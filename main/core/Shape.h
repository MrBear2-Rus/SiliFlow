#pragma once

// =============================================================================
//  SiliFlow · core/Shape.h
//  图元：元件外观、装饰图形、注释文字统一用这一套 variant 表达。
//  刻意不支持 SVG path（SigFlow 用 nanosvg 解析），改用「折线 + 贝塞尔」组合
//  已足够覆盖全部内置元件外观，且省掉一个大三方库。
// =============================================================================

#include "Json.h"
#include "Types.h"

#include <variant>
#include <vector>

class wxGraphicsContext;

namespace sf
{

// -----------------------------------------------------------------------------
//  七种图元
// -----------------------------------------------------------------------------
struct LineShape
{
    Point a, b;
    wxColour color;
    int width = 1;
};

struct RectShape
{
    Rect rect;
    wxColour color;
    wxColour fillColor;
    int width = 1;
    bool fill = false;
};

struct CircleShape
{
    Point center;
    int radius = 1;
    wxColour color;
    wxColour fillColor;
    int width = 1;
    bool fill = false;
};

/// 角度用「数学约定」：0° 指向右，90° 指向上（视觉），逆时针为正。
/// 绘制时按 x = cx + r·cosθ, y = cy − r·sinθ 采样折线，避免 wx 的弧线方向歧义。
struct ArcShape
{
    Point center;
    int radius = 1;
    double startAngle = 0.0;
    double endAngle = 0.0;
    wxColour color;
    int width = 1;
};

struct BezierShape
{
    Point p0, p1, p2, p3;
    wxColour color;
    int width = 1;
};

struct PolyShape
{
    std::vector<Point> pts;
    wxColour color;
    wxColour fillColor;
    int width = 1;
    bool closed = false;
    bool fill = false;
};

struct TextShape
{
    Point pos;
    wxString text;
    int pointSize = 10;
    wxColour color;
    bool centered = false;
};

using Shape = std::variant<LineShape, RectShape, CircleShape, ArcShape, BezierShape, PolyShape, TextShape>;

// -----------------------------------------------------------------------------
//  统一操作接口（对全部 7 种图元都成立）
// -----------------------------------------------------------------------------
void DrawShape(wxGraphicsContext* gc, const Shape& shape);
Rect ShapeBounds(const Shape& shape);
void TranslateShape(Shape& shape, int dx, int dy);
void RotateShape90(Shape& shape, int quarters);
void MirrorShapeX(Shape& shape);

wxString ShapeTypeName(const Shape& shape);
Json ShapeToJson(const Shape& shape);
Shape ShapeFromJson(const Json& j);

// -----------------------------------------------------------------------------
//  构造助手：给内置元件库用，省去每次手写颜色
// -----------------------------------------------------------------------------
Shape MakeLine(Point a, Point b, const wxColour& color = palette::kComponentStroke, int width = 1);
Shape MakeRect(const Rect& r, const wxColour& color = palette::kComponentStroke, int width = 1);
Shape MakeFilledRect(const Rect& r, const wxColour& fillColor, const wxColour& color = palette::kComponentStroke,
                     int width = 1);
Shape MakeCircle(Point center, int radius, const wxColour& color = palette::kComponentStroke, int width = 1);
Shape MakeFilledCircle(Point center, int radius, const wxColour& fillColor,
                       const wxColour& color = palette::kComponentStroke, int width = 1);
Shape MakeArc(Point center, int radius, double startDeg, double endDeg,
              const wxColour& color = palette::kComponentStroke, int width = 1);
Shape MakeBezier(Point p0, Point p1, Point p2, Point p3,
                 const wxColour& color = palette::kComponentStroke, int width = 1);
Shape MakePoly(std::vector<Point> pts, bool closed, const wxColour& color = palette::kComponentStroke, int width = 1);
Shape MakeFilledPoly(std::vector<Point> pts, const wxColour& fillColor,
                     const wxColour& color = palette::kComponentStroke, int width = 1);
Shape MakeText(Point pos, const wxString& text, int pointSize = 10,
               const wxColour& color = palette::kText, bool centered = false);

}  // namespace sf
