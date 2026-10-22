#include "MainFrame.h"

#include "MainMenuBar.h"
#include "canvas/CanvasEventHandler.h"
#include "canvas/CanvasNoteBook.h"
#include "canvas/CanvasPanel.h"
#include "core/SelfCheck.h"
#include "panels/ConsolePanel.h"
#include "panels/PropertyPanel.h"
#include "panels/ToolboxPanel.h"
#include "panels/TreePanel.h"
#include "panels/WavePanel.h"

#include <wx/aboutdlg.h>
#include <wx/cursor.h>
#include <wx/dcbuffer.h>
#include <wx/dcclient.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/graphics.h>
#include <wx/msgdlg.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/statusbr.h>
#include <wx/toplevel.h>
#include <wx/utils.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace sf
{

namespace
{

CanvasEventHandler* HandlerOf(CanvasPanel* canvas)
{
    return canvas == nullptr ? nullptr : canvas->Handler();
}

/// 节拍频率菜单项 → Hz
int SimHzForMenuId(int id)
{
    switch (id)
    {
    case ID_SIM_FREQ_1:  return 1;
    case ID_SIM_FREQ_2:  return 2;
    case ID_SIM_FREQ_5:  return 5;
    case ID_SIM_FREQ_10: return 10;
    case ID_SIM_FREQ_20: return 20;
    default:             return 2;
    }
}

}  // namespace

// =============================================================================
//  Apple 风格自定义外壳：标题栏 / 工具栏 / 窗口按钮（全部自绘，零位图资源）
// =============================================================================
namespace
{

enum class Glyph
{
    None, Min, Max, Restore, Close, Stop,
    Select, Hand, Text, Chip, Wire, Shapes, Eraser,
    Rotate, Mirror, ZoomIn, ZoomOut, Fit, Play, Step, Reset
};

constexpr int kTitleBarHeight = 40;
constexpr int kToolBarHeight = 52;

/// 用矢量线框画一个图标（Apple 单线条风格，stroke 一致）。
void DrawGlyph(wxGraphicsContext& gc, Glyph g, const wxRect& r, const wxColour& col)
{
    if (g == Glyph::None) return;

    const double w = r.width, h = r.height;
    const double cx = r.x + w / 2.0, cy = r.y + h / 2.0;
    const double s = std::min(w, h);

    auto pen = [&](double width) { gc.SetPen(wxPen(col, static_cast<int>(width))); };
    gc.SetBrush(*wxTRANSPARENT_BRUSH);

    switch (g)
    {
    case Glyph::Min: pen(1.6); gc.StrokeLine(cx - s * .28, cy, cx + s * .28, cy); break;
    case Glyph::Max: pen(1.6); gc.DrawRectangle(cx - s * .26, cy - s * .26, s * .52, s * .52); break;
    case Glyph::Restore: pen(1.6);
        gc.DrawRectangle(cx - s * .33, cy - s * .13, s * .38, s * .38);
        gc.DrawRectangle(cx - s * .10, cy - s * .30, s * .38, s * .38); break;
    case Glyph::Close: pen(1.6);
        gc.StrokeLine(cx - s * .24, cy - s * .24, cx + s * .24, cy + s * .24);
        gc.StrokeLine(cx + s * .24, cy - s * .24, cx - s * .24, cy + s * .24); break;
    case Glyph::Stop: gc.SetPen(*wxTRANSPARENT_PEN); gc.SetBrush(wxBrush(col));
        gc.DrawRectangle(cx - s * .24, cy - s * .24, s * .48, s * .48); break;
    case Glyph::Select: pen(1.5); {
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
    case Glyph::Hand: pen(1.5); {
        gc.StrokeLine(cx - s * .12, cy + s * .30, cx - s * .12, cy - s * .08);
        gc.StrokeLine(cx + s * .02, cy + s * .32, cx + s * .02, cy - s * .20);
        gc.StrokeLine(cx + s * .16, cy + s * .30, cx + s * .16, cy - s * .02);
        wxGraphicsPath p = gc.CreatePath();
        p.MoveToPoint(cx - s * .12, cy + s * .10);
        p.AddLineToPoint(cx - s * .28, cy - s * .04);
        gc.StrokePath(p);
    } break;
    case Glyph::Text: pen(1.6);
        gc.StrokeLine(cx - s * .26, cy - s * .26, cx + s * .26, cy - s * .26);
        gc.StrokeLine(cx, cy - s * .26, cx, cy + s * .28);
        gc.StrokeLine(cx - s * .14, cy + s * .28, cx + s * .14, cy + s * .28); break;
    case Glyph::Chip: pen(1.5);
        gc.DrawRectangle(cx - s * .24, cy - s * .24, s * .48, s * .48);
        gc.StrokeLine(cx - s * .12, cy - s * .24, cx - s * .12, cy - s * .34);
        gc.StrokeLine(cx + s * .12, cy - s * .24, cx + s * .12, cy - s * .34);
        gc.StrokeLine(cx - s * .12, cy + s * .24, cx - s * .12, cy + s * .34);
        gc.StrokeLine(cx + s * .12, cy + s * .24, cx + s * .12, cy + s * .34); break;
    case Glyph::Wire: pen(1.5); {
        gc.SetBrush(wxBrush(col));
        gc.DrawEllipse(cx - s * .32, cy + s * .12, s * .16, s * .16);
        gc.DrawEllipse(cx + s * .16, cy - s * .28, s * .16, s * .16);
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        wxGraphicsPath p = gc.CreatePath();
        p.MoveToPoint(cx - s * .23, cy + s * .20);
        p.AddCurveToPoint(cx + s * .02, cy + s * .15, cx + s * .06, cy - s * .06, cx + s * .23, cy - s * .19);
        gc.StrokePath(p);
    } break;
    case Glyph::Shapes: pen(1.5);
        gc.DrawRectangle(cx - s * .30, cy - s * .30, s * .34, s * .34);
        gc.DrawEllipse(cx - s * .02, cy - s * .02, s * .34, s * .34); break;
    case Glyph::Eraser: pen(1.5); {
        wxGraphicsPath p = gc.CreatePath();
        p.MoveToPoint(cx - s * .30, cy + s * .18);
        p.AddLineToPoint(cx + s * .02, cy - s * .30);
        p.AddLineToPoint(cx + s * .30, cy - s * .04);
        p.AddLineToPoint(cx - s * .02, cy + s * .26);
        p.CloseSubpath();
        gc.StrokePath(p);
        gc.StrokeLine(cx - s * .34, cy + s * .34, cx + s * .32, cy + s * .34);
    } break;
    case Glyph::Rotate: pen(1.6); {
        wxGraphicsPath p = gc.CreatePath();
        p.AddArc(cx, cy, s * .28, 0.7, 5.4);
        gc.StrokePath(p);
        gc.SetBrush(wxBrush(col));
        wxGraphicsPath a = gc.CreatePath();
        a.MoveToPoint(cx + s * .30, cy - s * .26);
        a.AddLineToPoint(cx + s * .14, cy - s * .30);
        a.AddLineToPoint(cx + s * .32, cy - s * .40);
        a.CloseSubpath();
        gc.FillPath(a);
    } break;
    case Glyph::Mirror: pen(1.5);
        gc.StrokeLine(cx, cy - s * .30, cx, cy + s * .30); {
        wxGraphicsPath l = gc.CreatePath();
        l.MoveToPoint(cx - s * .06, cy - s * .18); l.AddLineToPoint(cx - s * .28, cy);
        l.AddLineToPoint(cx - s * .06, cy + s * .18); l.CloseSubpath();
        gc.StrokePath(l);
        wxGraphicsPath rr = gc.CreatePath();
        rr.MoveToPoint(cx + s * .06, cy - s * .18); rr.AddLineToPoint(cx + s * .28, cy);
        rr.AddLineToPoint(cx + s * .06, cy + s * .18); rr.CloseSubpath();
        gc.StrokePath(rr); } break;
    case Glyph::ZoomIn: pen(1.6);
        gc.StrokeCircle(cx - s * .06, cy - s * .06, s * .24);
        gc.StrokeLine(cx - s * .18, cy - s * .06, cx + s * .06, cy - s * .06);
        gc.StrokeLine(cx - s * .06, cy - s * .18, cx - s * .06, cy + s * .06);
        gc.StrokeLine(cx + s * .12, cy + s * .12, cx + s * .30, cy + s * .30); break;
    case Glyph::ZoomOut: pen(1.6);
        gc.StrokeCircle(cx - s * .06, cy - s * .06, s * .24);
        gc.StrokeLine(cx - s * .18, cy - s * .06, cx + s * .06, cy - s * .06);
        gc.StrokeLine(cx + s * .12, cy + s * .12, cx + s * .30, cy + s * .30); break;
    case Glyph::Fit: pen(1.5);
        gc.StrokeLine(cx - s * .30, cy - s * .18, cx - s * .30, cy - s * .30);
        gc.StrokeLine(cx - s * .30, cy - s * .30, cx - s * .18, cy - s * .30);
        gc.StrokeLine(cx + s * .30, cy - s * .18, cx + s * .30, cy - s * .30);
        gc.StrokeLine(cx + s * .30, cy - s * .30, cx + s * .18, cy - s * .30);
        gc.StrokeLine(cx - s * .30, cy + s * .18, cx - s * .30, cy + s * .30);
        gc.StrokeLine(cx - s * .30, cy + s * .30, cx - s * .18, cy + s * .30);
        gc.StrokeLine(cx + s * .30, cy + s * .18, cx + s * .30, cy + s * .30);
        gc.StrokeLine(cx + s * .30, cy + s * .30, cx + s * .18, cy + s * .30); break;
    case Glyph::Play: gc.SetPen(*wxTRANSPARENT_PEN); gc.SetBrush(wxBrush(col)); {
        wxGraphicsPath p = gc.CreatePath();
        p.MoveToPoint(cx - s * .18, cy - s * .28);
        p.AddLineToPoint(cx + s * .28, cy);
        p.AddLineToPoint(cx - s * .18, cy + s * .28);
        p.CloseSubpath();
        gc.FillPath(p);
    } break;
    case Glyph::Step: pen(1.6);
        gc.StrokeLine(cx - s * .24, cy - s * .26, cx - s * .24, cy + s * .26);
        gc.SetPen(*wxTRANSPARENT_PEN); gc.SetBrush(wxBrush(col)); {
        wxGraphicsPath p = gc.CreatePath();
        p.MoveToPoint(cx - s * .14, cy - s * .26);
        p.AddLineToPoint(cx + s * .28, cy);
        p.AddLineToPoint(cx - s * .14, cy + s * .26);
        p.CloseSubpath();
        gc.FillPath(p);
    } break;
    case Glyph::Reset: pen(1.6); {
        wxGraphicsPath p = gc.CreatePath();
        p.AddArc(cx, cy, s * .28, 2.3, 5.7);
        gc.StrokePath(p);
        gc.SetPen(*wxTRANSPARENT_PEN); gc.SetBrush(wxBrush(col));
        wxGraphicsPath a = gc.CreatePath();
        a.MoveToPoint(cx - s * .30, cy - s * .22);
        a.AddLineToPoint(cx - s * .14, cy - s * .28);
        a.AddLineToPoint(cx - s * .34, cy - s * .38);
        a.CloseSubpath();
        gc.FillPath(a);
    } break;
    case Glyph::None:
    default: break;
    }

    gc.SetBrush(*wxTRANSPARENT_BRUSH);
}

/// 把菜单标题里的助记符 & 与「(&F)」去掉，只留「文件」这样的短标签。
wxString StripAmp(wxString s)
{
    s.Replace("&", "");
    const int p = s.Find('(');
    if (p != wxNOT_FOUND) s = s.Left(static_cast<size_t>(p));
    s.Trim(true);
    s.Trim(false);
    return s;
}

wxPanel* MakeSeparator(wxWindow* parent)
{
    auto* p = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(1, 24), wxBORDER_NONE);
    p->SetBackgroundColour(ui::kSeparator);
    return p;
}

}  // namespace

// -----------------------------------------------------------------------------
//  自绘按钮：窗口按钮（最小化 / 最大化 / 关闭）与工具按钮通用
// -----------------------------------------------------------------------------
class ChromeButton : public wxWindow
{
public:
    ChromeButton(wxWindow* parent, wxWindowID id, Glyph glyph, const wxString& label,
                 const wxString& tip = wxString(), bool danger = false, bool compact = false)
        : wxWindow(parent, id, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE),
          m_glyph(glyph), m_label(label), m_danger(danger), m_compact(compact)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetCursor(wxCursor(wxCURSOR_HAND));
        if (!tip.IsEmpty()) SetToolTip(tip);
        Bind(wxEVT_PAINT, &ChromeButton::OnPaint, this);
        Bind(wxEVT_ENTER_WINDOW, &ChromeButton::OnEnter, this);
        Bind(wxEVT_LEAVE_WINDOW, &ChromeButton::OnLeave, this);
        Bind(wxEVT_LEFT_DOWN, &ChromeButton::OnDown, this);
        Bind(wxEVT_LEFT_UP, &ChromeButton::OnUp, this);
    }

    void SetOnClick(std::function<void()> fn) { m_click = std::move(fn); }
    void SetActive(bool on) { if (m_active != on) { m_active = on; Refresh(); } }
    void SetActiveColour(const wxColour& c) { m_activeColour = c; }
    void SetTint(const wxColour& c) { m_tint = c; Refresh(); }

    wxSize DoGetBestSize() const override
    {
        if (m_glyph == Glyph::None)
        {
            wxClientDC dc(const_cast<ChromeButton*>(this));
            dc.SetFont(ui::Font(9));
            const wxSize ts = dc.GetTextExtent(m_label);
            return wxSize(ts.x + 18, 28);
        }
        if (m_compact) return wxSize(44, 30);
        if (m_label.IsEmpty()) return wxSize(46, 46);
        return wxSize(52, 46);
    }

private:
    void OnPaint(wxPaintEvent&)
    {
        wxAutoBufferedPaintDC dc(this);
        const wxSize sz = GetClientSize();
        const wxColour bg = GetParent() ? GetParent()->GetBackgroundColour() : ui::kChromeAlt;
        dc.SetBackground(wxBrush(bg));
        dc.Clear();

        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
        if (!gc) return;
        gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);

        if (m_active || m_hover || m_pressed)
        {
            wxColour fill;
            if (m_active) fill = m_activeColour;
            else if (m_pressed) fill = m_danger ? wxColour(0xE0, 0x2A, 0x20) : wxColour(0, 0, 0, 34);
            else fill = m_danger ? ui::kDanger : ui::kHoverFill;

            gc->SetPen(*wxTRANSPARENT_PEN);
            gc->SetBrush(wxBrush(fill));
            gc->DrawRoundedRectangle(2.5, 2.5, sz.x - 5.0, sz.y - 5.0, 8.0);
        }

        wxColour fg = m_tint;
        if (m_danger && (m_hover || m_pressed)) fg = *wxWHITE;
        else if (m_active && m_activeColour.Alpha() > 200) fg = *wxWHITE;

        if (m_glyph == Glyph::None)
        {
            gc->SetFont(ui::Font(9), fg);
            double tw = 0, th = 0;
            gc->GetTextExtent(m_label, &tw, &th);
            gc->DrawText(m_label, (sz.x - tw) / 2.0, (sz.y - th) / 2.0);
        }
        else if (m_label.IsEmpty())
        {
            DrawGlyph(*gc, m_glyph, wxRect(sz.x / 2 - 11, sz.y / 2 - 11, 22, 22), fg);
        }
        else
        {
            DrawGlyph(*gc, m_glyph, wxRect(sz.x / 2 - 10, 6, 20, 20), fg);
            gc->SetFont(ui::Font(8), fg);
            double tw = 0, th = 0;
            gc->GetTextExtent(m_label, &tw, &th);
            gc->DrawText(m_label, (sz.x - tw) / 2.0, 29);
        }
    }

    void OnEnter(wxMouseEvent&) { m_hover = true; Refresh(); }
    void OnLeave(wxMouseEvent&) { m_hover = false; m_pressed = false; Refresh(); }
    void OnDown(wxMouseEvent&) { m_pressed = true; Refresh(); }
    void OnUp(wxMouseEvent& e)
    {
        m_pressed = false;
        Refresh();
        if (GetClientRect().Contains(e.GetPosition())) Click();
    }
    void Click()
    {
        if (m_click) { m_click(); return; }
        wxCommandEvent evt(wxEVT_MENU, GetId());
        evt.SetEventObject(this);
        GetEventHandler()->ProcessEvent(evt);
    }

    Glyph m_glyph;
    wxString m_label;
    bool m_danger = false;
    bool m_compact = false;
    bool m_active = false;
    bool m_hover = false;
    bool m_pressed = false;
    wxColour m_tint = ui::kTitleText;
    wxColour m_activeColour = ui::kActiveFill;
    std::function<void()> m_click;
};

// -----------------------------------------------------------------------------
//  自绘标题栏：App 标记 + 菜单 + 文档名 + 窗口按钮 + 拖动
// -----------------------------------------------------------------------------
class AppTitleBar : public wxPanel
{
public:
    explicit AppTitleBar(wxWindow* parent)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, kTitleBarHeight), wxBORDER_NONE)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetBackgroundColour(ui::kChrome);
        Bind(wxEVT_PAINT, &AppTitleBar::OnPaint, this);
        Bind(wxEVT_LEFT_DOWN, &AppTitleBar::OnLeftDown, this);
        Bind(wxEVT_LEFT_UP, &AppTitleBar::OnLeftUp, this);
        Bind(wxEVT_MOTION, &AppTitleBar::OnMotion, this);
        Bind(wxEVT_LEFT_DCLICK, &AppTitleBar::OnDClick, this);

        m_appName = new wxStaticText(this, wxID_ANY, "SiliFlow");
        m_appName->SetFont(ui::Font(10, true));
        m_appName->SetForegroundColour(ui::kTitleText);

        m_doc = new wxStaticText(this, wxID_ANY, "未命名 · main");
        m_doc->SetFont(ui::Font(9));
        m_doc->SetForegroundColour(ui::kSecondary);

        m_winMin = new ChromeButton(this, wxID_ANY, Glyph::Min, wxEmptyString, "最小化", false, true);
        m_winMax = new ChromeButton(this, wxID_ANY, Glyph::Max, wxEmptyString, "最大化", false, true);
        m_winClose = new ChromeButton(this, wxID_ANY, Glyph::Close, wxEmptyString, "关闭", true, true);
        m_winMin->SetOnClick([this] { if (wxTopLevelWindow* f = Top()) f->Iconize(true); });
        m_winMax->SetOnClick([this] { if (wxTopLevelWindow* f = Top()) f->Maximize(!f->IsMaximized()); });
        m_winClose->SetOnClick([this] { if (wxTopLevelWindow* f = Top()) f->Close(); });

        m_row = new wxBoxSizer(wxHORIZONTAL);
        m_row->AddSpacer(34);  // 左侧留给自绘的 App 标记
        m_row->Add(m_appName, 0, wxALIGN_CENTER_VERTICAL);
        m_row->AddSpacer(10);
        m_menuStart = m_row->GetItemCount();
        m_row->AddStretchSpacer(1);
        m_row->Add(m_doc, 0, wxALIGN_CENTER_VERTICAL);
        m_row->AddSpacer(10);
        m_row->Add(m_winMin, 0, wxALIGN_CENTER_VERTICAL);
        m_row->Add(m_winMax, 0, wxALIGN_CENTER_VERTICAL);
        m_row->Add(m_winClose, 0, wxALIGN_CENTER_VERTICAL);
        m_row->AddSpacer(4);
        SetSizer(m_row);
    }

    void SetMenus(const std::vector<std::pair<wxString, wxMenu*>>& menus)
    {
        for (const auto& e : menus)
        {
            auto* b = new ChromeButton(this, wxID_ANY, Glyph::None, StripAmp(e.first), e.first);
            b->SetTint(ui::kTitleText);
            wxMenu* menu = e.second;
            b->SetOnClick(
                [this, b, menu]
                {
                    const wxPoint p = b->ClientToScreen(wxPoint(0, b->GetSize().y + 2));
                    PopupMenu(menu, p);
                });
            m_row->Insert(m_menuStart + static_cast<size_t>(m_menuCount), b, 0, wxALIGN_CENTER_VERTICAL);
            ++m_menuCount;
        }
        Layout();
    }

    void SetDocument(const wxString& doc)
    {
        m_doc->SetLabel(doc);
        Layout();
    }

private:
    wxTopLevelWindow* Top() const { return dynamic_cast<wxTopLevelWindow*>(wxGetTopLevelParent(const_cast<AppTitleBar*>(this))); }

    void OnPaint(wxPaintEvent&)
    {
        wxAutoBufferedPaintDC dc(this);
        const wxSize sz = GetClientSize();
        dc.SetBackground(wxBrush(ui::kChrome));
        dc.Clear();
        dc.SetPen(wxPen(ui::kSeparator, 1));
        dc.DrawLine(0, sz.y - 1, sz.x, sz.y - 1);

        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
        if (!gc) return;
        gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
        const double bx = 12, by = (sz.y - 18) / 2.0;
        gc->SetPen(*wxTRANSPARENT_PEN);
        gc->SetBrush(wxBrush(ui::kAccent));
        gc->DrawRoundedRectangle(bx, by, 18, 18, 5);
        gc->SetFont(ui::Font(10, true), *wxWHITE);
        double tw = 0, th = 0;
        gc->GetTextExtent("S", &tw, &th);
        gc->DrawText("S", bx + (18 - tw) / 2.0, by + (18 - th) / 2.0);
    }

    void OnLeftDown(wxMouseEvent&)
    {
        wxTopLevelWindow* f = Top();
        if (f == nullptr || f->IsMaximized()) return;
        m_dragging = true;
        m_dragStart = wxGetMousePosition();
        m_winStart = f->GetPosition();
        if (!HasCapture()) CaptureMouse();
    }
    void OnMotion(wxMouseEvent&)
    {
        if (!m_dragging) return;
        wxTopLevelWindow* f = Top();
        if (f == nullptr) return;
        f->Move(m_winStart + (wxGetMousePosition() - m_dragStart));
    }
    void OnLeftUp(wxMouseEvent&)
    {
        if (!m_dragging) return;
        m_dragging = false;
        if (HasCapture()) ReleaseMouse();
    }
    void OnDClick(wxMouseEvent&)
    {
        wxTopLevelWindow* f = Top();
        if (f != nullptr) f->Maximize(!f->IsMaximized());
    }

    wxStaticText* m_appName = nullptr;
    wxStaticText* m_doc = nullptr;
    ChromeButton* m_winMin = nullptr;
    ChromeButton* m_winMax = nullptr;
    ChromeButton* m_winClose = nullptr;
    wxBoxSizer* m_row = nullptr;
    size_t m_menuStart = 0;
    int m_menuCount = 0;
    bool m_dragging = false;
    wxPoint m_dragStart;
    wxPoint m_winStart;
};

// -----------------------------------------------------------------------------
//  自绘工具栏：工具 / 变换 / 缩放 / 仿真
// -----------------------------------------------------------------------------
class AppToolBar : public wxPanel
{
public:
    explicit AppToolBar(wxWindow* parent)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, kToolBarHeight), wxBORDER_NONE)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetBackgroundColour(ui::kChromeAlt);
        Bind(wxEVT_PAINT, &AppToolBar::OnPaint, this);

        auto* row = new wxBoxSizer(wxHORIZONTAL);
        row->AddSpacer(8);

        const int toolIds[] = { ID_TOOL_SELECT, ID_TOOL_DRAG, ID_TOOL_TEXT, ID_TOOL_COMPONENT,
                                ID_TOOL_WIRE, ID_TOOL_DRAWING, ID_TOOL_ERASER };
        const Glyph toolGlyphs[] = { Glyph::Select, Glyph::Hand, Glyph::Text, Glyph::Chip,
                                     Glyph::Wire, Glyph::Shapes, Glyph::Eraser };
        const wxString toolLabels[] = { "选择", "拖拽", "文本", "元件", "导线", "绘图", "擦除" };

        for (int i = 0; i < 7; ++i)
        {
            auto* b = new ChromeButton(this, toolIds[i], toolGlyphs[i], toolLabels[i]);
            m_toolBtns.push_back(b);
            row->Add(b, 0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, 3);
        }
        row->Add(MakeSeparator(this), 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 6);

        row->Add(new ChromeButton(this, ID_TOOL_ROTATE_CW, Glyph::Rotate, "旋转"),
                 0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, 3);
        row->Add(new ChromeButton(this, ID_TOOL_MIRROR, Glyph::Mirror, "镜像"),
                 0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, 3);
        row->Add(MakeSeparator(this), 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 6);

        row->Add(new ChromeButton(this, ID_VIEW_ZOOM_OUT, Glyph::ZoomOut, wxEmptyString, "缩小"),
                 0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, 3);
        row->Add(new ChromeButton(this, ID_VIEW_ZOOM_IN, Glyph::ZoomIn, wxEmptyString, "放大"),
                 0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, 3);
        row->Add(new ChromeButton(this, ID_VIEW_ZOOM_FIT, Glyph::Fit, wxEmptyString, "适应窗口"),
                 0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, 3);
        row->Add(MakeSeparator(this), 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 6);

        m_run = new ChromeButton(this, ID_SIM_RUN, Glyph::Play, "运行");
        m_run->SetActiveColour(palette::kLogicOne);
        row->Add(m_run, 0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, 3);
        row->Add(new ChromeButton(this, ID_SIM_STOP, Glyph::Stop, wxEmptyString, "停止"),
                 0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, 3);
        row->Add(new ChromeButton(this, ID_SIM_STEP, Glyph::Step, "单步"),
                 0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, 3);
        row->Add(new ChromeButton(this, ID_SIM_RESET, Glyph::Reset, "复位"),
                 0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, 3);

        row->AddStretchSpacer(1);
        SetSizer(row);
    }

    void SetActiveTool(int id)
    {
        for (ChromeButton* b : m_toolBtns) b->SetActive(b->GetId() == id);
    }
    void SetSimRunning(bool running)
    {
        if (m_run != nullptr) m_run->SetActive(running);
    }

private:
    void OnPaint(wxPaintEvent&)
    {
        wxAutoBufferedPaintDC dc(this);
        const wxSize sz = GetClientSize();
        dc.SetBackground(wxBrush(ui::kChromeAlt));
        dc.Clear();
        dc.SetPen(wxPen(ui::kSeparator, 1));
        dc.DrawLine(0, sz.y - 1, sz.x, sz.y - 1);
    }

    std::vector<ChromeButton*> m_toolBtns;
    ChromeButton* m_run = nullptr;
};

// =============================================================================
//  构造 / 析构
// =============================================================================
MainFrame::MainFrame(const wxString& title)
    : wxFrame(nullptr, wxID_ANY, title, wxDefaultPosition, wxSize(1440, 900),
              wxDEFAULT_FRAME_STYLE & ~wxCAPTION)  // 去掉原生标题栏，改用自绘 Apple 标题栏
{
    SetMinSize(wxSize(1024, 680));
    SetBackgroundColour(ui::kContent);

    if (!m_ctx.Initialize())
    {
        wxMessageBox("元件库初始化失败，程序无法继续。", "SiliFlow", wxOK | wxICON_ERROR);
    }

    SetupUi();
    SetupStatusBar();
    WireServices();
    BindMenuHandlers();
    SetupAuiLayout();

    // 自动节拍定时器（S1 只有组合逻辑，跑起来主要是把 tick 计数推着走）
    m_simTimer.SetOwner(this);
    Bind(wxEVT_TIMER, &MainFrame::OnSimTimer, this);

    RebindSim();
    UpdateTitle();
    UpdateStatusBar();
    UpdateUndoMenuState();
    SyncToolBar();
    SyncSimButtons();

    m_ctx.Log("SiliFlow 已启动。");
    m_ctx.Log("用法：左栏选元件 → 画布单击放置；W 切导线工具后点两个引脚即可连线。");
    m_ctx.Log("仿真：S 工具下单击输入引脚 / 常量 / 指示灯可翻转电平；Ctrl+T 单步，Ctrl+Shift+T 复位。");
    m_ctx.Log(wxString::Format("内置元件库：%zu 个元件定义。", m_ctx.Lib() != nullptr ? m_ctx.Lib()->Size() : 0));
}

MainFrame::~MainFrame()
{
    // 先让 wxAUI 收尾，再显式销毁子窗口：
    // 这样面板会在 AppContext 之前析构，避免 Services 里的指针被短暂悬空。
    m_aui.UnInit();
    DestroyChildren();

    // 标题栏承载的菜单由本窗口持有，需显式释放。
    for (wxMenu* menu : m_menus) delete menu;
    m_menus.clear();
}

// =============================================================================
//  装配
// =============================================================================
void MainFrame::SetupUi()
{
    m_aui.SetManagedWindow(this);

    // ---- 扁平 Apple 停靠外观：无渐变、细分割线、面板白底 ----
    wxAuiDockArt* art = m_aui.GetArtProvider();
    art->SetMetric(wxAUI_DOCKART_PANE_BORDER_SIZE, 0);
    art->SetMetric(wxAUI_DOCKART_SASH_SIZE, 6);
    art->SetMetric(wxAUI_DOCKART_CAPTION_SIZE, 26);
    art->SetMetric(wxAUI_DOCKART_GRADIENT_TYPE, wxAUI_GRADIENT_NONE);
    art->SetColour(wxAUI_DOCKART_BACKGROUND_COLOUR, ui::kContent);
    art->SetColour(wxAUI_DOCKART_SASH_COLOUR, ui::kContent);
    art->SetColour(wxAUI_DOCKART_BORDER_COLOUR, ui::kSeparator);
    art->SetColour(wxAUI_DOCKART_GRIPPER_COLOUR, ui::kSeparator);
    art->SetColour(wxAUI_DOCKART_INACTIVE_CAPTION_COLOUR, ui::kPanel);
    art->SetColour(wxAUI_DOCKART_INACTIVE_CAPTION_GRADIENT_COLOUR, ui::kPanel);
    art->SetColour(wxAUI_DOCKART_INACTIVE_CAPTION_TEXT_COLOUR, ui::kSecondary);
    art->SetColour(wxAUI_DOCKART_ACTIVE_CAPTION_COLOUR, ui::kPanel);
    art->SetColour(wxAUI_DOCKART_ACTIVE_CAPTION_GRADIENT_COLOUR, ui::kPanel);
    art->SetColour(wxAUI_DOCKART_ACTIVE_CAPTION_TEXT_COLOUR, ui::kTitleText);
    art->SetFont(wxAUI_DOCKART_CAPTION_FONT, ui::Font(9));

    m_notebook = new CanvasNoteBook(this, &m_ctx);
    m_toolbox = new ToolboxPanel(this, &m_ctx);
    m_property = new PropertyPanel(this, &m_ctx);
    m_tree = new TreePanel(this, &m_ctx);

    // 底部宿主：左侧波形、右侧消息。
    // 注意两个子面板的父窗口必须是宿主面板本身 —— sizer 只接受宿主窗口的子窗口。
    m_bottomHost = new wxPanel(this);
    m_wave = new WavePanel(m_bottomHost, &m_ctx);
    m_console = new ConsolePanel(m_bottomHost);

    auto* hostSizer = new wxBoxSizer(wxHORIZONTAL);
    hostSizer->Add(m_wave, 3, wxEXPAND);
    hostSizer->Add(m_console, 2, wxEXPAND);
    m_bottomHost->SetSizer(hostSizer);

    // Apple 自定义外壳：自绘标题栏（内含弹出菜单）+ 工具栏。取代原生菜单栏。
    SetupAppChrome();

    // 元件库面板选中元件 → 在活动画布上进入放置模式
    m_toolbox->SetPickHandler(
        [this](const wxString& type)
        {
            m_ctx.BeginPlacement(type);
        });

    m_notebook->RebuildFromProject();
    m_property->RefreshFromSelection();
}

void MainFrame::SetupAppChrome()
{
    // 把原生菜单“搬”到自绘标题栏：创建菜单栏后逐个取回顶层菜单（取得所有权），
    // 由标题栏上的文字按钮弹出。功能、快捷键、更新逻辑全部保持不变。
    wxMenuBar* bar = CreateMainMenuBar();
    std::vector<std::pair<wxString, wxMenu*>> menus;
    while (bar->GetMenuCount() > 0)
    {
        menus.emplace_back(bar->GetLabelTop(0), bar->Remove(static_cast<size_t>(0)));
    }
    delete bar;

    m_titleBar = new AppTitleBar(this);
    m_titleBar->SetMenus(menus);
    for (auto& e : menus) m_menus.push_back(e.second);

    m_toolBar = new AppToolBar(this);
}

void MainFrame::SyncToolBar()
{
    if (m_toolBar == nullptr) return;

    int id = ID_TOOL_SELECT;
    if (CanvasPanel* c = ActiveCanvas())
    {
        switch (c->Tools().Tool())
        {
        case ToolType::Drag:      id = ID_TOOL_DRAG; break;
        case ToolType::Select:    id = ID_TOOL_SELECT; break;
        case ToolType::Text:      id = ID_TOOL_TEXT; break;
        case ToolType::Component: id = ID_TOOL_COMPONENT; break;
        case ToolType::Wire:      id = ID_TOOL_WIRE; break;
        case ToolType::Drawing:   id = ID_TOOL_DRAWING; break;
        case ToolType::Eraser:    id = ID_TOOL_ERASER; break;
        }
    }
    m_toolBar->SetActiveTool(id);
}

void MainFrame::SyncSimButtons()
{
    if (m_toolBar != nullptr) m_toolBar->SetSimRunning(m_ctx.Sim().IsRunning());
}

void MainFrame::SetupAuiLayout()
{
    // 顶部：自绘标题栏 + 工具栏（不可停靠 / 无边框，固定在顶端）
    m_aui.AddPane(m_titleBar, wxAuiPaneInfo()
                                  .Name("titlebar").CaptionVisible(false)
                                  .Top().Layer(10).Position(0)
                                  .Dockable(false).Movable(false).Floatable(false)
                                  .Gripper(false).CloseButton(false).PaneBorder(false)
                                  .MinSize(wxSize(-1, kTitleBarHeight))
                                  .BestSize(wxSize(-1, kTitleBarHeight))
                                  .MaxSize(wxSize(-1, kTitleBarHeight)));

    m_aui.AddPane(m_toolBar, wxAuiPaneInfo()
                                 .Name("toolbar").CaptionVisible(false)
                                 .Top().Layer(10).Position(1)
                                 .Dockable(false).Movable(false).Floatable(false)
                                 .Gripper(false).CloseButton(false).PaneBorder(false)
                                 .MinSize(wxSize(-1, kToolBarHeight))
                                 .BestSize(wxSize(-1, kToolBarHeight))
                                 .MaxSize(wxSize(-1, kToolBarHeight)));

    m_aui.AddPane(m_toolbox, wxAuiPaneInfo()
                                 .Name("toolbox")
                                 .Caption("元件库")
                                 .Left()
                                 .Layer(1)
                                 .Position(0)
                                 .MinSize(200, 200)
                                 .BestSize(250, 700)
                                 .CloseButton(false));

    m_aui.AddPane(m_notebook, wxAuiPaneInfo().Name("canvas").Caption("原理图").CenterPane().PaneBorder(false));

    m_aui.AddPane(m_property, wxAuiPaneInfo()
                                  .Name("property")
                                  .Caption("属性")
                                  .Right()
                                  .Layer(1)
                                  .Position(0)
                                  .MinSize(240, 180)
                                  .BestSize(320, 460));

    m_aui.AddPane(m_tree, wxAuiPaneInfo()
                              .Name("tree")
                              .Caption("层次树")
                              .Right()
                              .Layer(1)
                              .Position(1)
                              .MinSize(240, 140)
                              .BestSize(320, 380));

    m_aui.AddPane(m_bottomHost, wxAuiPaneInfo()
                                   .Name("bottom")
                                   .Caption("波形 / 消息")
                                   .Bottom()
                                   .Layer(1)
                                   .Position(0)
                                   .MinSize(500, 150)
                                   .BestSize(1440, 280));

    m_aui.Update();
}

void MainFrame::SetupStatusBar()
{
    CreateStatusBar(4);
    const int widths[4] = {-2, -2, -2, -3};
    SetStatusWidths(4, widths);

    if (wxStatusBar* sb = GetStatusBar())
    {
        sb->SetBackgroundColour(ui::kChrome);
        sb->SetForegroundColour(ui::kSecondary);
        sb->SetFont(ui::Font(9));
    }

    SetStatusText("工具：选择", 0);
    SetStatusText("缩放 100%", 1);
    SetStatusText("元件 0  导线 0", 2);
    SetStatusText("仿真：未附着图纸", 3);
}

// =============================================================================
//  Services 接线
// =============================================================================
void MainFrame::WireServices()
{
    m_ctx.log = [this](const wxString& msg)
    {
        if (m_console != nullptr) m_console->Append(msg);
    };

    m_ctx.onRequestRefresh = [this]
    {
        if (CanvasPanel* c = ActiveCanvas()) c->Refresh(false);
    };

    m_ctx.onToolStateChanged = [this]
    {
        SyncToolBar();
        UpdateStatusBar();
    };

    m_ctx.onStatusText = [this](const wxString& text)
    {
        if (GetStatusBar() != nullptr) SetStatusText(text, 1);
    };

    m_ctx.onSheetChanged = [this]
    {
        // 图纸改了 → 求值图作废，重算一次再刷新视图
        m_ctx.Sim().MarkDirty();
        m_ctx.Sim().Evaluate();
        UpdateTitle();
        if (m_tree != nullptr) m_tree->Rebuild();
        UpdateStatusBar();
        RefreshSimViews();
    };

    m_ctx.onSelectionChanged = [this]
    {
        if (m_property != nullptr) m_property->RefreshFromSelection();
        UpdateStatusBar();
    };

    m_ctx.onActiveSheetChanged = [this]
    {
        RebindSim();
        if (m_property != nullptr) m_property->RefreshFromSelection();
        if (m_tree != nullptr) m_tree->Rebuild();
        SyncToolBar();
        UpdateTitle();
        UpdateStatusBar();
    };

    m_ctx.onUndoStateChanged = [this] { UpdateUndoMenuState(); };

    m_ctx.onSimStateChanged = [this]
    {
        SyncSimButtons();
        RefreshSimViews();
    };

    m_ctx.onSimStatus = [this](const wxString& text)
    {
        if (GetStatusBar() != nullptr) SetStatusText(text, 3);
    };

    // ---- 编辑命令 → 当前活动画布 ----
    m_ctx.cmdBeginPlacement = [this](const wxString& type)
    {
        if (CanvasEventHandler* h = HandlerOf(ActiveCanvas())) h->BeginPlacement(type);
    };

    m_ctx.cmdRotate = [this]
    {
        if (CanvasEventHandler* h = HandlerOf(ActiveCanvas())) h->RotateSelection(1);
    };

    m_ctx.cmdMirror = [this]
    {
        if (CanvasEventHandler* h = HandlerOf(ActiveCanvas())) h->MirrorSelection();
    };

    m_ctx.cmdDelete = [this]
    {
        if (CanvasEventHandler* h = HandlerOf(ActiveCanvas())) h->DeleteSelection();
    };

    m_ctx.cmdSelectAll = [this]
    {
        if (CanvasEventHandler* h = HandlerOf(ActiveCanvas())) h->SelectAll();
    };

    m_ctx.editComponent = [this](int compId, const wxString& prop, const wxString& value)
    {
        if (CanvasEventHandler* h = HandlerOf(ActiveCanvas())) h->ApplyComponentProperty(compId, prop, value);
    };

    m_ctx.editWire = [this](int wireId, const wxString& prop, const wxString& value)
    {
        if (CanvasEventHandler* h = HandlerOf(ActiveCanvas())) h->ApplyWireProperty(wireId, prop, value);
    };

    m_ctx.cmdHighlightNet = [this](const wxString& netName)
    {
        if (CanvasEventHandler* h = HandlerOf(ActiveCanvas())) h->HighlightNet(netName);
    };

    m_ctx.cmdFocusComponent = [this](int compId)
    {
        if (CanvasEventHandler* h = HandlerOf(ActiveCanvas())) h->FocusComponent(compId);
    };

    m_ctx.cmdActivateSheet = [this](const wxString& sheetName)
    {
        if (m_notebook != nullptr) m_notebook->SetActiveSheet(sheetName);
    };

    // ---- 仿真命令 ----
    m_ctx.cmdSimRun = [this] { ApplySimRunning(true); };
    m_ctx.cmdSimStop = [this] { ApplySimRunning(false); };

    m_ctx.cmdSimStep = [this]
    {
        m_ctx.Sim().Tick();
        RefreshSimViews();
    };

    m_ctx.cmdSimReset = [this]
    {
        m_ctx.Sim().Reset();
        m_ctx.Log("仿真已复位（强制电平与 tick 计数都清空）。");
        RefreshSimViews();
    };

    m_ctx.cmdSimTogglePin = [this](int compId)
    {
        if (!m_ctx.Sim().TogglePin(compId)) return;

        const sim::SimEngine& eng = m_ctx.Sim();
        const CircuitSheet* sheet = ActiveSheet();
        const ComponentInstance* c = sheet == nullptr ? nullptr : sheet->FindComponent(compId);

        m_ctx.Log(wxString::Format("翻转 %s → %s",
                                   c == nullptr ? wxString::Format("#%d", compId)
                                                : (c->label.IsEmpty() ? c->defType : c->label),
                                   LogicSignalName(eng.PinSignal(compId, 0))));
        RefreshSimViews();
    };

    m_ctx.cmdSimForcePin = [this](int compId, const wxString& level)
    {
        sim::SimEngine& eng = m_ctx.Sim();

        if (level.IsEmpty())
            eng.ClearForced(compId);
        else
            eng.SetForced(compId, LogicSignalFromName(level, LogicSignal::Zero));

        RefreshSimViews();

        // 属性面板此刻正在处理网格事件，直接重建网格不安全 —— 推迟到下一轮事件循环
        CallAfter(
            [this]
            {
                if (m_property != nullptr) m_property->RefreshFromSelection();
            });
    };
}

// =============================================================================
//  菜单绑定
// =============================================================================
void MainFrame::BindMenuHandlers()
{
    Bind(wxEVT_MENU, &MainFrame::OnFileNew, this, wxID_NEW);
    Bind(wxEVT_MENU, &MainFrame::OnFileOpen, this, wxID_OPEN);
    Bind(wxEVT_MENU, &MainFrame::OnFileSave, this, wxID_SAVE);
    Bind(wxEVT_MENU, &MainFrame::OnFileSaveAs, this, wxID_SAVEAS);
    Bind(wxEVT_MENU, &MainFrame::OnFileOpenVcd, this, ID_FILE_OPEN_VCD);
    Bind(wxEVT_MENU, &MainFrame::OnFileExit, this, wxID_EXIT);

    Bind(wxEVT_MENU, &MainFrame::OnEditUndo, this, wxID_UNDO);
    Bind(wxEVT_MENU, &MainFrame::OnEditRedo, this, wxID_REDO);
    Bind(wxEVT_MENU, &MainFrame::OnEditDelete, this, wxID_DELETE);
    Bind(wxEVT_MENU, &MainFrame::OnEditSelectAll, this, wxID_SELECTALL);
    Bind(wxEVT_MENU, &MainFrame::OnEditNotImplemented, this, wxID_CUT);
    Bind(wxEVT_MENU, &MainFrame::OnEditNotImplemented, this, wxID_COPY);
    Bind(wxEVT_MENU, &MainFrame::OnEditNotImplemented, this, wxID_PASTE);
    Bind(wxEVT_MENU, &MainFrame::OnEditNotImplemented, this, wxID_PREFERENCES);

    Bind(wxEVT_MENU, &MainFrame::OnViewZoomIn, this, ID_VIEW_ZOOM_IN);
    Bind(wxEVT_MENU, &MainFrame::OnViewZoomOut, this, ID_VIEW_ZOOM_OUT);
    Bind(wxEVT_MENU, &MainFrame::OnViewZoomFit, this, ID_VIEW_ZOOM_FIT);
    Bind(wxEVT_MENU, &MainFrame::OnViewZoomReset, this, ID_VIEW_ZOOM_RESET);
    Bind(wxEVT_MENU, &MainFrame::OnViewToggleGrid, this, ID_VIEW_SHOW_GRID);
    Bind(wxEVT_MENU, &MainFrame::OnViewTogglePinNames, this, ID_VIEW_SHOW_PIN_NAMES);
    Bind(wxEVT_MENU, &MainFrame::OnViewToggleLabels, this, ID_VIEW_SHOW_LABELS);
    Bind(wxEVT_MENU, &MainFrame::OnViewTogglePanel, this, ID_PANEL_TOOLBOX);
    Bind(wxEVT_MENU, &MainFrame::OnViewTogglePanel, this, ID_PANEL_PROPERTY);
    Bind(wxEVT_MENU, &MainFrame::OnViewTogglePanel, this, ID_PANEL_TREE);
    Bind(wxEVT_MENU, &MainFrame::OnViewTogglePanel, this, ID_PANEL_WAVE);
    Bind(wxEVT_MENU, &MainFrame::OnViewTogglePanel, this, ID_PANEL_CONSOLE);
    Bind(wxEVT_UPDATE_UI, &MainFrame::OnUpdateView, this, ID_VIEW_SHOW_GRID);
    Bind(wxEVT_UPDATE_UI, &MainFrame::OnUpdateView, this, ID_VIEW_SHOW_PIN_NAMES);
    Bind(wxEVT_UPDATE_UI, &MainFrame::OnUpdateView, this, ID_VIEW_SHOW_LABELS);

    for (int id = ID_TOOL_DRAG; id <= ID_TOOL_ERASER; ++id)
    {
        Bind(wxEVT_MENU, &MainFrame::OnToolSelect, this, id);
        Bind(wxEVT_UPDATE_UI, &MainFrame::OnUpdateTool, this, id);
    }

    Bind(wxEVT_MENU, &MainFrame::OnDrawKindSelect, this, ID_DRAW_LINE);
    Bind(wxEVT_MENU, &MainFrame::OnDrawKindSelect, this, ID_DRAW_RECTANGLE);
    Bind(wxEVT_MENU, &MainFrame::OnDrawKindSelect, this, ID_DRAW_CIRCLE);

    Bind(wxEVT_MENU, &MainFrame::OnToolRotate, this, ID_TOOL_ROTATE_CW);
    Bind(wxEVT_MENU, &MainFrame::OnToolRotate, this, ID_TOOL_ROTATE_CCW);
    Bind(wxEVT_MENU, &MainFrame::OnToolMirror, this, ID_TOOL_MIRROR);

    Bind(wxEVT_MENU, &MainFrame::OnSheetNew, this, ID_SHEET_NEW);
    Bind(wxEVT_MENU, &MainFrame::OnSheetClose, this, ID_SHEET_CLOSE);

    Bind(wxEVT_MENU, &MainFrame::OnSimRun, this, ID_SIM_RUN);
    Bind(wxEVT_MENU, &MainFrame::OnSimStop, this, ID_SIM_STOP);
    Bind(wxEVT_MENU, &MainFrame::OnSimStep, this, ID_SIM_STEP);
    Bind(wxEVT_MENU, &MainFrame::OnSimReset, this, ID_SIM_RESET);
    Bind(wxEVT_MENU, &MainFrame::OnSimFreq, this, ID_SIM_FREQ_1, ID_SIM_FREQ_20);
    Bind(wxEVT_MENU, &MainFrame::OnSimTruthTable, this, ID_SIM_TRUTH_TABLE);
    Bind(wxEVT_UPDATE_UI, &MainFrame::OnUpdateSim, this, ID_SIM_RUN);
    Bind(wxEVT_UPDATE_UI, &MainFrame::OnUpdateSim, this, ID_SIM_STOP);
    Bind(wxEVT_UPDATE_UI, &MainFrame::OnUpdateSim, this, ID_SIM_FREQ_1, ID_SIM_FREQ_20);
    Bind(wxEVT_UPDATE_UI, &MainFrame::OnUpdateSim, this, ID_SIM_STEP);
    Bind(wxEVT_UPDATE_UI, &MainFrame::OnUpdateSim, this, ID_SIM_RESET);

    Bind(wxEVT_MENU, &MainFrame::OnVerilogExport, this, ID_VERILOG_EXPORT);
    Bind(wxEVT_MENU, &MainFrame::OnVerilogImport, this, ID_VERILOG_IMPORT);
    Bind(wxEVT_MENU, &MainFrame::OnVerilogSyncStatus, this, ID_VERILOG_SYNC_STATUS);
    Bind(wxEVT_MENU, &MainFrame::OnVerilogOpenFile, this, ID_VERILOG_OPEN_FILE);

    Bind(wxEVT_MENU, &MainFrame::OnHelpShortcuts, this, ID_HELP_SHORTCUTS);
    Bind(wxEVT_MENU, &MainFrame::OnHelpSelfCheck, this, ID_HELP_SELFTEST);
    Bind(wxEVT_MENU, &MainFrame::OnAbout, this, wxID_ABOUT);

    Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnClose, this);
}

// =============================================================================
//  辅助
// =============================================================================
CanvasPanel* MainFrame::ActiveCanvas() const
{
    return m_notebook == nullptr ? nullptr : m_notebook->ActiveCanvas();
}

CircuitSheet* MainFrame::ActiveSheet() const
{
    return m_ctx.Proj() == nullptr ? nullptr : m_ctx.Proj()->Active();
}

void MainFrame::UpdateTitle()
{
    const wxString projectName = m_ctx.Proj() != nullptr ? m_ctx.Proj()->DisplayName() : wxString("无工程");
    const wxString sheetName = ActiveSheet() != nullptr ? ActiveSheet()->name : wxString("-");
    SetTitle(wxString::Format("%s  ·  %s  —  SiliFlow", projectName, sheetName));
    if (m_titleBar != nullptr) m_titleBar->SetDocument(projectName + "  ·  " + sheetName);
}

void MainFrame::UpdateStatusBar()
{
    if (GetStatusBar() == nullptr) return;

    CanvasPanel* canvas = ActiveCanvas();
    SetStatusText(
        wxString::Format("工具：%s", canvas != nullptr ? canvas->Tools().DisplayText() : wxString("—")), 0);

    CircuitSheet* sheet = ActiveSheet();
    if (sheet != nullptr)
        SetStatusText(wxString::Format("元件 %zu  导线 %zu  选中 %zu", sheet->components.size(),
                                       sheet->wires.size(), sheet->SelectionCount()),
                      2);
    else
        SetStatusText("—", 2);
}

void MainFrame::UpdateUndoMenuState()
{
    if (wxMenuBar* bar = GetMenuBar())
    {
        const wxString undoLabel = m_ctx.Undo().CanUndo()
                                       ? wxString::Format("撤销 %s\tCtrl+Z", m_ctx.Undo().UndoName())
                                       : wxString("撤销\tCtrl+Z");
        const wxString redoLabel = m_ctx.Undo().CanRedo()
                                       ? wxString::Format("重做 %s\tCtrl+Y", m_ctx.Undo().RedoName())
                                       : wxString("重做\tCtrl+Y");

        if (wxMenuItem* item = bar->FindItem(wxID_UNDO)) item->SetItemLabel(undoLabel);
        if (wxMenuItem* item = bar->FindItem(wxID_REDO)) item->SetItemLabel(redoLabel);
    }
}

void MainFrame::RefreshAllPanels()
{
    m_property->RefreshFromSelection();
    m_tree->Rebuild();
    UpdateTitle();
    UpdateStatusBar();
}

void MainFrame::RebuildNotebook()
{
    if (m_notebook == nullptr) return;
    m_notebook->RebuildFromProject();
}

bool MainFrame::ConfirmDiscardChanges()
{
    Project* project = m_ctx.Proj();
    if (project == nullptr || !project->dirty) return true;

    const int answer =
        wxMessageBox("当前工程有未保存的改动，是否先保存？", "SiliFlow", wxYES_NO | wxCANCEL | wxICON_QUESTION);

    if (answer == wxCANCEL) return false;
    if (answer == wxNO) return true;

    wxCommandEvent dummy;
    OnFileSave(dummy);
    return true;
}

// =============================================================================
//  仿真
// =============================================================================
void MainFrame::RebindSim()
{
    sim::SimEngine& eng = m_ctx.Sim();

    eng.Attach(ActiveSheet());
    eng.SetRunning(false);
    m_simTimer.Stop();
    eng.Evaluate();

    RefreshSimViews();
}

void MainFrame::RefreshSimViews()
{
    if (CanvasPanel* c = ActiveCanvas()) c->Refresh(false);

    if (GetStatusBar() == nullptr) return;

    const sim::SimEngine& eng = m_ctx.Sim();
    SetStatusText(eng.IsAttached() ? eng.StatusText() : wxString("仿真：未附着图纸"), 3);
}

void MainFrame::SetSimClockHz(int hz)
{
    m_simHz = (hz < 1) ? 1 : ((hz > 20) ? 20 : hz);

    if (m_ctx.Sim().IsRunning()) m_simTimer.Start(1000 / m_simHz);
    RefreshSimViews();
}

void MainFrame::ApplySimRunning(bool on)
{
    sim::SimEngine& eng = m_ctx.Sim();

    if (on && !eng.IsAttached())
    {
        m_ctx.Log("没有活动图纸，无法运行仿真。");
        return;
    }
    if (on == eng.IsRunning()) return;

    eng.SetRunning(on);

    if (on)
    {
        m_simTimer.Start(1000 / m_simHz);
        m_ctx.Log(wxString::Format("仿真运行，节拍 %d Hz。", m_simHz));
    }
    else
    {
        m_simTimer.Stop();
        m_ctx.Log("仿真已停止。");
    }

    RefreshSimViews();
}

void MainFrame::OnSimRun(wxCommandEvent& WXUNUSED(event))
{
    ApplySimRunning(true);
}

void MainFrame::OnSimStop(wxCommandEvent& WXUNUSED(event))
{
    ApplySimRunning(false);
}

void MainFrame::OnSimStep(wxCommandEvent& WXUNUSED(event))
{
    sim::SimEngine& eng = m_ctx.Sim();
    if (!eng.IsAttached())
    {
        m_ctx.Log("没有活动图纸，无法单步。");
        return;
    }

    eng.Tick();
    RefreshSimViews();
}

void MainFrame::OnSimReset(wxCommandEvent& WXUNUSED(event))
{
    m_ctx.Sim().Reset();
    m_ctx.Log("仿真已复位（强制电平与 tick 计数都清空）。");
    RefreshSimViews();
}

void MainFrame::OnSimFreq(wxCommandEvent& event)
{
    const int hz = SimHzForMenuId(event.GetId());
    SetSimClockHz(hz);
    m_ctx.Log(wxString::Format("节拍频率设为 %d Hz。", hz));
}

void MainFrame::OnSimTruthTable(wxCommandEvent& WXUNUSED(event))
{
    wxMessageBox("真值表面板属于 S3 里程碑，还没实现。\n\n"
                 "届时流程是：先在图纸上放好 input / output 引脚，"
                 "点这里穷举 2ⁿ 组合，出表后可以导出 CSV。",
                 "SiliFlow", wxOK | wxICON_INFORMATION, this);
}

void MainFrame::OnSimTimer(wxTimerEvent& WXUNUSED(event))
{
    sim::SimEngine& eng = m_ctx.Sim();
    if (!eng.IsRunning())
    {
        m_simTimer.Stop();
        return;
    }

    eng.Tick();
    RefreshSimViews();
}

void MainFrame::OnUpdateSim(wxUpdateUIEvent& event)
{
    const sim::SimEngine& eng = m_ctx.Sim();
    const int id = event.GetId();

    if (id == ID_SIM_RUN)
    {
        event.Enable(eng.IsAttached() && !eng.IsRunning());
        return;
    }
    if (id == ID_SIM_STOP)
    {
        event.Enable(eng.IsRunning());
        return;
    }
    if (id >= ID_SIM_FREQ_1 && id <= ID_SIM_FREQ_20)
    {
        event.Check(m_simHz == SimHzForMenuId(id));
        return;
    }

    event.Enable(eng.IsAttached());
}

// =============================================================================
//  文件
// =============================================================================
void MainFrame::OnFileNew(wxCommandEvent& WXUNUSED(event))
{
    if (!ConfirmDiscardChanges()) return;

    m_ctx.NewProject();
    RebuildNotebook();
    RefreshAllPanels();
    m_ctx.Log("新工程已就绪。");
}

void MainFrame::OnFileOpen(wxCommandEvent& WXUNUSED(event))
{
    if (!ConfirmDiscardChanges()) return;

    wxFileDialog dlg(this, "打开工程", wxEmptyString, wxEmptyString,
                     "SiliFlow 工程 (*.sfproj)|*.sfproj|所有文件 (*.*)|*.*",
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK) return;

    wxString err;
    if (!m_ctx.OpenProject(dlg.GetPath(), &err))
    {
        wxMessageBox("打开失败：" + err, "SiliFlow", wxOK | wxICON_ERROR);
        return;
    }

    RebuildNotebook();
    RefreshAllPanels();
}

void MainFrame::OnFileSave(wxCommandEvent& WXUNUSED(event))
{
    if (m_ctx.Proj() == nullptr) return;

    if (!m_ctx.HasProjectPath())
    {
        wxCommandEvent dummy;
        OnFileSaveAs(dummy);
        return;
    }

    wxString err;
    if (!m_ctx.SaveProject(&err)) wxMessageBox("保存失败：" + err, "SiliFlow", wxOK | wxICON_ERROR);

    UpdateTitle();
}

void MainFrame::OnFileSaveAs(wxCommandEvent& WXUNUSED(event))
{
    if (m_ctx.Proj() == nullptr) return;

    wxFileDialog dlg(this, "另存为", wxEmptyString,
                     m_ctx.Proj()->name.IsEmpty() ? wxString("untitled.sfproj")
                                                  : m_ctx.Proj()->name + ".sfproj",
                     "SiliFlow 工程 (*.sfproj)|*.sfproj", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() != wxID_OK) return;

    wxString err;
    if (!m_ctx.SaveProjectTo(dlg.GetPath(), &err))
        wxMessageBox("保存失败：" + err, "SiliFlow", wxOK | wxICON_ERROR);

    UpdateTitle();
}

void MainFrame::OnFileOpenVcd(wxCommandEvent& WXUNUSED(event))
{
    wxFileDialog dlg(this, "打开 VCD 波形", wxEmptyString, wxEmptyString,
                     "VCD 波形 (*.vcd)|*.vcd|所有文件 (*.*)|*.*", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK) return;

    m_ctx.Log("波形查看属于 M5 里程碑：面板与布局已就位，解析与绘制尚未接入。文件：" + dlg.GetPath());
}

void MainFrame::OnFileExit(wxCommandEvent& WXUNUSED(event))
{
    Close(false);
}

// =============================================================================
//  编辑
// =============================================================================
void MainFrame::OnEditUndo(wxCommandEvent& WXUNUSED(event))
{
    CircuitSheet* sheet = ActiveSheet();
    if (sheet == nullptr) return;

    const wxString name = m_ctx.Undo().UndoName();
    if (!m_ctx.Undo().Undo(*sheet)) return;

    m_ctx.Log("撤销：" + name);
    if (CanvasPanel* c = ActiveCanvas())
    {
        c->ClearHover();
        c->Refresh(false);
    }
    m_ctx.NotifySheetChanged();
    m_ctx.NotifySelectionChanged();
    m_ctx.NotifyUndoStateChanged();
}

void MainFrame::OnEditRedo(wxCommandEvent& WXUNUSED(event))
{
    CircuitSheet* sheet = ActiveSheet();
    if (sheet == nullptr) return;

    const wxString name = m_ctx.Undo().RedoName();
    if (!m_ctx.Undo().Redo(*sheet)) return;

    m_ctx.Log("重做：" + name);
    if (CanvasPanel* c = ActiveCanvas())
    {
        c->ClearHover();
        c->Refresh(false);
    }
    m_ctx.NotifySheetChanged();
    m_ctx.NotifySelectionChanged();
    m_ctx.NotifyUndoStateChanged();
}

void MainFrame::OnEditDelete(wxCommandEvent& WXUNUSED(event))
{
    m_ctx.Delete();
}

void MainFrame::OnEditSelectAll(wxCommandEvent& WXUNUSED(event))
{
    m_ctx.SelectAll();
}

void MainFrame::OnEditNotImplemented(wxCommandEvent& event)
{
    wxUnusedVar(event);
    m_ctx.Log("该功能尚未实现（骨架版暂只提供删除与全选）。");
}

// =============================================================================
//  视图
// =============================================================================
void MainFrame::OnViewZoomIn(wxCommandEvent& WXUNUSED(event))
{
    if (CanvasPanel* c = ActiveCanvas())
    {
        const wxSize size = c->GetClientSize();
        c->ZoomBy(1.2, Point(size.x / 2, size.y / 2));
    }
}

void MainFrame::OnViewZoomOut(wxCommandEvent& WXUNUSED(event))
{
    if (CanvasPanel* c = ActiveCanvas())
    {
        const wxSize size = c->GetClientSize();
        c->ZoomBy(1.0 / 1.2, Point(size.x / 2, size.y / 2));
    }
}

void MainFrame::OnViewZoomFit(wxCommandEvent& WXUNUSED(event))
{
    if (CanvasPanel* c = ActiveCanvas()) c->ZoomFit();
}

void MainFrame::OnViewZoomReset(wxCommandEvent& WXUNUSED(event))
{
    if (CanvasPanel* c = ActiveCanvas()) c->ZoomReset();
}

void MainFrame::OnViewToggleGrid(wxCommandEvent& event)
{
    if (CanvasPanel* c = ActiveCanvas())
    {
        c->Options().showGrid = event.IsChecked();
        c->Refresh(false);
    }
}

void MainFrame::OnViewTogglePinNames(wxCommandEvent& event)
{
    if (CanvasPanel* c = ActiveCanvas())
    {
        c->Options().showPinNames = event.IsChecked();
        c->Refresh(false);
    }
}

void MainFrame::OnViewToggleLabels(wxCommandEvent& event)
{
    if (CanvasPanel* c = ActiveCanvas())
    {
        c->Options().showLabels = event.IsChecked();
        c->Refresh(false);
    }
}

void MainFrame::OnViewTogglePanel(wxCommandEvent& event)
{
    const bool show = event.IsChecked();
    wxWindow* target = nullptr;
    bool isBottomChild = false;

    switch (event.GetId())
    {
    case ID_PANEL_TOOLBOX:  target = m_toolbox; break;
    case ID_PANEL_PROPERTY: target = m_property; break;
    case ID_PANEL_TREE:     target = m_tree; break;
    case ID_PANEL_WAVE:     target = m_wave; isBottomChild = true; break;
    case ID_PANEL_CONSOLE:  target = m_console; isBottomChild = true; break;
    default: return;
    }

    if (target == nullptr) return;
    target->Show(show);

    if (isBottomChild)
    {
        if (m_bottomHost != nullptr)
        {
            m_bottomHost->Layout();
            // 两个子面板都藏起来时，把整个底部窗格也收掉
            const bool anyVisible = m_wave->IsShown() || m_console->IsShown();
            m_aui.GetPane(m_bottomHost).Show(anyVisible);
        }
    }
    else
    {
        m_aui.GetPane(target).Show(show);
    }

    m_aui.Update();
}

void MainFrame::OnUpdateView(wxUpdateUIEvent& event)
{
    CanvasPanel* c = ActiveCanvas();
    if (c == nullptr)
    {
        event.Enable(false);
        return;
    }

    switch (event.GetId())
    {
    case ID_VIEW_SHOW_GRID:     event.Check(c->Options().showGrid); break;
    case ID_VIEW_SHOW_PIN_NAMES: event.Check(c->Options().showPinNames); break;
    case ID_VIEW_SHOW_LABELS:   event.Check(c->Options().showLabels); break;
    default: break;
    }
}

// =============================================================================
//  工具
// =============================================================================
void MainFrame::OnToolSelect(wxCommandEvent& event)
{
    CanvasPanel* canvas = ActiveCanvas();
    if (canvas == nullptr) return;

    CanvasEventHandler* handler = canvas->Handler();
    if (handler == nullptr) return;

    ToolType tool = ToolType::Select;
    switch (event.GetId())
    {
    case ID_TOOL_DRAG:      tool = ToolType::Drag; break;
    case ID_TOOL_SELECT:    tool = ToolType::Select; break;
    case ID_TOOL_TEXT:      tool = ToolType::Text; break;
    case ID_TOOL_COMPONENT: tool = ToolType::Component; break;
    case ID_TOOL_WIRE:      tool = ToolType::Wire; break;
    case ID_TOOL_DRAWING:   tool = ToolType::Drawing; break;
    case ID_TOOL_ERASER:    tool = ToolType::Eraser; break;
    default: return;
    }

    if (tool != ToolType::Component) handler->CancelPlacement();
    canvas->Tools().SetTool(tool);
    canvas->SetFocus();
    canvas->Refresh(false);
}

void MainFrame::OnDrawKindSelect(wxCommandEvent& event)
{
    CanvasPanel* canvas = ActiveCanvas();
    if (canvas == nullptr || canvas->Handler() == nullptr) return;

    CanvasEventHandler::DrawKind kind = CanvasEventHandler::DrawKind::Rectangle;
    if (event.GetId() == ID_DRAW_LINE)
        kind = CanvasEventHandler::DrawKind::Line;
    else if (event.GetId() == ID_DRAW_CIRCLE)
        kind = CanvasEventHandler::DrawKind::Circle;

    canvas->Handler()->SetDrawKind(kind);
    canvas->Tools().SetTool(ToolType::Drawing);
    canvas->SetFocus();
}

void MainFrame::OnToolRotate(wxCommandEvent& event)
{
    CanvasPanel* canvas = ActiveCanvas();
    if (canvas == nullptr || canvas->Handler() == nullptr) return;

    canvas->Handler()->RotateSelection(event.GetId() == ID_TOOL_ROTATE_CW ? 1 : -1);
}

void MainFrame::OnToolMirror(wxCommandEvent& WXUNUSED(event))
{
    CanvasPanel* canvas = ActiveCanvas();
    if (canvas == nullptr || canvas->Handler() == nullptr) return;

    canvas->Handler()->MirrorSelection();
}

void MainFrame::OnUpdateTool(wxUpdateUIEvent& event)
{
    CanvasPanel* canvas = ActiveCanvas();
    if (canvas == nullptr)
    {
        event.Check(false);
        return;
    }

    const ToolType current = canvas->Tools().Tool();
    ToolType mine = ToolType::Select;
    switch (event.GetId())
    {
    case ID_TOOL_DRAG:      mine = ToolType::Drag; break;
    case ID_TOOL_SELECT:    mine = ToolType::Select; break;
    case ID_TOOL_TEXT:      mine = ToolType::Text; break;
    case ID_TOOL_COMPONENT: mine = ToolType::Component; break;
    case ID_TOOL_WIRE:      mine = ToolType::Wire; break;
    case ID_TOOL_DRAWING:   mine = ToolType::Drawing; break;
    case ID_TOOL_ERASER:    mine = ToolType::Eraser; break;
    default: break;
    }

    event.Check(current == mine);
}

// =============================================================================
//  图纸
// =============================================================================
void MainFrame::OnSheetNew(wxCommandEvent& WXUNUSED(event))
{
    if (m_notebook != nullptr) m_notebook->CreateNewSheet();
    RefreshAllPanels();
}

void MainFrame::OnSheetClose(wxCommandEvent& WXUNUSED(event))
{
    if (m_notebook == nullptr) return;

    const wxString name = m_notebook->ActiveSheetName();
    if (name.IsEmpty()) return;

    m_notebook->CloseSheetByName(name);
    RefreshAllPanels();
}

// =============================================================================
//  Verilog（M3 / M4 里程碑）
// =============================================================================
void MainFrame::OnVerilogExport(wxCommandEvent& WXUNUSED(event))
{
    m_ctx.Log("导出 Verilog 属于 M3 里程碑：图 → 码 的生成器尚未接入。");
}

void MainFrame::OnVerilogImport(wxCommandEvent& WXUNUSED(event))
{
    m_ctx.Log("从 Verilog 导入属于 M4 里程碑：词法/语法分析与自动布局尚未接入。");
}

void MainFrame::OnVerilogSyncStatus(wxCommandEvent& WXUNUSED(event))
{
    Project* project = m_ctx.Proj();
    if (project == nullptr) return;

    const wxString side = project->linkedVerilogPath.IsEmpty() ? wxString("（未关联）") : project->linkedVerilogPath;
    wxMessageBox(wxString::Format("双向同步状态\n\n对侧文件：%s\n图纸是否已改动：%s\n\n同步引擎将在 M3 / M4 里程碑接入。",
                                  side, project->dirty ? "是" : "否"),
                 "SiliFlow", wxOK | wxICON_INFORMATION);
}

void MainFrame::OnVerilogOpenFile(wxCommandEvent& WXUNUSED(event))
{
    Project* project = m_ctx.Proj();
    if (project == nullptr || project->linkedVerilogPath.IsEmpty())
    {
        m_ctx.Log("尚未关联 Verilog 对侧文件。");
        return;
    }

    if (!wxLaunchDefaultApplication(project->linkedVerilogPath))
        m_ctx.Log("无法打开： " + project->linkedVerilogPath);
}

// =============================================================================
//  帮助
// =============================================================================
void MainFrame::OnHelpShortcuts(wxCommandEvent& WXUNUSED(event))
{
    const wxString text =
        "画布操作\n"
        "  中键拖动 / 空格 + 左键拖动     平移画布\n"
        "  Ctrl + 滚轮                    以鼠标为锚点缩放\n"
        "  Ctrl+0 / Ctrl+1                适应窗口 / 100%\n"
        "  左键单击                       选中；空白处拖动为框选\n"
        "  Shift + 单击                   加选\n"
        "  Shift + 拖动                   临时关闭网格吸附\n"
        "  双击元件                       修改标签\n"
        "  Esc                            取消当前手势 / 退出放置模式\n"
        "  Del                            删除选中对象\n"
        "  Ctrl+A                         全选\n"
        "  Ctrl+R / Ctrl+Shift+R          顺时针 / 逆时针旋转 90°\n"
        "  Ctrl+M                         水平镜像\n"
        "  方向键 / Shift+方向键          移动 10px / 1px\n"
        "\n"
        "工具快捷键\n"
        "  S 选择    T 文本    C 元件    W 导线    D 绘图    E 擦除    H 拖拽\n"
        "\n"
        "文件\n"
        "  Ctrl+N / Ctrl+O / Ctrl+S      新建 / 打开 / 保存\n"
        "  Ctrl+Z / Ctrl+Y               撤销 / 重做\n"
        "  F7 / F8                       导出 / 导入 Verilog（M3 / M4）";

    wxMessageBox(text, "快捷键一览", wxOK | wxICON_INFORMATION, this);
}

void MainFrame::OnHelpSelfCheck(wxCommandEvent& WXUNUSED(event))
{
    wxString report;
    const int code = RunCoreSelfCheck(&report);

    if (m_console != nullptr)
    {
        m_console->Append("========== 数据层自检 ==========");
        m_console->Append(report);
        m_console->Append(wxString::Format("========== 退出码 %d ==========", code));
    }

    if (m_wave != nullptr && m_console != nullptr)
    {
        m_console->Show(true);
        m_bottomHost->Layout();
        m_aui.GetPane(m_bottomHost).Show(true);
        m_aui.Update();
    }

    wxMessageBox(code == 0 ? "数据层自检全部通过，详情见「消息」面板。" : "自检存在失败项，详情见「消息」面板。",
                 "SiliFlow", wxOK | (code == 0 ? wxICON_INFORMATION : wxICON_WARNING), this);
}

void MainFrame::OnAbout(wxCommandEvent& WXUNUSED(event))
{
    wxAboutDialogInfo info;
    info.SetName("SiliFlow");
    info.SetVersion("0.1.0");
    info.SetDescription("数字电路原理图编辑器（类 Logisim 形态）\n"
                        "当前为框架骨架版：界面分层、数据模型、画布编辑闭环已完成；\n"
                        "Verilog 双向同步与 VCD 波形查看为后续里程碑。");
    info.SetCopyright("(C) 2026 SiliFlow");
    wxAboutBox(info, this);
}

// =============================================================================
//  关闭
// =============================================================================
void MainFrame::OnClose(wxCloseEvent& event)
{
    if (event.CanVeto() && !ConfirmDiscardChanges())
    {
        event.Veto();
        return;
    }

    event.Skip();
}

// =============================================================================
//  自检报告（命令行 --selfcheck 用）
// =============================================================================
void MainFrame::ShowSelfCheckReport(const wxString& report, int exitCode)
{
    if (m_console != nullptr)
    {
        m_console->Append("========== 数据层自检 ==========");
        m_console->Append(report);
        m_console->Append(wxString::Format("========== 退出码 %d ==========", exitCode));
    }

    if (m_bottomHost != nullptr)
    {
        m_console->Show(true);
        m_bottomHost->Layout();
        m_aui.GetPane(m_bottomHost).Show(true);
        m_aui.Update();
    }
}

}  // namespace sf
