#pragma once

// =============================================================================
//  SiliFlow · panels/TreePanel.h
//  层次树：工程 → 图纸 → 元件 / 网络。
//  双击元件或网络可定位到画布（通过 Services 的命令回调，不横向依赖 canvas/）。
// =============================================================================

#include "core/Services.h"

#include <wx/panel.h>
#include <wx/string.h>
#include <wx/treectrl.h>

class wxTreeEvent;

namespace sf
{

namespace ui
{
class PanelHeader;
}  // namespace ui

class CircuitSheet;

class TreePanel : public wxPanel
{
public:
    TreePanel(wxWindow* parent, Services* services, wxWindowID id = wxID_ANY);

    /// 重新扫描工程并重建树
    void Rebuild();

    /// 只刷新某个图纸节点下的内容（当前实现直接整体重建，量级足够小）
    void RebuildSheet(const wxString& sheetName);

private:
    void BuildSheetNode(const wxTreeItemId& parent, CircuitSheet& sheet);

    void OnItemActivated(wxTreeEvent& event);

    Services* m_services = nullptr;
    wxTreeCtrl* m_tree = nullptr;
    ui::PanelHeader* m_header = nullptr;
};

}  // namespace sf
