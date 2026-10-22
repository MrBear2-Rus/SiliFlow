#include "ToolboxPanel.h"

#include "core/Library.h"
#include "ui/UiKit.h"

#include <wx/sizer.h>
#include <wx/srchctrl.h>
#include <wx/stattext.h>
#include <wx/treectrl.h>

namespace sf
{

namespace
{

/// 挂在树节点上的元件类型
class TypeItemData : public wxTreeItemData
{
public:
    explicit TypeItemData(wxString type) : m_type(std::move(type)) {}

    const wxString& Type() const { return m_type; }

private:
    wxString m_type;
};

const wxString& NodeType(const wxTreeItemData* data)
{
    static const wxString kEmpty;
    const auto* d = dynamic_cast<const TypeItemData*>(data);
    return d == nullptr ? kEmpty : d->Type();
}

}  // namespace

ToolboxPanel::ToolboxPanel(wxWindow* parent, Services* services, wxWindowID id)
    : wxPanel(parent, id), m_services(services)
{
    SetBackgroundColour(ui::kPanel);

    auto* sizer = new wxBoxSizer(wxVERTICAL);

    // ---- 搜索框（原型 .search-wrap{padding:8px 10px 4px} + .search{height:30px}）----
    auto* searchRow = new wxBoxSizer(wxHORIZONTAL);
    m_search = new wxSearchCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                wxSize(-1, FromDIP(ui::metric::kSearchHeight)),
                                wxTE_PROCESS_ENTER | wxBORDER_NONE);
    m_search->ShowCancelButton(true);
    m_search->SetDescriptiveText("搜索元件…");
    m_search->SetFont(ui::Font(ui::fontsize::kBody));
    searchRow->Add(m_search, 1, wxALIGN_CENTER_VERTICAL);
    sizer->Add(searchRow, 0, wxEXPAND | wxLEFT | wxRIGHT,
               FromDIP(ui::space::kLg));

    // ---- 分类树（面板体本身不加内边距，交给树控件）----
    m_tree = new wxTreeCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                            wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT | wxTR_SINGLE | wxTR_HIDE_ROOT |
                                wxBORDER_NONE);
    m_tree->SetFont(ui::Font(ui::fontsize::kBody));
    m_tree->SetBackgroundColour(ui::kPanel);
    sizer->Add(m_tree, 1, wxEXPAND | wxTOP, FromDIP(ui::space::kXs));

    // ---- 底部提示（原型 .toolbox-hint：上方一条发丝线 + 11.5px 三级色）----
    sizer->Add(ui::MakeSeparator(this, -1, false), 0, wxEXPAND);
    m_hint = new wxStaticText(this, wxID_ANY, "单击元件进入放置模式，再点击画布即可放置。");
    m_hint->SetFont(ui::Font(ui::fontsize::kCaption));
    m_hint->SetForegroundColour(ui::kTertiary);
    sizer->Add(m_hint, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP | wxBOTTOM,
               FromDIP(ui::space::kMd));

    SetSizer(sizer);

    m_tree->Bind(wxEVT_TREE_SEL_CHANGED, &ToolboxPanel::OnSelectionChanged, this);
    m_tree->Bind(wxEVT_TREE_ITEM_ACTIVATED, &ToolboxPanel::OnItemActivated, this);
    m_search->Bind(wxEVT_TEXT, &ToolboxPanel::OnSearchText, this);
    m_search->Bind(wxEVT_SEARCH_CANCEL,
                   [this](wxCommandEvent&)
                   {
                       m_search->ChangeValue(wxEmptyString);
                       BuildTree(wxEmptyString);
                   });

    Rebuild();
}

void ToolboxPanel::Rebuild()
{
    BuildTree(m_search != nullptr ? m_search->GetValue() : wxString(wxEmptyString));
}

void ToolboxPanel::BuildTree(const wxString& filter)
{
    if (m_tree == nullptr) return;

    m_tree->Freeze();
    m_tree->DeleteAllItems();

    const wxTreeItemId root = m_tree->AddRoot("元件库");

    if (m_services == nullptr || m_services->library == nullptr)
    {
        m_tree->AppendItem(root, "（元件库未加载）");
        m_tree->Thaw();
        return;
    }

    Library* lib = m_services->library;

    if (!filter.IsEmpty())
    {
        // 搜索结果平铺到一个节点下
        const std::vector<const ComponentDef*> hits = lib->Search(filter);
        if (hits.empty())
            m_tree->AppendItem(root, "（没有匹配的元件）");
        else
            AddCategoryNode("搜索结果 (" + wxString::Format("%zu", hits.size()) + ")", hits);

        m_tree->ExpandAll();
        m_tree->Thaw();
        return;
    }

    for (const wxString& category : lib->Categories()) AddCategoryNode(category, lib->ByCategory(category));

    m_tree->ExpandAll();
    m_tree->Thaw();
}

void ToolboxPanel::AddCategoryNode(const wxString& category, const std::vector<const ComponentDef*>& defs)
{
    const wxTreeItemId root = m_tree->GetRootItem();
    const wxTreeItemId cat = m_tree->AppendItem(root, category);

    for (const ComponentDef* d : defs)
    {
        const wxString label = d->name + "  (" + d->type + ")";
        m_tree->AppendItem(cat, label, -1, -1, new TypeItemData(d->type));
    }
}

void ToolboxPanel::SelectType(const wxString& type)
{
    if (m_tree == nullptr || type.IsEmpty()) return;

    const wxTreeItemId root = m_tree->GetRootItem();
    wxTreeItemIdValue cookie;
    for (wxTreeItemId cat = m_tree->GetFirstChild(root, cookie); cat.IsOk();
         cat = m_tree->GetNextChild(root, cookie))
    {
        wxTreeItemIdValue leafCookie;
        for (wxTreeItemId leaf = m_tree->GetFirstChild(cat, leafCookie); leaf.IsOk();
             leaf = m_tree->GetNextChild(cat, leafCookie))
        {
            if (NodeType(m_tree->GetItemData(leaf)) == type)
            {
                m_tree->SelectItem(leaf);
                m_tree->EnsureVisible(leaf);
                return;
            }
        }
    }
}

void ToolboxPanel::OnSelectionChanged(wxTreeEvent& event)
{
    const wxString type = NodeType(m_tree->GetItemData(event.GetItem()));
    if (type.IsEmpty()) return;  // 选中的是分类节点

    if (m_onPick) m_onPick(type);
}

void ToolboxPanel::OnItemActivated(wxTreeEvent& event)
{
    const wxString type = NodeType(m_tree->GetItemData(event.GetItem()));
    if (type.IsEmpty()) return;

    if (m_onPick) m_onPick(type);
}

void ToolboxPanel::OnSearchText(wxCommandEvent& WXUNUSED(event))
{
    BuildTree(m_search->GetValue());
}

}  // namespace sf
