#include "UiKit.h"

#include <wx/aui/aui.h>
#include <wx/cursor.h>
#include <wx/dcbuffer.h>
#include <wx/dcclient.h>
#include <wx/graphics.h>
#include <wx/sizer.h>

#include <algorithm>
#include <cmath>
#include <memory>

namespace sf
{
namespace ui
{

// =============================================================================
//  矢量图标
// =============================================================================
void DrawGlyph(wxGraphicsContext& gc, Glyph g, const wxRect& r, const wxColour& col)
{
    if (g == Glyph::None) return;

    const double w = r.width, h = r.height;
    const double cx = r.x + w / 2.0, cy = r.y + h / 2.0;
    const double s = std::min(w, h);
    // 原型图标是 24x24 viewBox + stroke 1.7，按 .ico 15px 显示 ⇒ 视觉笔画 ≈ 1.1px。
    // 这里按实际尺寸换算，DPI 放大后笔画同比变粗。
    const double k = s / 22.0;
    auto stroke = [&](double width) { gc.SetPen(wxPen(col, std::max(1, static_cast<int>(width * k)))); };

    gc.SetBrush(*wxTRANSPARENT_BRUSH);

    switch (g)
    {
    // ---- 窗口按钮 ----
    case Glyph::Min: stroke(1.6); gc.StrokeLine(cx - s * .26, cy, cx + s * .26, cy); break;
    case Glyph::Max: stroke(1.6);
        gc.DrawRoundedRectangle(cx - s * .26, cy - s * .26, s * .52, s * .52, s * .08); break;
    case Glyph::Restore: stroke(1.6);
        gc.DrawRoundedRectangle(cx - s * .30, cy - s * .10, s * .34, s * .34, s * .06);
        gc.DrawRoundedRectangle(cx - s * .08, cy - s * .28, s * .34, s * .34, s * .06); break;
    case Glyph::Close: stroke(1.6);
        gc.StrokeLine(cx - s * .22, cy - s * .22, cx + s * .22, cy + s * .22);
        gc.StrokeLine(cx + s * .22, cy - s * .22, cx - s * .22, cy + s * .22); break;

    // ---- 工具 ----
    case Glyph::Select: stroke(1.5); {  // 原型 cursor：一个箭头多边形
        wxGraphicsPath p = gc.CreatePath();
        p.MoveToPoint(cx - s * .22, cy - s * .30);
        p.AddLineToPoint(cx - s * .22, cy + s * .26);
        p.AddLineToPoint(cx - s * .04, cy + s * .08);
        p.AddLineToPoint(cx + s * .08, cy + s * .30);
        p.AddLineToPoint(cx + s * .18, cy + s * .24);
        p.AddLineToPoint(cx + s * .06, cy + s * .02);
        p.AddLineToPoint(cx + s * .24, cy + s * .02);
        p.CloseSubpath();
        gc.StrokePath(p);
    } break;
    case Glyph::Hand: stroke(1.5); {
        gc.StrokeLine(cx - s * .12, cy + s * .30, cx - s * .12, cy - s * .08);
        gc.StrokeLine(cx + s * .02, cy + s * .32, cx + s * .02, cy - s * .20);
        gc.StrokeLine(cx + s * .16, cy + s * .30, cx + s * .16, cy - s * .02);
        wxGraphicsPath p = gc.CreatePath();
        p.MoveToPoint(cx - s * .12, cy + s * .10);
        p.AddLineToPoint(cx - s * .28, cy - s * .04);
        gc.StrokePath(p);
    } break;
    case Glyph::Text: stroke(1.5);
        gc.StrokeLine(cx - s * .26, cy - s * .26, cx + s * .26, cy - s * .26);
        gc.StrokeLine(cx, cy - s * .26, cx, cy + s * .28);
        gc.StrokeLine(cx - s * .14, cy + s * .28, cx + s * .14, cy + s * .28); break;
    case Glyph::Chip: stroke(1.4);
        gc.DrawRoundedRectangle(cx - s * .22, cy - s * .22, s * .44, s * .44, s * .06);
        gc.StrokeLine(cx - s * .12, cy - s * .22, cx - s * .12, cy - s * .32);
        gc.StrokeLine(cx + s * .12, cy - s * .22, cx + s * .12, cy - s * .32);
        gc.StrokeLine(cx - s * .12, cy + s * .22, cx - s * .12, cy + s * .32);
        gc.StrokeLine(cx + s * .12, cy + s * .22, cx + s * .12, cy + s * .32);
        gc.StrokeLine(cx - s * .32, cy - s * .10, cx - s * .22, cy - s * .10);
        gc.StrokeLine(cx - s * .32, cy + s * .10, cx - s * .22, cy + s * .10);
        gc.StrokeLine(cx + s * .22, cy - s * .10, cx + s * .32, cy - s * .10);
        gc.StrokeLine(cx + s * .22, cy + s * .10, cx + s * .32, cy + s * .10); break;
    case Glyph::Wire: stroke(1.5); {
        gc.SetBrush(wxBrush(col));
        gc.DrawEllipse(cx - s * .32, cy + s * .12, s * .16, s * .16);
        gc.DrawEllipse(cx + s * .16, cy - s * .28, s * .16, s * .16);
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        wxGraphicsPath p = gc.CreatePath();
        p.MoveToPoint(cx - s * .23, cy + s * .20);
        p.AddCurveToPoint(cx + s * .02, cy + s * .15, cx + s * .06, cy - s * .06, cx + s * .23, cy - s * .19);
        gc.StrokePath(p);
    } break;
    case Glyph::Shapes: stroke(1.5);
        gc.DrawRoundedRectangle(cx - s * .30, cy - s * .30, s * .34, s * .34, s * .06);
        gc.DrawEllipse(cx - s * .02, cy - s * .02, s * .34, s * .34); break;
    case Glyph::Eraser: stroke(1.5); {
        wxGraphicsPath p = gc.CreatePath();
        p.MoveToPoint(cx - s * .30, cy + s * .18);
        p.AddLineToPoint(cx + s * .02, cy - s * .30);
        p.AddLineToPoint(cx + s * .30, cy - s * .04);
        p.AddLineToPoint(cx - s * .02, cy + s * .26);
        p.CloseSubpath();
        gc.StrokePath(p);
        gc.StrokeLine(cx - s * .34, cy + s * .34, cx + s * .32, cy + s * .34);
    } break;

    // ---- 变换 / 视图 / 仿真 ----
    case Glyph::Rotate: stroke(1.6); {
        wxGraphicsPath p = gc.CreatePath();
        p.AddArc(cx, cy, s * .28, 0.7, 5.4, true);
        gc.StrokePath(p);
        gc.SetPen(*wxTRANSPARENT_PEN);
        gc.SetBrush(wxBrush(col));
        wxGraphicsPath a = gc.CreatePath();
        a.MoveToPoint(cx + s * .30, cy - s * .26);
        a.AddLineToPoint(cx + s * .14, cy - s * .30);
        a.AddLineToPoint(cx + s * .32, cy - s * .40);
        a.CloseSubpath();
        gc.FillPath(a);
    } break;
    case Glyph::Mirror: stroke(1.5);
        gc.StrokeLine(cx, cy - s * .32, cx, cy + s * .32); {
        wxGraphicsPath l = gc.CreatePath();
        l.MoveToPoint(cx - s * .08, cy - s * .18); l.AddLineToPoint(cx - s * .28, cy);
        l.AddLineToPoint(cx - s * .08, cy + s * .18); l.CloseSubpath();
        gc.StrokePath(l);
        wxGraphicsPath rr = gc.CreatePath();
        rr.MoveToPoint(cx + s * .08, cy - s * .18); rr.AddLineToPoint(cx + s * .28, cy);
        rr.AddLineToPoint(cx + s * .08, cy + s * .18); rr.CloseSubpath();
        gc.StrokePath(rr); } break;
    case Glyph::ZoomIn: stroke(1.6);
        gc.DrawEllipse(cx - s * .30, cy - s * .30, s * .48, s * .48);
        gc.StrokeLine(cx - s * .06, cy - s * .16, cx - s * .06, cy + s * .04);
        gc.StrokeLine(cx - s * .16, cy - s * .06, cx + s * .04, cy - s * .06);
        gc.StrokeLine(cx + s * .12, cy + s * .12, cx + s * .30, cy + s * .30); break;
    case Glyph::ZoomOut: stroke(1.6);
        gc.DrawEllipse(cx - s * .30, cy - s * .30, s * .48, s * .48);
        gc.StrokeLine(cx - s * .16, cy - s * .06, cx + s * .04, cy - s * .06);
        gc.StrokeLine(cx + s * .12, cy + s * .12, cx + s * .30, cy + s * .30); break;
    case Glyph::Plus: stroke(1.8); gc.StrokeLine(cx - s * .30, cy, cx + s * .30, cy);
        gc.StrokeLine(cx, cy - s * .30, cx, cy + s * .30); break;
    case Glyph::Minus: stroke(1.8); gc.StrokeLine(cx - s * .30, cy, cx + s * .30, cy); break;
    case Glyph::Fit: stroke(1.5);
        gc.StrokeLine(cx - s * .30, cy - s * .16, cx - s * .30, cy - s * .30);
        gc.StrokeLine(cx - s * .30, cy - s * .30, cx - s * .16, cy - s * .30);
        gc.StrokeLine(cx + s * .30, cy - s * .16, cx + s * .30, cy - s * .30);
        gc.StrokeLine(cx + s * .30, cy - s * .30, cx + s * .16, cy - s * .30);
        gc.StrokeLine(cx - s * .30, cy + s * .16, cx - s * .30, cy + s * .30);
        gc.StrokeLine(cx - s * .30, cy + s * .30, cx - s * .16, cy + s * .30);
        gc.StrokeLine(cx + s * .30, cy + s * .16, cx + s * .30, cy + s * .30);
        gc.StrokeLine(cx + s * .30, cy + s * .30, cx + s * .16, cy + s * .30); break;
    case Glyph::Play: gc.SetPen(*wxTRANSPARENT_PEN); gc.SetBrush(wxBrush(col)); {
        wxGraphicsPath p = gc.CreatePath();
        p.MoveToPoint(cx - s * .22, cy - s * .30);
        p.AddLineToPoint(cx + s * .30, cy);
        p.AddLineToPoint(cx - s * .22, cy + s * .30);
        p.CloseSubpath();
        gc.FillPath(p);
    } break;
    case Glyph::Stop: gc.SetPen(*wxTRANSPARENT_PEN); gc.SetBrush(wxBrush(col));
        gc.DrawRoundedRectangle(cx - s * .26, cy - s * .26, s * .52, s * .52, s * .08); break;
    case Glyph::Step: stroke(1.6);
        gc.StrokeLine(cx - s * .28, cy - s * .30, cx - s * .28, cy + s * .30);
        gc.SetPen(*wxTRANSPARENT_PEN); gc.SetBrush(wxBrush(col)); {
        wxGraphicsPath p = gc.CreatePath();
        p.MoveToPoint(cx - s * .18, cy - s * .30);
        p.AddLineToPoint(cx + s * .30, cy);
        p.AddLineToPoint(cx - s * .18, cy + s * .30);
        p.CloseSubpath();
        gc.FillPath(p);
    } break;
    case Glyph::Reset: stroke(1.6); {
        wxGraphicsPath p = gc.CreatePath();
        p.AddArc(cx, cy, s * .28, 2.3, 5.7, true);
        gc.StrokePath(p);
        gc.SetPen(*wxTRANSPARENT_PEN);
        gc.SetBrush(wxBrush(col));
        wxGraphicsPath a = gc.CreatePath();
        a.MoveToPoint(cx - s * .30, cy - s * .22);
        a.AddLineToPoint(cx - s * .14, cy - s * .28);
        a.AddLineToPoint(cx - s * .34, cy - s * .38);
        a.CloseSubpath();
        gc.FillPath(a);
    } break;

    // ---- 标题栏动作 ----
    case Glyph::ThemeMoon: stroke(1.5); {  // 原型月亮路径
        wxGraphicsPath p = gc.CreatePath();
        p.MoveToPoint(cx + s * .30, cy + s * .06);
        p.AddCurveToPoint(cx + s * .06, cy + s * .34, cx - s * .34, cy + s * .10, cx - s * .22, cy - s * .30);
        gc.StrokePath(p);
        p = gc.CreatePath();
        p.MoveToPoint(cx - s * .22, cy - s * .30);
        p.AddArc(cx, cy - s * .02, s * .30, 3.6, 5.0, true);
        gc.StrokePath(p);
    } break;
    case Glyph::Help: stroke(1.5);
        gc.DrawEllipse(cx - s * .30, cy - s * .30, s * .60, s * .60); {
        wxGraphicsPath p = gc.CreatePath();
        p.MoveToPoint(cx - s * .10, cy - s * .12);
        p.AddCurveToPoint(cx + s * .12, cy - s * .22, cx + s * .12, cy + s * .02, cx, cy + s * .06);
        p.AddLineToPoint(cx, cy + s * .12);
        gc.StrokePath(p);
        gc.SetPen(*wxTRANSPARENT_PEN);
        gc.SetBrush(wxBrush(col));
        gc.DrawEllipse(cx - s * .03, cy + s * .20, s * .07, s * .07);
    } break;
    case Glyph::SidebarToggle: stroke(1.5); {
        gc.DrawRoundedRectangle(cx - s * .32, cy - s * .26, s * .64, s * .52, s * .08);
        gc.StrokeLine(cx - s * .10, cy - s * .26, cx - s * .10, cy + s * .26);
    } break;

    // ---- 面板动作 ----
    case Glyph::Refresh: stroke(1.5); {
        wxGraphicsPath p = gc.CreatePath();
        p.AddArc(cx, cy, s * .26, 1.0, 5.6, true);
        gc.StrokePath(p);
        gc.SetPen(*wxTRANSPARENT_PEN);
        gc.SetBrush(wxBrush(col));
        wxGraphicsPath a = gc.CreatePath();
        a.MoveToPoint(cx + s * .30, cy - s * .20);
        a.AddLineToPoint(cx + s * .12, cy - s * .26);
        a.AddLineToPoint(cx + s * .32, cy - s * .36);
        a.CloseSubpath();
        gc.FillPath(a);
    } break;
    case Glyph::Trash: stroke(1.5);
        gc.StrokeLine(cx - s * .30, cy - s * .22, cx + s * .30, cy - s * .22);
        gc.StrokeLine(cx - s * .12, cy - s * .22, cx - s * .12, cy - s * .30);
        gc.StrokeLine(cx + s * .12, cy - s * .22, cx + s * .12, cy - s * .30);
        gc.StrokeLine(cx - s * .12, cy - s * .30, cx + s * .12, cy - s * .30);
        gc.StrokeLine(cx - s * .22, cy - s * .22, cx - s * .18, cy + s * .30);
        gc.StrokeLine(cx + s * .22, cy - s * .22, cx + s * .18, cy + s * .30);
        gc.StrokeLine(cx - s * .18, cy + s * .30, cx + s * .18, cy + s * .30); break;
    case Glyph::ChevronRight: stroke(1.8); {
        wxGraphicsPath p = gc.CreatePath();
        p.MoveToPoint(cx - s * .12, cy - s * .22);
        p.AddLineToPoint(cx + s * .12, cy);
        p.AddLineToPoint(cx - s * .12, cy + s * .22);
        gc.StrokePath(p);
    } break;
    case Glyph::Check: stroke(1.9); {
        wxGraphicsPath p = gc.CreatePath();
        p.MoveToPoint(cx - s * .26, cy + s * .02);
        p.AddLineToPoint(cx - s * .06, cy + s * .22);
        p.AddLineToPoint(cx + s * .26, cy - s * .22);
        gc.StrokePath(p);
    } break;
    case Glyph::Search: stroke(1.6);
        gc.DrawEllipse(cx - s * .30, cy - s * .30, s * .48, s * .48);
        gc.StrokeLine(cx + s * .12, cy + s * .12, cx + s * .30, cy + s * .30); break;
    case Glyph::FolderOpen: stroke(1.5); {
        wxGraphicsPath p = gc.CreatePath();
        p.MoveToPoint(cx - s * .30, cy + s * .26);
        p.AddLineToPoint(cx - s * .30, cy - s * .22);
        p.AddLineToPoint(cx - s * .06, cy - s * .22);
        p.AddLineToPoint(cx + s * .02, cy - s * .12);
        p.AddLineToPoint(cx + s * .22, cy - s * .12);
        p.AddLineToPoint(cx + s * .22, cy + s * .26);
        p.CloseSubpath();
        gc.StrokePath(p);
    } break;

    case Glyph::None:
    default: break;
    }

    gc.SetBrush(*wxTRANSPARENT_BRUSH);
}

// =============================================================================
//  空状态（原型 .wave-center / .empty-note）
// =============================================================================
void DrawEmptyState(wxGraphicsContext& gc, const wxRect& area, const wxString& title,
                    const wxString& subtitle)
{
    if (area.width <= 0 || area.height <= 0) return;

    const wxFont titleFont = Font(fontsize::kHead);
    const wxFont bodyFont = Font(fontsize::kCaption);
    const int gap = wxWindow::FromDIP(space::kSm, nullptr);
    const int pad = wxWindow::FromDIP(space::kLg, nullptr);
    const int maxTextWidth = wxWindow::FromDIP(340, nullptr);

    // 按原型 .ws{max-width:340px} 做简单折行。
    wxArrayString lines;
    lines.Add(title);
    wxString rest = subtitle;
    while (!rest.IsEmpty() && maxTextWidth > 0)
    {
        size_t cut = rest.length();
        while (cut > 1)
        {
            double lw = 0, lh = 0;
            gc.SetFont(bodyFont, kTertiary);
            const wxString probe = rest.Left(cut);
            gc.GetTextExtent(probe, &lw, &lh);
            if (lw <= maxTextWidth - 4 * pad) break;
            --cut;
        }
        lines.Add(rest.Left(cut));
        rest = rest.Mid(cut);
    }

    double total = 0;
    std::vector<double> heights;
    for (size_t i = 0; i < lines.size(); ++i)
    {
        gc.SetFont(i == 0 ? titleFont : bodyFont, i == 0 ? kSecondary : kTertiary);
        double w = 0, h = 0;
        gc.GetTextExtent(lines[i], &w, &h);
        heights.push_back(h);
        total += h + (i == 0 ? gap : 0);
    }

    double y = area.y + (area.height - total) / 2.0;
    for (size_t i = 0; i < lines.size(); ++i)
    {
        gc.SetFont(i == 0 ? titleFont : bodyFont, i == 0 ? kSecondary : kTertiary);
        double w = 0, h = 0;
        gc.GetTextExtent(lines[i], &w, &h);
        gc.DrawText(lines[i], area.x + (area.width - w) / 2.0, y);
        y += h + (i == 0 ? gap : 0);
    }
}

// =============================================================================
//  扁平按钮
// =============================================================================
Button::Button(wxWindow* parent, wxWindowID id, Glyph glyph, const wxString& label, Style style,
               const wxString& tip, bool danger)
    : wxWindow(parent, id, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE),
      m_glyph(glyph), m_label(label), m_style(style), m_danger(danger)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetCursor(wxCursor(wxCURSOR_HAND));
    if (!tip.IsEmpty()) SetToolTip(tip);
    Bind(wxEVT_PAINT, &Button::OnPaint, this);
    Bind(wxEVT_ENTER_WINDOW, &Button::OnEnter, this);
    Bind(wxEVT_LEAVE_WINDOW, &Button::OnLeave, this);
    Bind(wxEVT_LEFT_DOWN, &Button::OnDown, this);
    Bind(wxEVT_LEFT_UP, &Button::OnUp, this);
}

void Button::SetActive(bool on)
{
    if (m_active == on) return;
    m_active = on;
    Refresh();
}

void Button::SetTint(const wxColour& c)
{
    m_tint = c;
    m_hasTint = true;
    Refresh();
}

void Button::SetCompact(bool on)
{
    m_compact = on;
    InvalidateBestSize();
    Refresh();
}

wxSize Button::DoGetBestSize() const
{
    auto* self = const_cast<Button*>(this);
    wxClientDC dc(self);
    dc.SetFont(Font(fontsize::kBody));

    switch (m_style)
    {
    case Style::Window:
        return wxSize(FromDIP(metric::kWindowButton), FromDIP(30));

    case Style::Icon:
        return wxSize(FromDIP(metric::kToolButton), FromDIP(metric::kToolButton));

    case Style::IconLabel:
    {
        const int icon = FromDIP(kIconSize);
        const int text = m_label.IsEmpty() ? 0 : dc.GetTextExtent(m_label).x;
        const int pad = FromDIP(space::kMd);
        return wxSize(icon + (text == 0 ? 0 : FromDIP(space::kSm) + text) + 2 * pad,
                      FromDIP(m_compact ? metric::kToolButtonTight : metric::kToolButton));
    }

    case Style::SegItem:
    {
        const int icon = FromDIP(kIconSize);
        const int text = m_label.IsEmpty() ? 0 : dc.GetTextExtent(m_label).x;
        const int pad = FromDIP(9);
        return wxSize(icon + (text == 0 ? 0 : FromDIP(space::kSm) + text) + 2 * pad,
                      FromDIP(metric::kToolButton) - 2 * FromDIP(space::kXxs));
    }

    case Style::Menu:
    {
        // .menu-btn{padding:3px 9px; border-radius:5px; font-size:13px}
        const int text = dc.GetTextExtent(m_label).x;
        return wxSize(text + 2 * FromDIP(9), FromDIP(24));
    }

    case Style::Text:
    default:
    {
        const int text = dc.GetTextExtent(m_label).x;
        const int icon = m_glyph == Glyph::None ? 0 : FromDIP(kIconSize) + FromDIP(space::kSm);
        return wxSize(text + icon + 2 * FromDIP(11),
                      FromDIP(m_compact ? metric::kToolButtonTight : 30));
    }
    }
}

void Button::OnPaint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    const wxSize sz = GetClientSize();
    const wxColour bg = GetParent() != nullptr ? GetParent()->GetBackgroundColour() : kPanel;
    dc.SetBackground(wxBrush(bg));
    dc.Clear();

    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (gc == nullptr) return;
    gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);

    const bool enabled = IsEnabled();
    const bool hover = enabled && m_hover;
    const bool press = enabled && m_pressed;
    const bool on = enabled && m_active;

    wxColour fill = wxColour(0, 0, 0, 0);
    bool filled = true;
    wxColour border = wxColour(0, 0, 0, 0);
    bool bordered = false;
    wxColour fg = kTitleText;
    wxColour iconCol = fg;

    switch (m_style)
    {
    case Style::Menu:
        // .menu-btn：悬停凹陷底；展开态强调色实心 + 白字。
        filled = on || hover || press;
        fill = on ? kAccent : kHover;
        fg = on ? kOnAccent : (m_hasTint ? m_tint : kTitleText);
        break;

    case Style::Text:
        // .btn：凹陷底；悬停换更浅的 surface-2 并出现描边；危险按钮只把字染红。
        fill = press ? kSunken : (hover ? kPanelAlt : kSunken);
        bordered = hover;
        border = kBorder;
        fg = m_danger ? kDanger : (m_hasTint ? m_tint : kTitleText);
        break;

    case Style::Icon:
    case Style::IconLabel:
        // .tbtn：默认透明；悬停凹陷底 + 主文字色；按下/选中变强调色实心 + 白字。
        filled = on || hover || press;
        fill = press ? kAccentHot : (on ? kAccent : kHover);
        fg = (on || press) ? kOnAccent : (hover ? kTitleText : (m_hasTint ? m_tint : kSecondary));
        break;

    case Style::SegItem:
        // .seg button：选中变白底 + 主文字色 + 图标染强调色。
        filled = on || hover || press;
        fill = kPanel;
        border = kBorder;
        bordered = true;
        fg = (on || hover) ? kTitleText : (m_hasTint ? m_tint : kSecondary);
        iconCol = on ? kAccent : fg;
        break;

    case Style::Primary:
        // .tbtn.primary：常驻强调色；按下加深；运行中变绿。
        filled = true;
        fill = on ? m_runColour : (press || hover ? kAccentHot : kAccent);
        fg = kOnAccent;
        break;

    case Style::Window:
    default:
        filled = hover || press;
        fill = m_danger && (hover || press) ? kDanger : kHover;
        fg = (m_danger && (hover || press)) ? kOnAccent : kSecondary;
        break;
    }

    int corner = FromDIP(radius::kLg);
    if (m_style == Style::Menu) corner = FromDIP(radius::kXs);

    if (filled && (!enabled || fill.Alpha() != 0))
    {
        gc->SetBrush(wxBrush(fill));
        gc->SetPen(bordered ? wxPen(border, 1) : *wxTRANSPARENT_PEN);
        gc->DrawRoundedRectangle(0.5, 0.5, sz.x - 1.0, sz.y - 1.0, corner);
    }

    if (!enabled)
    {
        fg = kTertiary;
        iconCol = fg;
    }

    if (m_glyph == Glyph::None)
    {
        gc->SetFont(Font(fontsize::kBody), fg);
        double tw = 0, th = 0;
        gc->GetTextExtent(m_label, &tw, &th);
        gc->DrawText(m_label, (sz.x - tw) / 2.0, (sz.y - th) / 2.0);
        return;
    }

    const double icon = FromDIP(m_style == Style::Window ? 18 : kIconSize);
    gc->SetFont(Font(fontsize::kBody), fg);

    if (m_label.IsEmpty())
    {
        DrawGlyph(*gc, m_glyph, wxRect((sz.x - icon) / 2, (sz.y - icon) / 2, icon, icon), iconCol);
        return;
    }

    // 图标 + 文字横排，整组居中（原型 display:flex; align-items:center; gap:6px）
    double tw = 0, th = 0;
    gc->GetTextExtent(m_label, &tw, &th);
    const double gap = FromDIP(space::kSm);
    const double total = icon + gap + tw;
    const double x0 = (sz.x - total) / 2.0;
    DrawGlyph(*gc, m_glyph, wxRect(x0, (sz.y - icon) / 2, icon, icon), iconCol);
    gc->DrawText(m_label, x0 + icon + gap, (sz.y - th) / 2.0);
}

void Button::OnEnter(wxMouseEvent&)
{
    m_hover = true;
    Refresh();
}

void Button::OnLeave(wxMouseEvent&)
{
    m_hover = false;
    m_pressed = false;
    Refresh();
}

void Button::OnDown(wxMouseEvent&)
{
    m_pressed = true;
    Refresh();
}

void Button::OnUp(wxMouseEvent& e)
{
    m_pressed = false;
    Refresh();
    if (GetClientRect().Contains(e.GetPosition())) Click();
}

void Button::Click()
{
    if (!IsEnabled()) return;
    if (m_click)
    {
        m_click();
        return;
    }
    wxCommandEvent evt(wxEVT_MENU, GetId());
    evt.SetEventObject(this);
    GetEventHandler()->ProcessEvent(evt);
}

// =============================================================================
//  分段控件（原型 .seg）
// =============================================================================
SegmentedGroup::SegmentedGroup(wxWindow* parent, wxWindowID id)
    : wxPanel(parent, id, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(kSunken);
    Bind(wxEVT_PAINT, &SegmentedGroup::OnPaint, this);

    m_row = new wxBoxSizer(wxHORIZONTAL);
    SetSizer(m_row);
}

void SegmentedGroup::Add(Button* child)
{
    m_row->Add(child, 0, wxALIGN_CENTER_VERTICAL | wxALL, FromDIP(space::kXxs));
}

wxSize SegmentedGroup::DoGetBestSize() const
{
    if (m_row == nullptr) return wxSize(-1, FromDIP(metric::kToolButton));
    const wxSize need = const_cast<wxBoxSizer*>(m_row)->CalcMin();
    return need;
}

void SegmentedGroup::OnPaint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    const wxSize sz = GetClientSize();
    dc.SetBackground(wxBrush(kSunken));
    dc.Clear();

    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (gc == nullptr) return;
    gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
    gc->SetPen(wxPen(kHairline, 1));
    gc->DrawRoundedRectangle(0.5, 0.5, sz.x - 1.0, sz.y - 1.0, FromDIP(radius::kXl));
}

// =============================================================================
//  菜单栏（原型 .menubar）
// =============================================================================
MenuBar::MenuBar(wxWindow* parent, wxWindowID id)
    : wxPanel(parent, id, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(kChromeAlt);
    Bind(wxEVT_PAINT, &MenuBar::OnPaint, this);

    m_row = new wxBoxSizer(wxHORIZONTAL);
    m_row->AddSpacer(FromDIP(space::kMd));
    SetSizer(m_row);
}

Button* MenuBar::AddMenu(const wxString& label, const wxString& tip)
{
    auto* b = new Button(this, wxID_ANY, Glyph::None, label, Button::Style::Menu, tip);
    b->SetTint(kTitleText);
    m_row->Add(b, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(space::kXxs));
    return b;
}

void MenuBar::SetOpen(Button* button)
{
    if (m_open == button) return;
    if (m_open != nullptr) m_open->SetActive(false);
    m_open = button;
    if (m_open != nullptr) m_open->SetActive(true);
    Refresh();
}

wxSize MenuBar::DoGetBestSize() const
{
    return wxSize(-1, FromDIP(metric::kMenuBarHeight));
}

void MenuBar::OnPaint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    const wxSize sz = GetClientSize();
    dc.SetBackground(wxBrush(kChromeAlt));
    dc.Clear();
}

// =============================================================================
//  面板头（原型 .panel-head）
// =============================================================================
PanelHeader::PanelHeader(wxWindow* parent, const wxString& title, const wxString& subtitle, wxWindowID id)
    : wxPanel(parent, id, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE),
      m_title(title), m_subtitle(subtitle)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(kPanel);
    Bind(wxEVT_PAINT, &PanelHeader::OnPaint, this);
}

void PanelHeader::SetTitle(const wxString& title)
{
    m_title = title;
    Refresh(false);
}

void PanelHeader::SetSubtitle(const wxString& subtitle)
{
    m_subtitle = subtitle;
    Refresh(false);
}

void PanelHeader::SetTrailing(wxWindow* trailing)
{
    m_trailing = trailing;
    Refresh(false);
}

void PanelHeader::SetFlat(bool flat)
{
    m_flat = flat;
    Refresh(false);
}

wxSize PanelHeader::DoGetBestSize() const
{
    // 原型 .prop-head{padding:10px 12px 6px}，标题 13px ⇒ 10 + 一行 + 6 + 1px 分隔
    return wxSize(-1, FromDIP(10) + FromDIP(18) + FromDIP(6) + FromDIP(metric::kHairline));
}

void PanelHeader::OnPaint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    const wxSize sz = GetClientSize();
    dc.SetBackground(wxBrush(kPanel));
    dc.Clear();

    const int left = FromDIP(ui::space::kLg);
    const int top = FromDIP(10);

    if (!m_flat)
    {
        dc.SetPen(wxPen(kSeparator, 1));
        dc.DrawLine(0, sz.y - 1, sz.x, sz.y - 1);
    }

    // 原型 .prop-head .ph-t{font-size:13px; font-weight:600}
    dc.SetFont(Font(fontsize::kHead, true));
    dc.SetTextForeground(kTitleText);
    const wxSize ts = dc.GetTextExtent(m_title);
    dc.DrawText(m_title, left, top);

    if (!m_subtitle.IsEmpty())
    {
        // 原型 .ph-s{font-size:11.5px; color:var(--text-3)}：紧跟在标题右边
        dc.SetFont(Font(fontsize::kCaption));
        dc.SetTextForeground(kTertiary);
        const wxSize ss = dc.GetTextExtent(m_subtitle);
        dc.DrawText(m_subtitle, left + ts.x + FromDIP(space::kMd), top + (ts.y - ss.y));
    }

    if (m_trailing != nullptr)
    {
        const wxSize need = m_trailing->GetBestSize();
        m_trailing->SetSize(sz.x - left - need.x, (sz.y - need.y) / 2, need.x, need.y);
    }
}

// =============================================================================
//  胶囊标签（原型 .chip-dirty）
// =============================================================================
Chip::Chip(wxWindow* parent, const wxColour& tone, wxWindowID id)
    : wxPanel(parent, id, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE), m_tone(tone),
      m_font(Font(fontsize::kCaption))
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(kChrome);
    Bind(wxEVT_PAINT, &Chip::OnPaint, this);
}

void Chip::SetText(const wxString& text)
{
    if (m_text == text) return;
    m_text = text;
    InvalidateBestSize();
    Refresh(false);
}

wxSize Chip::DoGetBestSize() const
{
    wxClientDC dc(const_cast<Chip*>(this));
    dc.SetFont(m_font);
    const wxSize ts = dc.GetTextExtent(m_text.IsEmpty() ? wxString("未保存") : m_text);
    return wxSize(ts.x + 2 * FromDIP(space::kMd), ts.y + 2 * FromDIP(space::kXxs) + FromDIP(4));
}

void Chip::OnPaint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    const wxSize sz = GetClientSize();
    const wxColour bg = GetParent() != nullptr ? GetParent()->GetBackgroundColour() : kChrome;
    dc.SetBackground(wxBrush(bg));
    dc.Clear();

    dc.SetPen(wxPen(kBorder, 1));
    dc.SetBrush(wxBrush(kPanel));
    dc.DrawRoundedRectangle(0, 0, sz.x, sz.y, sz.y / 2.0);

    dc.SetFont(m_font);
    dc.SetTextForeground(m_tone);
    const wxSize ts = dc.GetTextExtent(m_text);
    dc.DrawText(m_text, (sz.x - ts.x) / 2, (sz.y - ts.y) / 2);
}

// =============================================================================
//  AUI 停靠外观
// =============================================================================
namespace
{

/// 在默认 DockArt 的基础上补一条面板标题底部的发丝线（原型 .panel-head 的 border-bottom）。
class DockArt : public wxAuiDefaultDockArt
{
public:
    void DrawCaption(wxDC& dc, wxWindow* window, const wxString& text, const wxRect& rect,
                     wxAuiPaneInfo& pane) override
    {
        wxAuiDefaultDockArt::DrawCaption(dc, window, text, rect, pane);
        dc.SetPen(wxPen(kSeparator, 1));
        dc.DrawLine(rect.x, rect.GetBottom(), rect.GetRight() + 1, rect.GetBottom());
    }
};

}  // namespace

wxAuiDockArt* MakeDockArt()
{
    auto* art = new DockArt();

    // 度量：与原型的 .panel-head 对齐（高 32，1px 缝）。
    art->SetMetric(wxAUI_DOCKART_PANE_BORDER_SIZE, 0);
    art->SetMetric(wxAUI_DOCKART_SASH_SIZE, wxWindow::FromDIP(metric::kSashSize, nullptr));
    art->SetMetric(wxAUI_DOCKART_CAPTION_SIZE, wxWindow::FromDIP(metric::kPanelHeadHeight, nullptr));
    art->SetMetric(wxAUI_DOCKART_GRADIENT_TYPE, wxAUI_GRADIENT_NONE);

    const int grip = wxWindow::FromDIP(3, nullptr);
    art->SetMetric(wxAUI_DOCKART_PANE_BUTTON_SIZE, wxWindow::FromDIP(14, nullptr));
    (void)grip;

    art->SetColour(wxAUI_DOCKART_BACKGROUND_COLOUR, kPanel);
    art->SetColour(wxAUI_DOCKART_SASH_COLOUR, kContent);
    art->SetColour(wxAUI_DOCKART_BORDER_COLOUR, kSeparator);
    art->SetColour(wxAUI_DOCKART_GRIPPER_COLOUR, kSeparator);
    art->SetColour(wxAUI_DOCKART_INACTIVE_CAPTION_COLOUR, kPanel);
    art->SetColour(wxAUI_DOCKART_INACTIVE_CAPTION_GRADIENT_COLOUR, kPanel);
    art->SetColour(wxAUI_DOCKART_INACTIVE_CAPTION_TEXT_COLOUR, kSecondary);
    art->SetColour(wxAUI_DOCKART_ACTIVE_CAPTION_COLOUR, kPanel);
    art->SetColour(wxAUI_DOCKART_ACTIVE_CAPTION_GRADIENT_COLOUR, kPanel);
    art->SetColour(wxAUI_DOCKART_ACTIVE_CAPTION_TEXT_COLOUR, kSecondary);
    art->SetFont(wxAUI_DOCKART_CAPTION_FONT, Font(fontsize::kBody, true));

    return art;
}

// =============================================================================
//  AUI 标签页外观（原型 .tabs / .tab）
// =============================================================================
namespace
{

/// 页签：激活页白底 + 上圆角 + 描边，未激活透明；关闭按钮悬停变红。
/// 布局数学照抄 wxAuiGenericTabArt::DrawTab（outTabRect 驱动命中测试），只换视觉。
class TabArt : public wxAuiGenericTabArt
{
public:
    TabArt()
    {
        const wxFont font = Font(fontsize::kBody);
        SetNormalFont(font);
        SetSelectedFont(font);
        SetMeasuringFont(font);
    }

    wxAuiTabArt* Clone() override
    {
        auto* art = new TabArt();
        art->SetFlags(m_flags);
        return art;
    }

    void DrawBackground(wxDC& dc, wxWindow*, const wxRect& rect) override
    {
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(kChromeAlt));  // 原型 .tabs{background:var(--chrome-2)}
        dc.DrawRectangle(rect);
    }

    int GetBorderWidth(wxWindow*) override { return 0; }
    int GetAdditionalBorderSpace(wxWindow*) override { return 0; }

    wxSize GetTabSize(wxDC& dc, wxWindow* wnd, const wxString& caption, const wxBitmapBundle&,
                      bool, int closeButtonState, int* xExtent) override
    {
        dc.SetFont(Font(fontsize::kBody));
        wxCoord tw = 0, th = 0;
        dc.GetTextExtent(caption.IsEmpty() ? wxString("Xj") : caption, &tw, &th);

        const int width = TabWidth(wnd, tw, closeButtonState);
        if (xExtent != nullptr) *xExtent = width;
        return wxSize(width, Dip(wnd, metric::kTabBarHeight) - Dip(wnd, 3));
    }

    void DrawTab(wxDC& dc, wxWindow* wnd, const wxAuiNotebookPage& page, const wxRect& inRect,
                 int closeButtonState, wxRect* outTabRect, wxRect* outButtonRect, int* xExtent) override
    {
        const int padX = Dip(wnd, 11);   // .tab{padding:5px 11px}
        const int btn = Dip(wnd, 16);    // .tab .tab-x{width:16px;height:16px}
        const int gap = Dip(wnd, space::kSm);
        const int bottom = Dip(wnd, 3);
        const bool showClose = closeButtonState != wxAUI_BUTTON_STATE_HIDDEN;

        const wxString caption = page.caption.IsEmpty() ? wxString("Xj") : page.caption;
        dc.SetFont(Font(fontsize::kBody));
        wxCoord tw = 0, th = 0;
        dc.GetTextExtent(caption, &tw, &th);

        const int want = TabWidth(wnd, tw, closeButtonState);
        const int width = std::max(1, std::min(want, std::max(1, inRect.width)));
        const int height = std::max(1, inRect.height - bottom);
        const wxRect tab(inRect.x, inRect.y + inRect.height - height, width, height);

        if (page.active)
        {
            // 上圆下直：底边与标签栏底线重合（原型 .tab{border-radius:8px 8px 0 0}）。
            // 这里用折线近似圆角 —— DrawTab 拿到的是 wxDC&（不一定能从它建 GraphicsContext）。
            const int r = Dip(wnd, radius::kLg);
            const int x0 = tab.x, y0 = tab.y;
            const int x1 = tab.x + tab.width - 1, y1 = tab.y + tab.height;
            const wxPoint pts[8] = {
                wxPoint(x0, y0 + r),
                wxPoint(x0 + r / 4, y0 + r / 4),
                wxPoint(x0 + r, y0),
                wxPoint(x1 - r, y0),
                wxPoint(x1 - r / 4, y0 + r / 4),
                wxPoint(x1, y0 + r),
                wxPoint(x1, y1),
                wxPoint(x0, y1),
            };
            dc.SetPen(wxPen(kBorder, 1));
            dc.SetBrush(wxBrush(kPanel));
            dc.DrawPolygon(8, pts);
        }

        dc.SetFont(Font(fontsize::kBody));
        dc.SetTextForeground(page.active ? kTitleText : kSecondary);
        dc.DrawText(caption, tab.x + padX, tab.y + (tab.height - th) / 2);

        wxRect buttonRect;
        if (showClose)
            buttonRect = wxRect(tab.GetRight() - padX - btn, tab.y + (tab.height - btn) / 2, btn, btn);

        if (outTabRect != nullptr) *outTabRect = tab;
        if (outButtonRect != nullptr) *outButtonRect = buttonRect;
        if (xExtent != nullptr) *xExtent = width;
    }

    void DrawButton(wxDC& dc, wxWindow* wnd, const wxRect& inRect, int bitmapId, int buttonState,
                    int orientation, wxRect* outRect) override
    {
        // 只接管关闭按钮；窗口列表等其它按钮交回基类，避免画错图标。
        if (bitmapId != wxAUI_BUTTON_CLOSE)
        {
            wxAuiGenericTabArt::DrawButton(dc, wnd, inRect, bitmapId, buttonState, orientation, outRect);
            return;
        }
        if (outRect != nullptr) *outRect = inRect;
        if (buttonState == wxAUI_BUTTON_STATE_HIDDEN) return;

        const bool hot = (buttonState & wxAUI_BUTTON_STATE_HOVER) != 0 ||
                         (buttonState & wxAUI_BUTTON_STATE_PRESSED) != 0;

        if (hot)
        {
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.SetBrush(wxBrush(kHover));
            dc.DrawRoundedRectangle(inRect, Dip(wnd, radius::kXs));
        }

        // 原型 .tab .tab-x：默认三级色，悬停变危险色
        const int pad = Dip(wnd, 5);
        dc.SetPen(wxPen(hot ? kDanger : kTertiary, 1));
        dc.DrawLine(inRect.x + pad, inRect.y + pad, inRect.GetRight() - pad, inRect.GetBottom() - pad);
        dc.DrawLine(inRect.GetRight() - pad, inRect.y + pad, inRect.x + pad, inRect.GetBottom() - pad);
    }

private:
    static int Dip(const wxWindow* w, int v) { return wxWindow::FromDIP(v, w); }

    static int TabWidth(const wxWindow* wnd, int textWidth, int closeButtonState)
    {
        const int padX = Dip(wnd, 11);
        int width = padX * 2 + textWidth;
        if (closeButtonState != wxAUI_BUTTON_STATE_HIDDEN)
            width += Dip(wnd, 16) + Dip(wnd, space::kSm);
        return width;
    }
};

}  // namespace

wxAuiTabArt* MakeTabArt()
{
    return new TabArt();
}

// =============================================================================
//  小工具
// =============================================================================
wxWindow* MakeSeparator(wxWindow* parent, int length, bool vertical)
{
    const wxSize sz = vertical ? wxSize(1, length) : wxSize(length, 1);
    auto* p = new wxPanel(parent, wxID_ANY, wxDefaultPosition, sz, wxBORDER_NONE);
    p->SetBackgroundColour(kSeparator);
    return p;
}

}  // namespace ui
}  // namespace sf
