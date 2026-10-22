#include "WavePanel.h"

#include "core/Types.h"
#include "ui/UiKit.h"

#include <wx/dcbuffer.h>
#include <wx/graphics.h>
#include <wx/sizer.h>
#include <wx/splitter.h>
#include <wx/stattext.h>
#include <wx/treectrl.h>

namespace sf
{

// =============================================================================
//  WaveCanvas
// =============================================================================
WaveCanvas::WaveCanvas(wxWindow* parent, wxWindowID id) : wxPanel(parent, id)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(ui::kPanel);
    Bind(wxEVT_PAINT, &WaveCanvas::OnPaint, this);
}

int WaveCanvas::NameColumnWidth() const { return FromDIP(190); }
int WaveCanvas::TimeAxisHeight() const { return FromDIP(28); }
int WaveCanvas::RowHeight() const { return FromDIP(ui::metric::kRowHeight); }

void WaveCanvas::SetPlaceholder(const wxString& text)
{
    m_placeholder = text;
    Refresh(false);
}

void WaveCanvas::SetSignalCount(int count)
{
    m_signalCount = count;
    Refresh(false);
}

void WaveCanvas::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    wxAutoBufferedPaintDC dc(this);
    const wxSize size = GetClientSize();

    dc.SetBackground(wxBrush(ui::kPanel));
    dc.Clear();

    wxGraphicsContext* gc = wxGraphicsContext::Create(dc);
    if (gc == nullptr) return;

    gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);

    const int nameW = NameColumnWidth();
    const int axisH = TimeAxisHeight();

    // ---- 左侧信号名列底纹 ----
    gc->SetBrush(wxBrush(ui::kPanelAlt));
    gc->SetPen(*wxTRANSPARENT_PEN);
    gc->DrawRectangle(0, 0, nameW, size.y);

    // ---- 顶部时间轴 ----
    gc->SetBrush(wxBrush(ui::kChromeAlt));
    gc->DrawRectangle(0, 0, size.x, axisH);

    // 分隔线（发丝线，与面板内其它分隔保持同一灰度）
    gc->SetPen(wxPen(ui::kSeparator, 1));
    gc->StrokeLine(nameW, 0, nameW, size.y);
    gc->StrokeLine(0, axisH, size.x, axisH);

    // 时间刻度（仅装饰，M5 接入真实时间轴后替换）
    gc->SetPen(wxPen(palette::kGrid, 1));
    const int tick = FromDIP(60);
    for (int x = nameW; x < size.x; x += tick) gc->StrokeLine(x, axisH - FromDIP(6), x, size.y);

    // ---- 空状态 / 信号行占位 ----
    if (m_signalCount <= 0)
    {
        const wxRect area(nameW, axisH, std::max(0, size.x - nameW), std::max(0, size.y - axisH));
        ui::DrawEmptyState(*gc, area, m_placeholder,
                           "使用「打开 VCD…」加载仿真波形（M5 里程碑接入解析与绘制）");
    }
    else
    {
        // 已加载信号时画出网格化的行底纹，等待 M5 填入真实波形
        gc->SetPen(wxPen(palette::kGrid, 1));
        const int rowH = RowHeight();
        for (int i = 0; i < m_signalCount; ++i)
        {
            const int y = axisH + i * rowH;
            if (y > size.y) break;
            gc->StrokeLine(nameW, y, size.x, y);
        }
    }

    delete gc;
}

// =============================================================================
//  WavePanel
// =============================================================================
WavePanel::WavePanel(wxWindow* parent, Services* services, wxWindowID id)
    : wxPanel(parent, id), m_services(services)
{
    SetBackgroundColour(ui::kPanel);

    auto* sizer = new wxBoxSizer(wxVERTICAL);

    // ---- 内容标题 + 右侧紧凑按钮（原型 .bh-head）----
    auto* actions = new wxPanel(this, wxID_ANY);
    actions->SetBackgroundColour(ui::kPanel);
    auto* actionRow = new wxBoxSizer(wxHORIZONTAL);
    auto* openBtn = new ui::Button(actions, wxID_ANY, ui::Glyph::None, "打开 VCD…",
                                   ui::Button::Style::Text, "加载 VCD 波形文件");
    auto* clearBtn = new ui::Button(actions, wxID_ANY, ui::Glyph::None, "清空",
                                    ui::Button::Style::Text, "清空已加载的波形");
    openBtn->SetCompact(true);
    clearBtn->SetCompact(true);
    openBtn->SetOnClick(
        [this]
        {
            if (m_services != nullptr)
                m_services->Log("波形查看属于 M5 里程碑：本版已就位面板与布局，解析与绘制尚未接入。");
        });
    clearBtn->SetOnClick([this] { Reset(); });
    actionRow->Add(openBtn, 0, wxRIGHT, FromDIP(ui::space::kSm));
    actionRow->Add(clearBtn, 0);
    actions->SetSizer(actionRow);

    m_head = new ui::PanelHeader(this, "波形", "未加载");
    m_head->SetTrailing(actions);
    sizer->Add(m_head, 0, wxEXPAND);

    // ---- 左信号树 + 右波形区（原型 .wave-body：180px + 1fr）----
    m_splitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                      wxSP_LIVE_UPDATE | wxSP_THIN_SASH);
    m_splitter->SetMinimumPaneSize(FromDIP(120));

    m_signalTree = new wxTreeCtrl(m_splitter, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                  wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT | wxTR_HIDE_ROOT | wxBORDER_NONE);
    m_signalTree->SetFont(ui::Font(ui::fontsize::kBody));
    m_signalTree->SetBackgroundColour(ui::kPanelAlt);
    m_signalTree->AddRoot("signals");
    m_signalTree->AppendItem(m_signalTree->GetRootItem(), "（未加载）");

    m_canvas = new WaveCanvas(m_splitter);

    m_splitter->SplitVertically(m_signalTree, m_canvas, FromDIP(ui::metric::kWaveSignalWidth));
    sizer->Add(m_splitter, 1, wxEXPAND);

    SetSizer(sizer);
}

void WavePanel::Reset()
{
    if (m_signalTree != nullptr)
    {
        m_signalTree->DeleteAllItems();
        m_signalTree->AddRoot("signals");
        m_signalTree->AppendItem(m_signalTree->GetRootItem(), "（未加载）");
    }

    if (m_canvas != nullptr) m_canvas->SetSignalCount(0);
    if (m_head != nullptr) m_head->SetSubtitle("未加载");
}

}  // namespace sf
