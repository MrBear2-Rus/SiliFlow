#pragma once

// =============================================================================
//  SiliFlow · panels/ToolboxPanel.h
//  元件库面板：按分类列出元件、支持搜索，单击即进入放置模式。
//
//  面板不直接持有画布，改用回调把「选中了哪个元件」交给 MainFrame，
//  由 MainFrame 转给当前活动图纸的 CanvasPanel。这样 panels/ 与 canvas/
//  之间没有横向依赖。
// =============================================================================

#include "core/Services.h"

#include <wx/panel.h>
#include <wx/string.h>

#include <functional>
#include <vector>

class wxTreeCtrl;
class wxTreeEvent;
class wxSearchCtrl;
class wxCommandEvent;
class wxStaticText;

namespace sf
{

class ComponentDef;

class ToolboxPanel : public wxPanel
{
public:
    ToolboxPanel(wxWindow* parent, Services* services, wxWindowID id = wxID_ANY);

    /// 用户选中某个元件类型时回调（参数是 ComponentDef::type）
    void SetPickHandler(std::function<void(const wxString&)> handler) { m_onPick = std::move(handler); }

    /// 依据元件库重建分类树
    void Rebuild();

    /// 外部（如菜单）想让面板高亮某类型时调用
    void SelectType(const wxString& type);

private:
    void BuildTree(const wxString& filter);
    void AddCategoryNode(const wxString& category, const std::vector<const ComponentDef*>& defs);

    void OnSelectionChanged(wxTreeEvent& event);
    void OnItemActivated(wxTreeEvent& event);
    void OnSearchText(wxCommandEvent& event);

    Services* m_services = nullptr;

    wxSearchCtrl* m_search = nullptr;
    wxTreeCtrl* m_tree = nullptr;
    wxStaticText* m_hint = nullptr;

    std::function<void(const wxString&)> m_onPick;
};

}  // namespace sf
