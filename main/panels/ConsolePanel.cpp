#include "ConsolePanel.h"

#include "ui/UiKit.h"

#include <wx/datetime.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>

#include <algorithm>

namespace sf
{

ConsolePanel::ConsolePanel(wxWindow* parent, wxWindowID id) : wxPanel(parent, id)
{
    SetBackgroundColour(ui::kPanel);

    auto* sizer = new wxBoxSizer(wxVERTICAL);

    // ---- 内容标题（原型 .bh-head）：标题 + 说明 + 右侧紧凑按钮 ----
    auto* clearBtn = new ui::Button(this, wxID_ANY, ui::Glyph::None, "清空",
                                    ui::Button::Style::Text, "清空消息");
    clearBtn->SetCompact(true);
    clearBtn->SetOnClick([this] { Clear(); });

    auto* head = new ui::PanelHeader(this, "消息", "运行日志");
    head->SetTrailing(clearBtn);
    sizer->Add(head, 0, wxEXPAND);

    // ---- 文本区（原型 .console-body：等宽 11.5px）----
    m_text = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                            wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2 | wxTE_DONTWRAP | wxHSCROLL |
                                wxBORDER_NONE);
    m_text->SetFont(ui::MonoFont(ui::fontsize::kCaption));
    m_text->SetBackgroundColour(ui::kPanel);
    sizer->Add(m_text, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(ui::space::kLg));

    SetSizer(sizer);
}

void ConsolePanel::Append(const wxString& message)
{
    if (m_text == nullptr) return;

    const wxString stamp = wxDateTime::Now().Format("[%H:%M:%S] ");
    m_text->AppendText(stamp + message + "\n");
    TrimIfNeeded();
}

void ConsolePanel::Clear()
{
    if (m_text != nullptr) m_text->Clear();
}

void ConsolePanel::TrimIfNeeded()
{
    if (m_text == nullptr) return;

    const long lines = m_text->GetNumberOfLines();
    if (lines <= static_cast<long>(m_maxLines)) return;

    // 一次性砍掉前 1/4，避免每行都触发裁剪
    const long toRemove = std::max<long>(1, static_cast<long>(m_maxLines) / 4);
    long endPos = 0;

    for (long i = 0; i < toRemove && i < lines; ++i)
        endPos += m_text->GetLineLength(i) + 1;  // +1 为换行

    if (endPos > 0) m_text->Remove(0, endPos);
}

}  // namespace sf
