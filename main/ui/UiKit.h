#pragma once

// =============================================================================
//  SiliFlow · ui/UiKit.h
//  共享视觉组件层（叶子层，L2.5）：
//    · 只依赖 core/Types.h 与 wx 控件，不依赖 canvas / panels / app
//    · app 与 panels 都可以用，但不许在自己模块里另画一套
//
//  组件与 od/styles.css（Open Design 原型）的对应关系：
//    ui::Button(Style::Text)      ←→ .btn            （面板内动作按钮）
//    ui::Button(Style::Icon)      ←→ .tbtn           （工具栏纯图标）
//    ui::Button(Style::IconLabel) ←→ .tbtn + .lbl    （工具栏图标 + 文字）
//    ui::Button(Style::SegItem)   ←→ .seg button     （分段控件内）
//    ui::Button(Style::Primary)   ←→ .tbtn.primary   （运行按钮，常驻强调色）
//    ui::Button(Style::Window)    ←→ .titlebar .tbtn （标题栏窗口按钮，紧凑）
//    ui::SegmentedGroup           ←→ .seg
//    ui::MenuBar                  ←→ .menubar
//    ui::PanelHeader              ←→ .panel-head
//    ui::DrawEmptyState           ←→ .wave-center / .empty-note
//
//  尺寸/取色一律来自 core/Types.h 的 ui:: 令牌；使用处全部过 FromDIP()。
// =============================================================================

#include "core/Types.h"

#include <wx/gdicmn.h>
#include <wx/panel.h>
#include <wx/window.h>

#include <functional>

class wxAuiDockArt;
class wxAuiTabArt;
class wxBoxSizer;
class wxGraphicsContext;

namespace sf
{
namespace ui
{

// -----------------------------------------------------------------------------
//  矢量图标
// -----------------------------------------------------------------------------
/// 单线条风格图标集合（零位图资源），viewBox 与原型 24x24 对齐。
enum class Glyph
{
    None,
    // 窗口
    Min, Max, Restore, Close,
    // 工具栏：工具
    Select, Hand, Text, Chip, Wire, Shapes, Eraser,
    // 工具栏：变换 / 视图 / 仿真
    Rotate, Mirror, ZoomIn, ZoomOut, Fit, Play, Stop, Step, Reset,
    // 标题栏动作
    ThemeMoon, Help, SidebarToggle,
    // 面板动作
    Plus, Minus, Refresh, Trash, ChevronRight, Search, FolderOpen, Check
};

/// 在 r 指定的方框内描边绘制图标。
/// 笔画宽度按 r 的尺寸换算，所以调用方对 rect 做过 FromDIP() 后，高 DPI 下笔画会同比变粗。
void DrawGlyph(wxGraphicsContext& gc, Glyph g, const wxRect& r, const wxColour& col);

/// 图标的“设计基准尺寸”（原型 .ico = 15px；标题栏用 18px）。
constexpr int kIconSize = 15;

// -----------------------------------------------------------------------------
//  空状态
// -----------------------------------------------------------------------------
/// 在任意自绘画布里画居中的空状态（主文案 13px 次要色 + 说明 11.5px 三级色）。
void DrawEmptyState(wxGraphicsContext& gc, const wxRect& area, const wxString& title,
                    const wxString& subtitle = wxString());

// -----------------------------------------------------------------------------
//  扁平按钮
// -----------------------------------------------------------------------------
class Button : public wxWindow
{
public:
    /// 形态。取色/圆角/内边距都按原型 CSS 逐条对齐，不要在这里塞新状态。
    enum class Style
    {
        Text,       ///< .btn：凹陷底 + 圆角 8 + 6x11 内边距
        Menu,       ///< .menu-btn：菜单栏文字按钮，展开时强调色实心
        Icon,       ///< .tbtn：32x32 图标按钮
        IconLabel,  ///< .tbtn + 文字，图标在左、文字在右
        SegItem,    ///< .seg button：分段控件内，选中变白底
        Primary,    ///< .tbtn.primary：常驻强调色，运行时变绿
        Window      ///< 标题栏窗口按钮：紧凑、悬停有点底、关闭悬停变红
    };

    Button(wxWindow* parent, wxWindowID id, Glyph glyph, const wxString& label,
           Style style = Style::Text, const wxString& tip = wxString(), bool danger = false);

    /// 点击回调。不设的话按 id 发 wxEVT_MENU，兼容老的菜单式绑定。
    void SetOnClick(std::function<void()> fn) { m_click = std::move(fn); }

    void SetActive(bool on);
    void SetTint(const wxColour& c);
    /// 紧凑高度（原型里面板内小工具条上的按钮写死 height:24px）。
    void SetCompact(bool on);
    /// “运行中”专用：Primary 形态在选中时改成绿色填充（原型 .tbtn.primary[aria-pressed=true]）。
    void SetRunningColour(const wxColour& c) { m_runColour = c; }

    wxSize DoGetBestSize() const override;

private:
    void OnPaint(wxPaintEvent& event);
    void OnEnter(wxMouseEvent& event);
    void OnLeave(wxMouseEvent& event);
    void OnDown(wxMouseEvent& event);
    void OnUp(wxMouseEvent& event);
    void Click();

    Glyph m_glyph;
    wxString m_label;
    Style m_style;
    bool m_danger = false;
    bool m_active = false;
    bool m_hover = false;
    bool m_pressed = false;
    bool m_compact = false;
    wxColour m_tint;      ///< 未指定时按形态取默认色
    bool m_hasTint = false;
    wxColour m_runColour = kGreen;
    std::function<void()> m_click;
};

// -----------------------------------------------------------------------------
//  分段控件（原型 .seg：一个凹陷槽，内部按钮选中变白底）
// -----------------------------------------------------------------------------
class SegmentedGroup : public wxPanel
{
public:
    explicit SegmentedGroup(wxWindow* parent, wxWindowID id = wxID_ANY);

    void Add(Button* child);

    wxSize DoGetBestSize() const override;

private:
    void OnPaint(wxPaintEvent& event);

    wxBoxSizer* m_row = nullptr;
};

// -----------------------------------------------------------------------------
//  菜单栏（原型 .menubar：30px 高，文字按钮 + 底部 1px 描边）
// -----------------------------------------------------------------------------
class MenuBar : public wxPanel
{
public:
    explicit MenuBar(wxWindow* parent, wxWindowID id = wxID_ANY);

    /// 追加一个顶层菜单按钮；返回按钮指针方便调用方绑定弹出逻辑。
    Button* AddMenu(const wxString& label, const wxString& tip = wxString());

    /// 标记某个菜单当前处于展开态（原型 aria-expanded=true → 强调色实心）。
    void SetOpen(Button* button);

    wxSize DoGetBestSize() const override;

private:
    void OnPaint(wxPaintEvent& event);

    wxBoxSizer* m_row = nullptr;
    Button* m_open = nullptr;
};

// -----------------------------------------------------------------------------
//  面板内内容标题（原型 .prop-head）
// -----------------------------------------------------------------------------
/// AUI 面板**自身**的标题栏由 ui::DockArt 负责（对应原型的 .panel-head）。
/// 这个类用于面板内部的内容标题：13px/600 主文案 + 11.5px 三级色副标题 + 底部发丝线，
/// 例如属性区显示“元件：AND2 / 类型 · 位置”。
class PanelHeader : public wxPanel
{
public:
    PanelHeader(wxWindow* parent, const wxString& title, const wxString& subtitle = wxString(),
                wxWindowID id = wxID_ANY);

    void SetTitle(const wxString& title);
    void SetSubtitle(const wxString& subtitle);
    /// 右侧动作区（例如原型 .hd-seg 里的“刷新”）。会被放到标题右侧。
    void SetTrailing(wxWindow* trailing);

    /// 关掉底部那条分隔线（用于紧跟着已有分隔线的场合）。
    void SetFlat(bool flat);

    wxSize DoGetBestSize() const override;

private:
    void OnPaint(wxPaintEvent& event);

    wxString m_title;
    wxString m_subtitle;
    wxWindow* m_trailing = nullptr;
    bool m_flat = false;
};

// -----------------------------------------------------------------------------
//  胶囊标签（原型 .chip-dirty）
// -----------------------------------------------------------------------------
/// 一行小字 + 描边圆角胶囊，用于「未保存」这类状态提示。
class Chip : public wxPanel
{
public:
    Chip(wxWindow* parent, const wxColour& tone = kWarn, wxWindowID id = wxID_ANY);

    void SetText(const wxString& text);

    wxSize DoGetBestSize() const override;

private:
    void OnPaint(wxPaintEvent& event);

    wxString m_text;
    wxColour m_tone;
    wxFont m_font;
};

// -----------------------------------------------------------------------------
//  AUI 停靠外观（把 AUI 的面板标题栏刷成原型的 .panel-head）
// -----------------------------------------------------------------------------
/// 返回一个配置好的 DockArt（调用方负责交给 wxAuiManager，所有权也随之转移）。
wxAuiDockArt* MakeDockArt();

// -----------------------------------------------------------------------------
//  AUI 标签页外观（把 wxAuiNotebook 的页签刷成原型的 .tabs / .tab）
// -----------------------------------------------------------------------------
/// 返回一个配置好的 TabArt（调用方负责交给 wxAuiNotebook，所有权也随之转移）。
wxAuiTabArt* MakeTabArt();

// -----------------------------------------------------------------------------
//  小工具
// -----------------------------------------------------------------------------
/// 1px 发丝线（竖直 / 水平）。length < 0 表示由 sizer 决定。
wxWindow* MakeSeparator(wxWindow* parent, int length, bool vertical = true);

}  // namespace ui
}  // namespace sf
