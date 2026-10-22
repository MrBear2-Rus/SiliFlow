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
#include "ui/UiKit.h"

#include <wx/aboutdlg.h>
#include <wx/choice.h>
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
//  Apple 风格自定义外壳：标题栏 / 工具栏 / 窗口按钮
//  绘制细节（矢量图标、扁平按钮）在 ui/UiKit.cpp，这里只做装配。
// =============================================================================
namespace
{

// 让下面既有代码保持短名，同时只有一个实现（ui/UiKit）。
using ui::Button;
using ui::DrawGlyph;
using ui::Glyph;
using ui::MakeSeparator;

constexpr int kTitleBarHeight = ui::metric::kTitleBarHeight;
constexpr int kToolBarHeight = ui::metric::kToolBarHeight;

/// DPI 换算的短手写。w 必须是一个已存在的窗口（用它所在的显示器的缩放比）。
inline int Dip(const wxWindow* w, int v) { return wxWindow::FromDIP(v, w); }

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

}  // namespace


// -----------------------------------------------------------------------------
//  自绘标题栏（原型 .titlebar，38px）
//    左：App 标记    中：应用名 + 文档   右：动作按钮 | 窗口按钮
//
//  原型把窗口按钮画在左侧的 macOS“交通灯”上，那是 macOS 专有交互；
//  Windows / Linux 上把窗口按钮放左边会让用户按错，这里保留平台习惯（按钮在右），
//  左侧改用 App 标记 —— 设计语言一致，交互不拧巴。
// -----------------------------------------------------------------------------
class AppTitleBar : public wxPanel
{
public:
    explicit AppTitleBar(wxWindow* parent)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition,
                  wxSize(-1, Dip(parent, ui::metric::kTitleBarHeight)), wxBORDER_NONE)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetBackgroundColour(ui::kChrome);
        Bind(wxEVT_PAINT, &AppTitleBar::OnPaint, this);
        Bind(wxEVT_LEFT_DOWN, &AppTitleBar::OnLeftDown, this);
        Bind(wxEVT_LEFT_UP, &AppTitleBar::OnLeftUp, this);
        Bind(wxEVT_MOTION, &AppTitleBar::OnMotion, this);
        Bind(wxEVT_LEFT_DCLICK, &AppTitleBar::OnDClick, this);

        // 原型 .doc b{font-size:13.5px; font-weight:600}
        m_appName = new wxStaticText(this, wxID_ANY, "SiliFlow");
        m_appName->SetFont(ui::Font(ui::fontsize::kHead, true));
        m_appName->SetForegroundColour(ui::kTitleText);

        // 原型 .doc .crumb{font-size:12.5px; color:var(--text-3)}
        m_doc = new wxStaticText(this, wxID_ANY, "未命名 · main");
        m_doc->SetFont(ui::Font(ui::fontsize::kBody));
        m_doc->SetForegroundColour(ui::kTertiary);

        // 复用已有的「快捷键一览」命令，标题栏按钮从菜单栏搬过来但行为不变。
        m_help = new ui::Button(this, ID_HELP_SHORTCUTS, ui::Glyph::Help, wxEmptyString,
                                ui::Button::Style::Icon, "快捷键一览 (F1)");

        m_winMin = new ui::Button(this, wxID_ANY, ui::Glyph::Min, wxEmptyString,
                                  ui::Button::Style::Window, "最小化");
        m_winMax = new ui::Button(this, wxID_ANY, ui::Glyph::Max, wxEmptyString,
                                  ui::Button::Style::Window, "最大化");
        m_winClose = new ui::Button(this, wxID_ANY, ui::Glyph::Close, wxEmptyString,
                                    ui::Button::Style::Window, "关闭", true);
        m_winMin->SetOnClick([this] { if (wxTopLevelWindow* f = Top()) f->Iconize(true); });
        m_winMax->SetOnClick([this] { if (wxTopLevelWindow* f = Top()) f->Maximize(!f->IsMaximized()); });
        m_winClose->SetOnClick([this] { if (wxTopLevelWindow* f = Top()) f->Close(); });

        // 有未保存改动时的胶囊（原型 .chip-dirty），默认不显示。
        m_dirty = new ui::Chip(this, ui::kWarn);
        m_dirty->SetText("未保存");
        m_dirty->Hide();

        // 原型 .titlebar{gap:14px; padding:0 12px}，文档块在两侧留白之间居中。
        m_row = new wxBoxSizer(wxHORIZONTAL);
        m_row->AddSpacer(Dip(this, ui::space::kLg) * 2 + Dip(this, ui::space::kXxs));  // 左侧徽标位
        m_row->AddStretchSpacer(1);
        m_row->Add(m_appName, 0, wxALIGN_CENTER_VERTICAL);
        m_row->AddSpacer(Dip(this, ui::space::kMd));
        m_row->Add(m_doc, 0, wxALIGN_CENTER_VERTICAL);
        m_row->AddSpacer(Dip(this, ui::space::kMd));
        m_row->Add(m_dirty, 0, wxALIGN_CENTER_VERTICAL);
        m_row->AddStretchSpacer(1);
        m_row->Add(m_help, 0, wxALIGN_CENTER_VERTICAL);
        m_row->Add(MakeSeparator(this, Dip(this, 16)), 0,
                   wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, Dip(this, ui::space::kSm));
        m_row->Add(m_winMin, 0, wxALIGN_CENTER_VERTICAL);
        m_row->Add(m_winMax, 0, wxALIGN_CENTER_VERTICAL);
        m_row->Add(m_winClose, 0, wxALIGN_CENTER_VERTICAL);
        m_row->AddSpacer(Dip(this, ui::space::kXs));
        SetSizer(m_row);
    }

    void SetDocument(const wxString& doc)
    {
        m_doc->SetLabel(doc);
        Layout();
    }

    void SetDirty(bool on)
    {
        if (m_dirty == nullptr || m_dirty->IsShown() == on) return;
        m_dirty->Show(on);
        Layout();
    }

private:
    wxTopLevelWindow* Top() const
    {
        return dynamic_cast<wxTopLevelWindow*>(wxGetTopLevelParent(const_cast<AppTitleBar*>(this)));
    }

    void OnPaint(wxPaintEvent&)
    {
        wxAutoBufferedPaintDC dc(this);
        const wxSize sz = GetClientSize();
        dc.SetBackground(wxBrush(ui::kChrome));
        dc.Clear();
        dc.SetPen(wxPen(ui::kBorder, 1));
        dc.DrawLine(0, sz.y - 1, sz.x, sz.y - 1);

        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
        if (gc == nullptr) return;
        gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);

        // App 标记：站在原型交通灯的位置上，尺寸与三个圆点占位一致。
        const double badge = Dip(this, 20);
        const double bx = Dip(this, ui::space::kLg);
        const double by = (sz.y - badge) / 2.0;
        gc->SetPen(*wxTRANSPARENT_PEN);
        gc->SetBrush(wxBrush(ui::kAccent));
        gc->DrawRoundedRectangle(bx, by, badge, badge, Dip(this, ui::radius::kXs));
        gc->SetFont(ui::Font(ui::fontsize::kCaption, true), ui::kOnAccent);
        double tw = 0, th = 0;
        gc->GetTextExtent("S", &tw, &th);
        gc->DrawText("S", bx + (badge - tw) / 2.0, by + (badge - th) / 2.0);
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
    ui::Chip* m_dirty = nullptr;
    ui::Button* m_help = nullptr;
    ui::Button* m_winMin = nullptr;
    ui::Button* m_winMax = nullptr;
    ui::Button* m_winClose = nullptr;
    wxBoxSizer* m_row = nullptr;
    bool m_dragging = false;
    wxPoint m_dragStart;
    wxPoint m_winStart;
};

// -----------------------------------------------------------------------------
//  自绘菜单栏（原型 .menubar，30px）
//  把原生菜单栏取出的顶层菜单交给文字按钮弹出：菜单项、快捷键、更新逻辑都不变。
// -----------------------------------------------------------------------------
class AppMenuBar : public ui::MenuBar
{
public:
    explicit AppMenuBar(wxWindow* parent) : ui::MenuBar(parent) {}

    void SetMenus(const std::vector<std::pair<wxString, wxMenu*>>& menus)
    {
        for (const auto& e : menus)
        {
            wxMenu* menu = e.second;
            ui::Button* b = AddMenu(StripAmp(e.first), e.first);
            b->SetOnClick(
                [this, b, menu]
                {
                    const wxPoint p = b->ClientToScreen(wxPoint(0, GetSize().y));
                    SetOpen(b);          // 展开态 = 强调色实心（原型 aria-expanded）
                    PopupMenu(menu, p);
                    SetOpen(nullptr);
                });
        }
        Layout();
    }
};

// -----------------------------------------------------------------------------
//  自绘工具栏（原型 .toolbar，46px）
//  工具组是一个分段控件，其余是 32px 图标按钮；右侧是节拍频率。
// -----------------------------------------------------------------------------
class AppToolBar : public wxPanel
{
public:
    explicit AppToolBar(wxWindow* parent)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition,
                  wxSize(-1, Dip(parent, ui::metric::kToolBarHeight)), wxBORDER_NONE)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetBackgroundColour(ui::kChromeAlt);
        Bind(wxEVT_PAINT, &AppToolBar::OnPaint, this);

        const int gap = Dip(this, 3);   // .tb-sep{margin:0 3px}
        const int pad = Dip(this, 3);   // 按钮左右留 3（.toolbar gap:6 的一半）

        auto* row = new wxBoxSizer(wxHORIZONTAL);
        row->AddSpacer(Dip(this, 10));  // .toolbar{padding:0 10px}

        auto addGroupGap = [&]
        {
            row->Add(MakeSeparator(this, Dip(this, 22)), 0,
                     wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, gap);
        };
        auto addIcon = [&](wxWindowID id, ui::Glyph glyph, const wxString& tip,
                           ui::Button::Style style = ui::Button::Style::Icon)
        {
            auto* b = new ui::Button(this, id, glyph, wxEmptyString, style, tip);
            row->Add(b, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, pad);
            return b;
        };

        // ---- 工具组：原型 .seg（凹陷槽，选中项变白底 + 图标染强调色）----
        auto* seg = new ui::SegmentedGroup(this);
        const wxWindowID toolIds[] = { ID_TOOL_SELECT, ID_TOOL_DRAG, ID_TOOL_TEXT, ID_TOOL_COMPONENT,
                                       ID_TOOL_WIRE, ID_TOOL_DRAWING, ID_TOOL_ERASER };
        const ui::Glyph toolGlyphs[] = { ui::Glyph::Select, ui::Glyph::Hand, ui::Glyph::Text,
                                         ui::Glyph::Chip, ui::Glyph::Wire, ui::Glyph::Shapes,
                                         ui::Glyph::Eraser };
        const wxString toolLabels[] = { "选择", "拖拽", "文本", "元件", "导线", "绘图", "擦除" };

        for (int i = 0; i < 7; ++i)
        {
            auto* b = new ui::Button(seg, toolIds[i], toolGlyphs[i], toolLabels[i],
                                     ui::Button::Style::SegItem, toolLabels[i]);
            m_toolBtns.push_back(b);
            seg->Add(b);
        }
        row->Add(seg, 0, wxALIGN_CENTER_VERTICAL);
        addGroupGap();

        addIcon(ID_TOOL_ROTATE_CW, ui::Glyph::Rotate, "顺时针旋转 90°");
        addIcon(ID_TOOL_MIRROR, ui::Glyph::Mirror, "水平镜像");
        addGroupGap();

        addIcon(ID_VIEW_ZOOM_OUT, ui::Glyph::ZoomOut, "缩小");
        m_zoom = new wxStaticText(this, wxID_ANY, "100%");
        m_zoom->SetFont(ui::MonoFont(ui::fontsize::kBody));
        m_zoom->SetForegroundColour(ui::kSecondary);
        m_zoom->SetMinSize(wxSize(Dip(this, 44), -1));
        row->Add(m_zoom, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, pad);
        addIcon(ID_VIEW_ZOOM_IN, ui::Glyph::ZoomIn, "放大");
        addIcon(ID_VIEW_ZOOM_FIT, ui::Glyph::Fit, "适应窗口 (Ctrl+0)");
        addGroupGap();

        // 运行是常驻强调色的主操作按钮；跑起来时变绿（原型 .tbtn.primary[aria-pressed]）。
        m_run = addIcon(ID_SIM_RUN, ui::Glyph::Play, "运行自动节拍 (F5)", ui::Button::Style::Primary);
        addIcon(ID_SIM_STOP, ui::Glyph::Stop, "停止");
        addIcon(ID_SIM_STEP, ui::Glyph::Step, "单步 (Ctrl+T)");
        addIcon(ID_SIM_RESET, ui::Glyph::Reset, "复位 (Ctrl+Shift+T)");

        row->AddStretchSpacer(1);

        // ---- 右侧：节拍频率（原型 .freq-select）----
        auto* lab = new wxStaticText(this, wxID_ANY, "节拍");
        lab->SetFont(ui::Font(ui::fontsize::kCaption));
        lab->SetForegroundColour(ui::kTertiary);
        row->Add(lab, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, Dip(this, ui::space::kSm));

        m_freq = new wxChoice(this, wxID_ANY, wxDefaultPosition,
                              wxSize(Dip(this, 78), Dip(this, ui::metric::kToolButton)));
        m_freq->SetFont(ui::Font(ui::fontsize::kCaption));
        for (int hz : { 1, 2, 5, 10, 20 }) m_freq->Append(wxString::Format("%d Hz", hz));
        m_freq->SetSelection(1);
        // 复用既有的频率菜单命令（ID_SIM_FREQ_* 连续排列），不新增业务分支。
        m_freq->Bind(wxEVT_CHOICE,
                     [this](wxCommandEvent& e)
                     {
                         const int sel = e.GetSelection();
                         if (sel < 0) return;
                         wxCommandEvent evt(wxEVT_MENU, ID_SIM_FREQ_1 + sel);
                         evt.SetEventObject(this);
                         GetEventHandler()->ProcessEvent(evt);
                     });
        row->Add(m_freq, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, pad);
        row->AddSpacer(Dip(this, ui::space::kSm));

        SetSizer(row);
    }

    void SetActiveTool(int id)
    {
        for (ui::Button* b : m_toolBtns) b->SetActive(b->GetId() == id);
    }
    void SetSimRunning(bool running)
    {
        if (m_run != nullptr) m_run->SetActive(running);
    }
    /// 状态栏传过来的「缩放 120%」→ 工具栏里只留「120%」（原型 .zoom-val）。
    void SetZoomText(const wxString& text)
    {
        if (m_zoom == nullptr) return;
        wxString t = text;
        const int sp = t.Find(' ');
        if (sp != wxNOT_FOUND) t = t.Mid(static_cast<size_t>(sp) + 1);
        if (m_zoom->GetLabel() == t) return;
        m_zoom->SetLabel(t);
        Layout();
    }

private:
    void OnPaint(wxPaintEvent&)
    {
        wxAutoBufferedPaintDC dc(this);
        const wxSize sz = GetClientSize();
        dc.SetBackground(wxBrush(ui::kChromeAlt));
        dc.Clear();
        dc.SetPen(wxPen(ui::kBorder, 1));
        dc.DrawLine(0, sz.y - 1, sz.x, sz.y - 1);
    }

    std::vector<ui::Button*> m_toolBtns;
    ui::Button* m_run = nullptr;
    wxStaticText* m_zoom = nullptr;
    wxChoice* m_freq = nullptr;
};

// =============================================================================
//  构造 / 析构
// =============================================================================
MainFrame::MainFrame(const wxString& title)
    : wxFrame(nullptr, wxID_ANY, title, wxDefaultPosition, wxSize(1440, 900),
#if SF_CUSTOM_CHROME
              wxDEFAULT_FRAME_STYLE & ~wxCAPTION  // 去掉原生标题栏，改用自绘标题栏
#else
              wxDEFAULT_FRAME_STYLE               // 交给窗口管理器画标题栏
#endif
      )
{
    SetMinSize(FromDIP(wxSize(1024, 680)));
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

    // ---- 扁平 Apple 停靠外观：无渐变、发丝线、面板白底、32px 面板标题 ----
    //  外观本体在 ui::MakeDockArt()（唯一实现），这里只接管所有权。
    m_aui.SetArtProvider(ui::MakeDockArt());

    m_notebook = new CanvasNoteBook(this, &m_ctx);
    m_toolbox = new ToolboxPanel(this, &m_ctx);
    m_property = new PropertyPanel(this, &m_ctx);
    m_tree = new TreePanel(this, &m_ctx);

    // 底部宿主：左侧波形、右侧消息。
    // 注意两个子面板的父窗口必须是宿主面板本身 —— sizer 只接受宿主窗口的子窗口。
    m_bottomHost = new wxPanel(this);
    m_bottomHost->SetBackgroundColour(ui::kPanel);
    m_wave = new WavePanel(m_bottomHost, &m_ctx);
    m_console = new ConsolePanel(m_bottomHost);

    // 两块内容之间用一条发丝线分隔，而不是靠底色差异（浅色主题下更干净）。
    auto* hostSizer = new wxBoxSizer(wxHORIZONTAL);
    hostSizer->Add(m_wave, 3, wxEXPAND);
    hostSizer->Add(ui::MakeSeparator(m_bottomHost, -1), 0, wxEXPAND);
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
#if SF_CUSTOM_CHROME
    // 菜单从原生菜单栏取出来（取得所有权），交给自绘菜单栏的文字按钮弹出。
    // 菜单项、快捷键、wxEVT_UPDATE_UI 更新逻辑全部保持不变。
    wxMenuBar* bar = CreateMainMenuBar();

    // 菜单标签写一份在这里，**不要**去问 wxMenuBar：
    // 还没 Attach 到 frame 的菜单栏在 MSW 上没有原生句柄，GetMenuLabel() 会断言。
    // 顺序必须与 CreateMainMenuBar() 里的 bar->Append(...) 一致。
    static const char* const kLabels[] = {"文件(&F)", "编辑(&E)", "视图(&V)", "工具(&T)",
                                          "仿真(&M)", "图纸(&H)", "Verilog(&L)", "帮助(&A)"};
    const size_t labelCount = sizeof(kLabels) / sizeof(kLabels[0]);

    std::vector<std::pair<wxString, wxMenu*>> menus;
    wxASSERT_MSG(bar->GetMenuCount() == labelCount, "菜单数量与标签表不一致，请同步 kLabels");
    for (size_t i = 0; i < labelCount && bar->GetMenuCount() > 0; ++i)
        menus.emplace_back(wxString(kLabels[i]), bar->Remove(static_cast<size_t>(0)));

    delete bar;

    m_titleBar = new AppTitleBar(this);
    m_menuBar = new AppMenuBar(this);
    m_menuBar->SetMenus(menus);
    for (auto& e : menus) m_menus.push_back(e.second);
#else
    // 原生标题栏：菜单交回系统菜单栏，只有工具栏是自绘的。
    // 这样窗口的拖动 / 缩放 / 吸附 / 阴影全部由窗口管理器负责，跨平台最稳。
    SetMenuBar(CreateMainMenuBar());  // wxFrame 接管菜单所有权
    m_titleBar = nullptr;
    m_menuBar = nullptr;
    m_menus.clear();
#endif

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
    const int titleH = FromDIP(kTitleBarHeight);
    const int menuH = FromDIP(ui::metric::kMenuBarHeight);
    const int toolH = FromDIP(kToolBarHeight);

    // 顶部三行（原型 .titlebar / .menubar / .toolbar）：不可停靠 / 无边框，钉在顶端。
    // 走原生标题栏的平台没有 titlebar 面板，这里判空跳过即可。
    int topPos = 0;
    if (m_titleBar != nullptr)
    {
        m_aui.AddPane(m_titleBar, wxAuiPaneInfo()
                                      .Name("titlebar").CaptionVisible(false)
                                      .Top().Layer(10).Position(topPos++)
                                      .Dockable(false).Movable(false).Floatable(false)
                                      .Gripper(false).CloseButton(false).PaneBorder(false)
                                      .MinSize(wxSize(-1, titleH))
                                      .BestSize(wxSize(-1, titleH))
                                      .MaxSize(wxSize(-1, titleH)));
    }

    if (m_menuBar != nullptr)
    {
        m_aui.AddPane(m_menuBar, wxAuiPaneInfo()
                                     .Name("menubar").CaptionVisible(false)
                                     .Top().Layer(10).Position(topPos++)
                                     .Dockable(false).Movable(false).Floatable(false)
                                     .Gripper(false).CloseButton(false).PaneBorder(false)
                                     .MinSize(wxSize(-1, menuH))
                                     .BestSize(wxSize(-1, menuH))
                                     .MaxSize(wxSize(-1, menuH)));
    }

    m_aui.AddPane(m_toolBar, wxAuiPaneInfo()
                                 .Name("toolbar").CaptionVisible(false)
                                 .Top().Layer(10).Position(topPos)
                                 .Dockable(false).Movable(false).Floatable(false)
                                 .Gripper(false).CloseButton(false).PaneBorder(false)
                                 .MinSize(wxSize(-1, toolH))
                                 .BestSize(wxSize(-1, toolH))
                                 .MaxSize(wxSize(-1, toolH)));

    // 左右侧栏宽度取原型的 264 / 316；底栏取 224。
    m_aui.AddPane(m_toolbox, wxAuiPaneInfo()
                                 .Name("toolbox")
                                 .Caption("元件库")
                                 .Left()
                                 .Layer(1)
                                 .Position(0)
                                 .MinSize(FromDIP(200), FromDIP(200))
                                 .BestSize(FromDIP(ui::metric::kSidebarWidth), FromDIP(720))
                                 .CloseButton(false));

    m_aui.AddPane(m_notebook, wxAuiPaneInfo().Name("canvas").Caption("原理图").CenterPane().PaneBorder(false));

    m_aui.AddPane(m_property, wxAuiPaneInfo()
                                  .Name("property")
                                  .Caption("属性")
                                  .Right()
                                  .Layer(1)
                                  .Position(0)
                                  .MinSize(FromDIP(240), FromDIP(180))
                                  .BestSize(FromDIP(ui::metric::kRightWidth), FromDIP(460)));

    m_aui.AddPane(m_tree, wxAuiPaneInfo()
                              .Name("tree")
                              .Caption("层次树")
                              .Right()
                              .Layer(1)
                              .Position(1)
                              .MinSize(FromDIP(240), FromDIP(140))
                              .BestSize(FromDIP(ui::metric::kRightWidth), FromDIP(380)));

    // 底栏：原型里底部是左右两半，各自带 .bh-head，外面没有标题 —— 所以隐藏 AUI 标题。
    m_aui.AddPane(m_bottomHost, wxAuiPaneInfo()
                                   .Name("bottom")
                                   .CaptionVisible(false)
                                   .Bottom()
                                   .Layer(1)
                                   .Position(0)
                                   .MinSize(FromDIP(500), FromDIP(150))
                                   .BestSize(FromDIP(1440), FromDIP(ui::metric::kBottomHeight))
                                   .PaneBorder(false));

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
        sb->SetFont(ui::Font(ui::fontsize::kCaption));
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
        if (m_toolBar != nullptr) m_toolBar->SetZoomText(text);
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
            eng.SetForced(compId, LogicVector::FromText(level));

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
    if (m_titleBar != nullptr)
    {
        m_titleBar->SetDocument(projectName + "  ·  " + sheetName);
        m_titleBar->SetDirty(m_ctx.Proj() != nullptr && m_ctx.Proj()->dirty);
    }
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
