#pragma once

// =============================================================================
//  SiliFlow · core/Types.h
//  基础值类型：几何别名、数学助手、调色板。
//  本文件是 L2 层最底层，不依赖任何其他 core 头文件，也不依赖任何 wx 控件。
// =============================================================================

#include <wx/colour.h>
#include <wx/font.h>
#include <wx/fontenum.h>  // 仅用于探测系统里是否存在某个字体族（值类型工具，不是控件）
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
inline const wxColour kGrid(0xE8, 0xE8, 0xEA);
inline const wxColour kGridMajor(0xD7, 0xD7, 0xDB);

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
//  界面外观（Apple HIG 设计语言在 wxWidgets 上的「唯一令牌来源」）
//  这里只放「值类型」（wxColour / wxFont / 整数常量），不引入任何 wx 控件，仍符合 L2 约束。
//
//  值全部取自 od/styles.css（Open Design 生成的 HTML 原型）的 :root 变量，
//  换算约定：CSS 1px == 1 逻辑像素；CSS 色值照抄；字号 px ≈ pt * 0.75。
//
//  约定：
//   · 所有尺寸都是 96 DPI 下的逻辑像素，使用处必须过 window->FromDIP() 再落盘。
//   · 面板、画布、主窗口一律从这里取色取字，不要在模块里硬编码颜色或 FaceName。
// -----------------------------------------------------------------------------
namespace ui
{

// ---- 颜色：外壳（原型 --bg-window / --bg-app / --chrome / --chrome-2）----
inline const wxColour kWindow(0xEC, 0xEC, 0xEE);     ///< 窗口最外层底
inline const wxColour kContent(0xF5, 0xF5, 0xF7);    ///< 工作区底（面板之间 1px 缝露出它）
inline const wxColour kChrome(0xE9, 0xE9, 0xEC);     ///< 标题栏 / 状态栏
inline const wxColour kChromeAlt(0xF1, 0xF1, 0xF4);  ///< 菜单栏 / 工具栏 / 标签栏

// ---- 颜色：面板与分层底（原型 --surface / -2 / -3）----
inline const wxColour kPanel(0xFF, 0xFF, 0xFF);     ///< 面板底（卡片）
inline const wxColour kPanelAlt(0xF5, 0xF5, 0xF7);  ///< 面板内次级区块 / 左侧信号列
inline const wxColour kSunken(0xEC, 0xEC, 0xF0);    ///< 凹陷底：按钮底 / 输入框底 / 分段控件槽
inline const wxColour kCanvas(0xFB, 0xFB, 0xFC);    ///< 画布底

// ---- 颜色：描边（原型 --border / --sep / --hair）----
inline const wxColour kBorder(0x00, 0x00, 0x00, 25);   ///< 主描边（≈ rgba(0,0,0,.10)）
inline const wxColour kSeparator(0xE4, 0xE4, 0xE8);    ///< 面板内分隔 / 发丝线
inline const wxColour kHairline(0x00, 0x00, 0x00, 18); ///< 更轻的发丝线（≈ rgba(0,0,0,.07)）

// ---- 颜色：文字（原型 --text / -2 / -3）----
inline const wxColour kTitleText(0x1D, 0x1D, 0x1F);  ///< 正文 / 值
inline const wxColour kSecondary(0x6E, 0x6E, 0x73);  ///< 面板标题 / 标签 / 次要文字
inline const wxColour kTertiary(0x90, 0x90, 0x96);   ///< 注记 / 单位 / 占位

// ---- 颜色：语义（原型 --accent* / --green / --red / --orange）----
inline const wxColour kAccent(0x00, 0x7A, 0xFF);       ///< 强调色 #007AFF
inline const wxColour kAccentHot(0x00, 0x68, 0xD6);    ///< 按下态 #0068D6
inline const wxColour kAccentWeak(0x00, 0x7A, 0xFF, 31); ///< 淡强调底（列表选中）
inline const wxColour kOnAccent(0xFF, 0xFF, 0xFF);
inline const wxColour kGreen(0x34, 0xC7, 0x59);   ///< 运行中 / 高电平
inline const wxColour kDanger(0xFF, 0x3B, 0x30);  ///< 危险 / 低电平冲突
inline const wxColour kWarn(0xFF, 0x95, 0x00);    ///< 未保存等提醒

// ---- 交互态底（原型是**不透明**的表面色，不是半透明叠加）----
inline const wxColour kHover(0xEC, 0xEC, 0xF0);  ///< 悬停底 = --surface-3
inline const wxColour kPress(0xE3, 0xE3, 0xE8);  ///< 按下底（比悬停再深一档）

// ---- 间距刻度（统一节奏，别再散落 wxALL 3 / 4 / 6）----
namespace space
{
constexpr int kXxs = 2;   ///< 分段控件槽与内容的缝
constexpr int kXs = 4;    ///< 图标与文字之间
constexpr int kSm = 6;    ///< 工具栏按钮之间
constexpr int kMd = 8;    ///< 面板内边距 / 工具条左右
constexpr int kLg = 12;   ///< 面板头左右内边距
constexpr int kXl = 14;   ///< 标题栏元素间距
}  // namespace space

// ---- 圆角刻度（原型 --r-xs:5 --r-sm:6 / 7 / --r:8 / 9 / --r-lg:12）----
namespace radius
{
constexpr int kXs = 5;   ///< 菜单项 / 树行 / 分类头
constexpr int kSm = 6;   ///< 就地编辑器等小控件
constexpr int kMd = 7;   ///< 分段控件内按钮 / 输入框
constexpr int kLg = 8;   ///< 工具栏按钮 / 面板内动作按钮 / 标签页
constexpr int kXl = 9;   ///< 分段控件外框
constexpr int k2Xl = 12; ///< 浮层 / 弹出菜单
}  // namespace radius

// ---- 字号刻度（pt，对应原型 px ≈ pt / 0.75）----
namespace fontsize
{
constexpr int kCaption = 8;   ///< ≈11px：注记 / 单位 / 占位符
constexpr int kBody = 9;      ///< ≈12px：正文 / 按钮 / 面板标题（配 bold）/ 树行
constexpr int kHead = 10;     ///< ≈13px：菜单文字 / 属性头 / 空状态主文案
constexpr int kDisplay = 12;  ///< ≈16px：应用名
}  // namespace fontsize

// ---- 控件尺寸刻度（96 DPI 逻辑像素，全部取自原型）----
namespace metric
{
constexpr int kTitleBarHeight = 38;   ///< .titlebar
constexpr int kMenuBarHeight = 30;    ///< .menubar
constexpr int kToolBarHeight = 46;    ///< .toolbar
constexpr int kToolButton = 32;       ///< .tbtn 高 / 纯图标宽
constexpr int kToolButtonTight = 24;  ///< 面板内小工具条里的按钮高
constexpr int kWindowButton = 34;     ///< 标题栏右侧窗口按钮宽
constexpr int kControlHeight = 28;    ///< 输入框 / 下拉高（.field input）
constexpr int kSearchHeight = 30;     ///< 元件库搜索框
constexpr int kPanelHeadHeight = 32;  ///< .panel-head（也就是 AUI 面板标题）
constexpr int kSubHeadHeight = 30;    ///< .bh-head（波形 / 消息的头）
constexpr int kStatusBarHeight = 26;  ///< .statusbar
constexpr int kTabBarHeight = 35;     ///< .tabs
constexpr int kSashSize = 4;          ///< AUI 分隔条（原型是 1px 缝，这里留给拖拽）
constexpr int kSidebarWidth = 264;    ///< .panel-left
constexpr int kRightWidth = 316;      ///< .panel-right
constexpr int kBottomHeight = 224;    ///< 底栏初始高
constexpr int kWaveSignalWidth = 180; ///< .wave-signals
constexpr int kRowHeight = 22;        ///< 波形行高
constexpr int kTrafficDot = 12;       ///< 标题栏交通灯直径
constexpr int kHairline = 1;
}  // namespace metric

// ---- 字体：跨平台候选回退 ----
//  原文硬编码 "Microsoft YaHei UI"，在 Linux 上会静默退化成默认族（中文可能变方框）。
//  这里按「中文字形优先」的顺序探测第一个可用的族名，探测不到就交回系统默认字体。
inline const wxString& UIFace()
{
    static const wxString face = []
    {
        static const char* const kCandidates[] = {
            "Microsoft YaHei UI", "Microsoft YaHei",           // Windows
            "PingFang SC", "Hiragino Sans GB",                 // macOS
            "Noto Sans CJK SC", "Source Han Sans SC",          // Linux 常见中文族
            "WenQuanYi Micro Hei", "Segoe UI", "DejaVu Sans"   // 兜底
        };
        for (const char* name : kCandidates)
        {
            const wxString candidate = wxString::FromUTF8(name);
            if (wxFontEnumerator::IsValidFacename(candidate)) return candidate;
        }
        return wxString();
    }();
    return face;
}

inline const wxString& MonoFace()
{
    static const wxString face = []
    {
        static const char* const kCandidates[] = {
            "Cascadia Mono", "Consolas",                                                // Windows
            "SF Mono", "Menlo", "Monaco",                                               // macOS
            "JetBrains Mono", "DejaVu Sans Mono", "Noto Sans Mono", "Liberation Mono"   // Linux
        };
        for (const char* name : kCandidates)
        {
            const wxString candidate = wxString::FromUTF8(name);
            if (wxFontEnumerator::IsValidFacename(candidate)) return candidate;
        }
        return wxString();
    }();
    return face;
}

/// 统一 UI 字体。pt 为磅值，bold 为粗体。
inline wxFont Font(int pt = fontsize::kBody, bool bold = false)
{
    wxFontInfo info(pt);
    info.Bold(bold);
    const wxString& face = UIFace();
    if (!face.IsEmpty()) info.FaceName(face);
    return wxFont(info);
}

/// 等宽字体：控制台、VCD 文本、引脚表等需要对齐的场合。
inline wxFont MonoFont(int pt = fontsize::kBody, bool bold = false)
{
    wxFontInfo info(pt);
    info.Bold(bold);
    const wxString& face = MonoFace();
    if (!face.IsEmpty()) info.FaceName(face);
    return wxFont(info);
}

}  // namespace ui

}  // namespace sf
