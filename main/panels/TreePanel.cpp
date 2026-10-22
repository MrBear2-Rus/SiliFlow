#include "TreePanel.h"

#include "core/Circuit.h"
#include "core/Component.h"
#include "core/Project.h"
#include "ui/UiKit.h"

#include <wx/sizer.h>

namespace sf
{

namespace
{

enum class NodeKind
{
    None,
    Sheet,
    Component,
    Net
};

class NodeData : public wxTreeItemData
{
public:
    NodeData(NodeKind kind, int id, wxString name) : m_kind(kind), m_id(id), m_name(std::move(name)) {}

    NodeKind Kind() const { return m_kind; }
    int Id() const { return m_id; }
    const wxString& Name() const { return m_name; }

private:
    NodeKind m_kind;
    int m_id;
    wxString m_name;
};

const NodeData* DataOf(const wxTreeItemData* d)
{
    return dynamic_cast<const NodeData*>(d);
}

}  // namespace

TreePanel::TreePanel(wxWindow* parent, Services* services, wxWindowID id)
    : wxPanel(parent, id), m_services(services)
{
    SetBackgroundColour(ui::kPanel);

    auto* sizer = new wxBoxSizer(wxVERTICAL);

    // ---- 内容标题 + 右侧“刷新”（原型 .panel-head + .hd-seg）----
    auto* refresh = new ui::Button(this, wxID_ANY, ui::Glyph::None, "刷新",
                                   ui::Button::Style::Text, "重新扫描工程并重建层次树");
    refresh->SetCompact(true);
    refresh->SetOnClick([this] { Rebuild(); });

    m_header = new ui::PanelHeader(this, "工程导航", "双击定位");
    m_header->SetTrailing(refresh);
    sizer->Add(m_header, 0, wxEXPAND);

    m_tree = new wxTreeCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                            wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT | wxTR_SINGLE | wxTR_HIDE_ROOT |
                                wxBORDER_NONE);
    m_tree->SetFont(ui::Font(ui::fontsize::kBody));
    m_tree->SetBackgroundColour(ui::kPanel);
    sizer->Add(m_tree, 1, wxEXPAND, FromDIP(ui::space::kMd));

    SetSizer(sizer);

    m_tree->Bind(wxEVT_TREE_ITEM_ACTIVATED, &TreePanel::OnItemActivated, this);

    Rebuild();
}

// -----------------------------------------------------------------------------
//  建树
// -----------------------------------------------------------------------------
void TreePanel::Rebuild()
{
    if (m_tree == nullptr) return;

    m_tree->Freeze();
    m_tree->DeleteAllItems();

    const wxTreeItemId root = m_tree->AddRoot("root");

    if (m_services == nullptr || m_services->project == nullptr)
    {
        m_tree->AppendItem(root, "（没有打开的工程）");
        m_tree->Thaw();
        return;
    }

    Project* project = m_services->project;
    const wxTreeItemId projectNode = m_tree->AppendItem(root, "工程：" + project->DisplayName());

    for (auto& sheetPtr : project->sheets)
    {
        CircuitSheet* sheet = sheetPtr.get();
        if (sheet == nullptr) continue;

        const wxString title = wxString::Format("图纸：%s  (%zu 元件 / %zu 导线)", sheet->name,
                                                sheet->components.size(), sheet->wires.size());
        const wxTreeItemId sheetNode = m_tree->AppendItem(projectNode, title, -1, -1, new NodeData(NodeKind::Sheet, 0, sheet->name));
        BuildSheetNode(sheetNode, *sheet);
    }

    m_tree->ExpandAll();
    m_tree->Thaw();
}

void TreePanel::BuildSheetNode(const wxTreeItemId& parent, CircuitSheet& sheet)
{
    // ---- 元件 ----
    const wxTreeItemId compRoot =
        m_tree->AppendItem(parent, wxString::Format("元件 (%zu)", sheet.components.size()));

    for (const auto& c : sheet.components)
    {
        const wxString typeName = (c->def != nullptr) ? c->def->name : c->defType;
        const wxString label =
            wxString::Format("%s : %s", c->label.IsEmpty() ? wxString("(无名)") : c->label, typeName);
        m_tree->AppendItem(compRoot, label, -1, -1, new NodeData(NodeKind::Component, c->id, c->label));
    }

    // ---- 网络 ----
    const std::vector<Net> nets = sheet.BuildNets();
    const wxTreeItemId netRoot = m_tree->AppendItem(parent, wxString::Format("网络 (%zu)", nets.size()));

    for (const Net& n : nets)
    {
        const wxString label = wxString::Format("%s  (%zu 引脚 / %zu 导线)", n.name, n.pins.size(),
                                                n.wireIds.size());
        m_tree->AppendItem(netRoot, label, -1, -1, new NodeData(NodeKind::Net, 0, n.name));
    }
}

void TreePanel::RebuildSheet(const wxString& WXUNUSED(sheetName))
{
    Rebuild();
}

// -----------------------------------------------------------------------------
//  事件
// -----------------------------------------------------------------------------
void TreePanel::OnItemActivated(wxTreeEvent& event)
{
    if (m_services == nullptr) return;

    const NodeData* data = DataOf(m_tree->GetItemData(event.GetItem()));
    if (data == nullptr) return;

    switch (data->Kind())
    {
    case NodeKind::Sheet:
        m_services->ActivateSheet(data->Name());
        break;

    case NodeKind::Component:
        m_services->FocusComponent(data->Id());
        break;

    case NodeKind::Net:
        m_services->HighlightNet(data->Name());
        break;

    default:
        break;
    }
}

}  // namespace sf
