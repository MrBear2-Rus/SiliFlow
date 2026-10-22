#include "CanvasNoteBook.h"

#include "CanvasPanel.h"
#include "core/Project.h"
#include "ui/UiKit.h"

#include <wx/menu.h>
#include <wx/textdlg.h>

namespace sf
{

namespace
{
enum
{
    ID_NEW_SHEET = wxID_HIGHEST + 700,
    ID_RENAME_SHEET,
    ID_CLOSE_SHEET
};
}

CanvasNoteBook::CanvasNoteBook(wxWindow* parent, Services* services)
    : wxAuiNotebook(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                    wxAUI_NB_TOP | wxAUI_NB_TAB_MOVE | wxAUI_NB_TAB_SPLIT | wxAUI_NB_CLOSE_ON_ALL_TABS |
                        wxAUI_NB_WINDOWLIST_BUTTON),
      m_services(services)
{
    // 页签外观统一走 ui::MakeTabArt()（原型 .tabs：35px 高、激活页白底圆角上沿）。
    SetArtProvider(ui::MakeTabArt());
    SetTabCtrlHeight(FromDIP(ui::metric::kTabBarHeight));
    SetBackgroundColour(ui::kPanel);

    Bind(wxEVT_AUINOTEBOOK_PAGE_CHANGED, &CanvasNoteBook::OnPageChanged, this);
    Bind(wxEVT_AUINOTEBOOK_PAGE_CLOSE, &CanvasNoteBook::OnPageClose, this);
    Bind(wxEVT_AUINOTEBOOK_TAB_RIGHT_UP, &CanvasNoteBook::OnTabRightUp, this);
}

// -----------------------------------------------------------------------------
//  建页 / 重建
// -----------------------------------------------------------------------------
void CanvasNoteBook::RebuildFromProject()
{
    Freeze();

    while (GetPageCount() > 0) DeletePage(0);
    m_pages.clear();

    if (m_services != nullptr && m_services->project != nullptr)
    {
        for (const auto& sheet : m_services->project->sheets)
        {
            CanvasPanel* canvas = new CanvasPanel(this, m_services);
            canvas->SetSheet(sheet.get());
            AddPage(canvas, sheet->name, true);
            m_pages.emplace_back(sheet->name, canvas);
        }

        if (!m_pages.empty())
        {
            Project* p = m_services->project;
            SetActiveSheet(p->activeSheetName);
        }
    }

    Thaw();

    if (m_services != nullptr) m_services->NotifyActiveSheetChanged();
}

// -----------------------------------------------------------------------------
//  查询
// -----------------------------------------------------------------------------
CanvasPanel* CanvasNoteBook::CanvasFor(const wxString& sheetName) const
{
    for (const auto& e : m_pages)
        if (e.first == sheetName) return e.second;
    return nullptr;
}

CanvasPanel* CanvasNoteBook::ActiveCanvas() const
{
    const int sel = GetSelection();
    if (sel < 0 || sel >= static_cast<int>(m_pages.size())) return nullptr;
    return m_pages[sel].second;
}

wxString CanvasNoteBook::ActiveSheetName() const
{
    const int sel = GetSelection();
    if (sel < 0 || sel >= static_cast<int>(m_pages.size())) return wxString();
    return m_pages[sel].first;
}

bool CanvasNoteBook::SetActiveSheet(const wxString& sheetName)
{
    for (size_t i = 0; i < m_pages.size(); ++i)
    {
        if (m_pages[i].first == sheetName)
        {
            if (GetSelection() != static_cast<int>(i)) SetSelection(static_cast<int>(i));

            if (m_services != nullptr && m_services->project != nullptr)
                m_services->project->activeSheetName = sheetName;
            return true;
        }
    }
    return false;
}

// -----------------------------------------------------------------------------
//  新建
// -----------------------------------------------------------------------------
void CanvasNoteBook::CreateNewSheet()
{
    if (m_services == nullptr || m_services->project == nullptr) return;

    CircuitSheet* sheet = m_services->project->AddSheet("sheet");
    if (sheet == nullptr) return;

    CanvasPanel* canvas = new CanvasPanel(this, m_services);
    canvas->SetSheet(sheet);
    AddPage(canvas, sheet->name, true);
    m_pages.emplace_back(sheet->name, canvas);
    SetSelection(GetPageCount() - 1);

    m_services->project->activeSheetName = sheet->name;
    m_services->Log("新建图纸：" + sheet->name);
    m_services->NotifyActiveSheetChanged();
    m_services->NotifySheetChanged();
}

void CanvasNoteBook::CloseSheetByName(const wxString& name)
{
    if (m_services == nullptr || m_services->project == nullptr) return;

    if (m_services->project->sheets.size() <= 1)
    {
        m_services->Log("至少保留一张图纸，无法关闭：" + name);
        return;
    }

    int pageIndex = -1;
    for (size_t i = 0; i < m_pages.size(); ++i)
        if (m_pages[i].first == name) pageIndex = static_cast<int>(i);

    if (pageIndex < 0) return;

    DeletePage(static_cast<size_t>(pageIndex));
    m_pages.erase(m_pages.begin() + pageIndex);

    m_services->project->RemoveSheet(name);
    m_services->Log("已关闭图纸：" + name);
    m_services->NotifySheetChanged();
    m_services->NotifyActiveSheetChanged();
}

// -----------------------------------------------------------------------------
//  事件
// -----------------------------------------------------------------------------
void CanvasNoteBook::OnPageChanged(wxAuiNotebookEvent& event)
{
    const int sel = event.GetSelection();
    if (sel >= 0 && sel < static_cast<int>(m_pages.size()) && m_services != nullptr &&
        m_services->project != nullptr)
    {
        m_services->project->activeSheetName = m_pages[sel].first;
        m_services->NotifyActiveSheetChanged();
    }
    event.Skip();
}

void CanvasNoteBook::OnPageClose(wxAuiNotebookEvent& event)
{
    const int sel = event.GetSelection();

    // 页面删除必须延后到事件处理结束之后，否则会在事件中析构自己
    if (sel >= 0 && sel < static_cast<int>(m_pages.size()))
    {
        const wxString name = m_pages[sel].first;
        if (m_services != nullptr && m_services->project != nullptr &&
            m_services->project->sheets.size() <= 1)
        {
            m_services->Log("至少保留一张图纸，无法关闭：" + name);
            event.Veto();
            return;
        }

        event.Veto();  // 统一走 CallAfter 里的删除流程
        CallAfter(
            [this, name]
            {
                CloseSheetByName(name);
            });
    }
}

void CanvasNoteBook::OnTabRightUp(wxAuiNotebookEvent& event)
{
    m_contextPage = event.GetSelection();

    wxMenu menu;
    menu.Append(ID_NEW_SHEET, "新建图纸");
    menu.AppendSeparator();
    menu.Append(ID_RENAME_SHEET, "重命名图纸…");
    menu.Append(ID_CLOSE_SHEET, "关闭图纸");

    const int chosen = GetPopupMenuSelectionFromUser(menu);
    if (chosen == wxID_NONE) return;

    if (chosen == ID_NEW_SHEET)
    {
        CreateNewSheet();
        return;
    }

    if (m_contextPage < 0 || m_contextPage >= static_cast<int>(m_pages.size())) return;
    const wxString name = m_pages[m_contextPage].first;

    if (chosen == ID_CLOSE_SHEET)
        CloseSheetByName(name);
    else if (chosen == ID_RENAME_SHEET && m_services != nullptr && m_services->project != nullptr)
    {
        const wxString newName = wxGetTextFromUser("新的图纸名：", "重命名图纸", name, this);
        if (!newName.IsEmpty() && newName != name && m_services->project->Sheet(newName) == nullptr)
        {
            if (CircuitSheet* s = m_services->project->Sheet(name))
            {
                s->name = newName;
                SetPageText(static_cast<size_t>(m_contextPage), newName);
                m_pages[m_contextPage].first = newName;
                m_services->project->activeSheetName = newName;
                m_services->NotifySheetChanged();
                m_services->NotifyActiveSheetChanged();
            }
        }
    }
}

}  // namespace sf
