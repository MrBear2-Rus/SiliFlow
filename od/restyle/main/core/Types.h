#pragma once

// =============================================================================
//  SiliFlow · core/Types.h
//  基础值类型：几何别名、数学助手、调色板。
//  本文件是 L2 层最底层，不依赖任何其他 core 头文件，也不依赖任何 wx 控件。
// =============================================================================

#include <wx/colour.h>
#include <wx/font.h>
#include <wx/gdicmn.h>
#include <wx/string.h>

#include <algorithm>
#include <cmath>

namespace sf
{

// -----------------------------------------------------------------------------
//  几何别名
// -----------------------------------------------------------------------------
//  统一用别名而不是直接用 wxPoint，将来若换坐标系只需改这一处。
//  注意：这里用的都是 wx 的「值类型」（不拖入 UI 框架），符合 L2 层约束。
// -----------------------------------------------------------------------------
using Point = wxPoint;
using Rect = wxRect;

constexpr double kPi = 3.14159265358979323846;

inline double Deg2Rad(double deg) { return deg * kPi / 180.0; }
inline double Rad2Deg(double rad) { return rad * 180.0 / kPi; }

// 网格尺寸：细网格 10px，粗网格 50px（1 个引脚间距 = 10px）
namespace grid
{
constexpr int kFine = 10;
constexpr int kCoarse = 50;
}  // namespace grid

/// 把单个坐标吸附到网格（grid <= 0 表示不吸附，原值返回）。
inline int SnapValue(int v, int step)
{
    if (step <= 0) return v;
    // 负数用 floor 语义，避免朝 0 舍入导致的不对称
    return static_cast<int>(std::floor(static_cast<double>(v) / step + 0.5)) * step;
}

/// 把点吸附到网格。
inline Point SnapPoint(const Point& p, int step)
{
    return Point(SnapValue(p.x, step), SnapValue(p.y, step));
}

/// 由任意两点构造规范矩形（自动处理负宽高）。
inline Rect NormalizeRect(const Point& a, const Point& b)
{
    const int x1 = std::min(a.x, b.x);
    const int y1 = std::min(a.y, b.y);
    const int x2 = std::max(a.x, b.x);
    const int y2 = std::max(a.y, b.y);
    return Rect(x1, y1, x2 - x1, y2 - y1);
}

/// 点 p 到线段 ab 的距离平方（整数运算，避免开方）。
inline long long PointSegmentDistSq(const Point& p, const Point& a, const Point& b)
{
    const long long dx = b.x - a.x;
    const long long dy = b.y - a.y;
    const long long lenSq = dx * dx + dy * dy;
    long long t = 0;
    if (lenSq != 0)
    {
        t = ((static_cast<long long>(p.x - a.x) * dx) + (static_cast<long long>(p.y - a.y) * dy)) * 1000 / lenSq;
        t = std::clamp<long long>(t, 0, 1000);
    }
    const long long px = a.x * 1000 + t * dx / 1000 - static_cast<long long>(p.x) * 1000;
    const long long py = a.y * 1000 + t * dy / 1000 - static_cast<long long>(p.y) * 1000;
    return (px * px + py * py) / 1000000;
}

/// 线段命中测试：p 距线段 ab 的距离是否在 tol 之内。
inline bool PointOnSegment(const Point& p, const Point& a, const Point& b, int tol)
{
    const long long d2 = PointSegmentDistSq(p, a, b);
    return d2 <= static_cast<long long>(tol) * tol;
}

/// 绕原点旋转 90° 的整数倍。
/// 屏幕坐标系（y 轴向下）下 (x, y) -> (-y, x) 视觉上是「顺时针 90°」。
inline Point RotatePoint90(const Point& p, int quarters)
{
    int q = quarters % 4;
    if (q < 0) q += 4;
    switch (q)
    {
    case 1:  return Point(-p.y, p.x);
    case 2:  return Point(-p.x, -p.y);
    case 3:  return Point(p.y, -p.x);
    default: return p;
    }
}

/// 绕原点水平镜像（x 取反）。
inline Point MirrorPointX(const Point& p) { return Point(-p.x, p.y); }

/// 两点是否近似相等（用于去重）。
inline bool NearlyEqual(const Point& a, const Point& b, int tol = 1)
{
    return std::abs(a.x - b.x) <= tol && std::abs(a.y - b.y) <= tol;
}

// -----------------------------------------------------------------------------
//  调色板（Apple 现代风格 · 中性浅色主题）
// -----------------------------------------------------------------------------
//  逻辑电平配色约定：0 灰、1 绿、X 红、Z 浅灰。
//  Apple 蓝 0x007AFF 作为强调色 / 选中色。
// -----------------------------------------------------------------------------
namespace palette
{
inline const wxColour kCanvasBg(0xFB, 0xFB, 0xFC);
inline const wxColour kGrid(0xEC, 0xEC, 0xEE);
inline const wxColour kGridMajor(0xDB, 0xDB, 0xDF);

inline const wxColour kComponentStroke(0x33, 0x37, 0x3D);
inline const wxColour kComponentFill(0xFF, 0xFF, 0xFF);
inline const wxColour kText(0x1D, 0x1D, 0x1F);
inline const wxColour kTextDim(0x6E, 0x6E, 0x73);

inline const wxColour kWireIdle(0x9A, 0x9A, 0xA0);
inline const wxColour kWireSelect(0x00, 0x7A, 0xFF);
inline const wxColour kWireHover(0x6F, 0xD9, 0x8A);

inline const wxColour kSelect(0x00, 0x7A, 0xFF);
inline const wxColour kPin(0x8E, 0x8E, 0x93);
inline const wxColour kPinHot(0x00, 0x7A, 0xFF);

inline const wxColour kLogicZero(0x8A, 0x8A, 0x90);
inline const wxColour kLogicOne(0x34, 0xC7, 0x59);
inline const wxColour kLogicX(0xFF, 0x3B, 0x30);
inline const wxColour kLogicZ(0xC9, 0xC9, 0xCE);

inline const wxColour kPreviewFill(0x00, 0x7A, 0xFF, 40);
}  // namespace palette

// -----------------------------------------------------------------------------
//  界面外观（Apple 现代风格）：自定义标题栏 / 工具栏 / 面板 / 状态栏取色与字体。
//  这里只放「值类型」（wxColour / wxFont），不引入任何 wx 控件，仍符合 L2 约束。
// -----------------------------------------------------------------------------
namespace ui
{
inline const wxColour kChrome(0xE9, 0xE9, 0xEC);       ///< 标题栏 / 状态栏底
inline const wxColour kChromeAlt(0xF1, 0xF1, 0xF4);    ///< 工具栏底
inline const wxColour kContent(0xF5, 0xF5, 0xF7);      ///< 工作区底
inline const wxColour kPanel(0xFF, 0xFF, 0xFF);        ///< 面板底
inline const wxColour kSeparator(0xE2, 0xE2, 0xE6);    ///< 分隔线
inline const wxColour kTitleText(0x1D, 0x1D, 0x1F);
inline const wxColour kSecondary(0x6E, 0x6E, 0x73);
inline const wxColour kAccent(0x00, 0x7A, 0xFF);
inline const wxColour kAccentHot(0x00, 0x68, 0xD6);
inline const wxColour kDanger(0xFF, 0x3B, 0x30);
inline const wxColour kHoverFill(0x00, 0x00, 0x00, 18);  ///< 悬停覆盖（带透明度）
inline const wxColour kActiveFill(0x00, 0x7A, 0xFF, 26); ///< 选中覆盖（带透明度）

/// 统一 UI 字体（默认族）。pt 为磅值，bold 为粗体。
inline wxFont Font(int pt = 9, bool bold = false)
{
    return wxFont(wxFontInfo(pt).Bold(bold).FaceName("Microsoft YaHei UI"));
}
}  // namespace ui

}  // namespace sf
