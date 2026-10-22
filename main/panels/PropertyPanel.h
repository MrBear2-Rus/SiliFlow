#pragma once

// =============================================================================
//  SiliFlow · panels/PropertyPanel.h
//  属性面板：用 wxPropertyGrid 显示当前选中元件 / 导线的可编辑属性。
//
//  修改通过 Services 里的编辑命令回调下发给当前活动画布，因此本面板
//  不直接依赖 canvas/（守住 L3 内部不横向依赖的约定）。
// =============================================================================

#include "core/Services.h"

#include <wx/panel.h>
#include <wx/string.h>

class wxPropertyGrid;
class wxPropertyGridEvent;

namespace sf
{

namespace ui
{
class Button;
class PanelHeader;
}  // namespace ui

class PropertyPanel : public wxPanel
{
public:
    PropertyPanel(wxWindow* parent, Services* services, wxWindowID id = wxID_ANY);

    /// 依据当前选中集重建属性表。selection 变化时由 MainFrame 调用。
    void RefreshFromSelection();

private:
    void BuildEmpty();
    void BuildSingleComponent(int compId);
    void BuildSingleWire(int wireId);
    void BuildMulti(size_t compCount, size_t wireCount);

    void OnPropertyChanged(wxPropertyGridEvent& event);

    Services* m_services = nullptr;

    wxPropertyGrid* m_grid = nullptr;
    ui::PanelHeader* m_header = nullptr;
    ui::Button* m_rotateBtn = nullptr;
    ui::Button* m_mirrorBtn = nullptr;
    ui::Button* m_deleteBtn = nullptr;

    /// 当前属性表正在展示的对象
    int m_componentId = -1;
    int m_wireId = -1;
};

}  // namespace sf
