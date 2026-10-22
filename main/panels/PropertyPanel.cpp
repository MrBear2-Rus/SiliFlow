#include "PropertyPanel.h"

#include "core/Circuit.h"
#include "core/Component.h"
#include "core/Library.h"
#include "core/Project.h"
#include "core/Wire.h"
#include "core/sim/SimEngine.h"
#include "ui/UiKit.h"

#include <wx/propgrid/advprops.h>
#include <wx/propgrid/propgrid.h>
#include <wx/sizer.h>

namespace sf
{

PropertyPanel::PropertyPanel(wxWindow* parent, Services* services, wxWindowID id)
    : wxPanel(parent, id), m_services(services)
{
    SetBackgroundColour(ui::kPanel);

    auto* sizer = new wxBoxSizer(wxVERTICAL);

    // 内容标题（原型 .prop-head：13px/600 主文案 + 11.5px 三级色副标题）。
    m_header = new ui::PanelHeader(this, "未选中对象");
    sizer->Add(m_header, 0, wxEXPAND);

    m_grid = new wxPropertyGrid(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                wxPG_SPLITTER_AUTO_CENTER | wxPG_DEFAULT_STYLE | wxBORDER_NONE);
    m_grid->SetFont(ui::Font(ui::fontsize::kBody));
    m_grid->SetBackgroundColour(ui::kPanel);
    sizer->Add(m_grid, 1, wxEXPAND);

    // 底部动作条（原型 .prop-actions{gap:6px; padding:8px 12px 12px}）：纯文字扁平按钮。
    auto* strip = new wxBoxSizer(wxHORIZONTAL);
    m_rotateBtn = new ui::Button(this, wxID_ANY, ui::Glyph::None, "旋转 90°",
                                 ui::Button::Style::Text, "把选中元件顺时针旋转 90°");
    m_mirrorBtn = new ui::Button(this, wxID_ANY, ui::Glyph::None, "水平镜像",
                                 ui::Button::Style::Text, "把选中元件左右镜像");
    m_deleteBtn = new ui::Button(this, wxID_ANY, ui::Glyph::None, "删除",
                                 ui::Button::Style::Text, "删除选中对象", true);

    // 动作走 Services 的命令回调（和原来的按钮事件同一条路径），完成后按新选中刷新属性表。
    m_rotateBtn->SetOnClick([this] { if (m_services != nullptr) m_services->Rotate(); RefreshFromSelection(); });
    m_mirrorBtn->SetOnClick([this] { if (m_services != nullptr) m_services->Mirror(); RefreshFromSelection(); });
    m_deleteBtn->SetOnClick([this] { if (m_services != nullptr) m_services->Delete(); RefreshFromSelection(); });

    const int gap = FromDIP(ui::space::kSm);
    strip->Add(m_rotateBtn, 0, wxRIGHT, gap);
    strip->Add(m_mirrorBtn, 0, wxRIGHT, gap);
    strip->AddStretchSpacer(1);
    strip->Add(m_deleteBtn, 0);
    sizer->Add(strip, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, FromDIP(ui::space::kLg));

    SetSizer(sizer);

    m_grid->Bind(wxEVT_PG_CHANGED, &PropertyPanel::OnPropertyChanged, this);

    BuildEmpty();
}

// -----------------------------------------------------------------------------
//  刷新
// -----------------------------------------------------------------------------
void PropertyPanel::RefreshFromSelection()
{
    if (m_services == nullptr || m_services->project == nullptr)
    {
        BuildEmpty();
        return;
    }

    CircuitSheet* sheet = m_services->project->Active();
    if (sheet == nullptr)
    {
        BuildEmpty();
        return;
    }

    const auto comps = sheet->SelectedComponents();
    const auto wires = sheet->SelectedWires();

    if (comps.size() == 1 && wires.empty())
        BuildSingleComponent(comps.front()->id);
    else if (wires.size() == 1 && comps.empty())
        BuildSingleWire(wires.front()->id);
    else if (comps.empty() && wires.empty())
        BuildEmpty();
    else
        BuildMulti(comps.size(), wires.size());
}

void PropertyPanel::BuildEmpty()
{
    m_componentId = -1;
    m_wireId = -1;

    if (m_header != nullptr) m_header->SetTitle("未选中对象");

    if (m_grid == nullptr) return;
    m_grid->Freeze();
    m_grid->Clear();
    m_grid->Append(new wxPropertyCategory("提示"));
    m_grid->Append(new wxStringProperty("说明", wxPG_LABEL, "在画布上点选一个元件或导线"));
    m_grid->Thaw();

    if (m_rotateBtn) m_rotateBtn->Enable(false);
    if (m_mirrorBtn) m_mirrorBtn->Enable(false);
    if (m_deleteBtn) m_deleteBtn->Enable(false);
}

void PropertyPanel::BuildSingleComponent(int compId)
{
    CircuitSheet* sheet = m_services->project->Active();
    ComponentInstance* comp = sheet == nullptr ? nullptr : sheet->FindComponent(compId);
    if (comp == nullptr)
    {
        BuildEmpty();
        return;
    }

    m_componentId = compId;
    m_wireId = -1;

    const wxString typeName = (comp->def != nullptr) ? comp->def->name : comp->defType;
    if (m_header != nullptr) m_header->SetTitle(wxString::Format("元件：%s", typeName));

    m_grid->Freeze();
    m_grid->Clear();

    m_grid->Append(new wxPropertyCategory("基本"));
    wxPGProperty* typeProp = m_grid->Append(new wxStringProperty("类型", wxPG_LABEL, comp->defType));
    typeProp->ChangeFlag(wxPG_PROP_READONLY, true);
    m_grid->Append(new wxStringProperty("标签", "label", comp->label));
    m_grid->Append(new wxStringProperty("值", "value", comp->value));

    m_grid->Append(new wxPropertyCategory("位置与朝向"));
    m_grid->Append(new wxIntProperty("X", "posx", comp->pos.x));
    m_grid->Append(new wxIntProperty("Y", "posy", comp->pos.y));

    wxArrayString rotLabels;
    rotLabels.Add("0°");
    rotLabels.Add("90°");
    rotLabels.Add("180°");
    rotLabels.Add("270°");
    m_grid->Append(new wxEnumProperty("旋转", "rotation", rotLabels, wxArrayInt(), comp->rotation));

    m_grid->Append(new wxBoolProperty("水平镜像", "mirrored", comp->mirrored));

    // 数据位宽：1..32。改了要让 Rebuild 重新解析引脚位宽。
    wxPGProperty* widthProp =
        m_grid->Append(new wxIntProperty("位宽", "width", comp->width));
    widthProp->SetAttribute(wxPG_ATTR_MIN, 1);
    widthProp->SetAttribute(wxPG_ATTR_MAX, kMaxBusWidth);

    // ---- 仿真：只在单引脚元件上显示（输入/输出端口、常量、接地、时钟、指示灯）----
    // 多引脚元件不给，因为导线颜色已经说明了每个网络的状态。
    if (comp->pins.size() == 1 && m_services != nullptr && m_services->sim != nullptr)
    {
        sim::SimEngine& eng = *m_services->sim;

        m_grid->Append(new wxPropertyCategory("仿真"));

        wxPGProperty* cur = m_grid->Append(
            new wxStringProperty("当前电平", wxPG_LABEL, LogicSignalName(eng.PinSignal(comp->id, 0))));
        cur->ChangeFlag(wxPG_PROP_READONLY, true);

        if (eng.IsToggleable(comp->id))
        {
            LogicVector forced;
            const bool hasForced = eng.ForcedValue(comp->id, &forced);
            m_grid->Append(new wxBoolProperty("强制高电平", "force",
                                              hasForced && forced.Bit(0) == LogicSignal::One));
        }
    }

    if (comp->def != nullptr && !comp->def->gate.IsEmpty())
    {
        m_grid->Append(new wxPropertyCategory("Verilog"));
        wxPGProperty* gateProp = m_grid->Append(new wxStringProperty("原语", wxPG_LABEL, comp->def->gate));
        gateProp->ChangeFlag(wxPG_PROP_READONLY, true);
        wxPGProperty* kindProp = m_grid->Append(
            new wxStringProperty("类别", wxPG_LABEL, comp->def->kind.IsEmpty() ? wxString("-") : comp->def->kind));
        kindProp->ChangeFlag(wxPG_PROP_READONLY, true);
    }

    if (!comp->pins.empty())
    {
        m_grid->Append(new wxPropertyCategory("引脚"));
        for (size_t i = 0; i < comp->pins.size(); ++i)
        {
            const Point pp = comp->PinPos(static_cast<int>(i));
            wxPGProperty* p = m_grid->Append(new wxStringProperty(
                comp->pins[i].identifier, wxPG_LABEL,
                wxString::Format("%s  (%d, %d)", PinDirName(comp->pins[i].dir), pp.x, pp.y)));
            p->ChangeFlag(wxPG_PROP_READONLY, true);
        }
    }

    m_grid->Thaw();

    if (m_rotateBtn) m_rotateBtn->Enable(true);
    if (m_mirrorBtn) m_mirrorBtn->Enable(true);
    if (m_deleteBtn) m_deleteBtn->Enable(true);
}

void PropertyPanel::BuildSingleWire(int wireId)
{
    CircuitSheet* sheet = m_services->project->Active();
    Wire* wire = sheet == nullptr ? nullptr : sheet->FindWire(wireId);
    if (wire == nullptr)
    {
        BuildEmpty();
        return;
    }

    m_componentId = -1;
    m_wireId = wireId;

    if (m_header != nullptr) m_header->SetTitle(wxString::Format("导线：#%d", wire->id));

    m_grid->Freeze();
    m_grid->Clear();

    m_grid->Append(new wxPropertyCategory("基本"));
    wxPGProperty* lenProp =
        m_grid->Append(new wxIntProperty("总长（px）", wxPG_LABEL, wire->Length()));
    lenProp->ChangeFlag(wxPG_PROP_READONLY, true);
    wxPGProperty* segProp =
        m_grid->Append(new wxIntProperty("段数", wxPG_LABEL, wire->SegmentCount()));
    segProp->ChangeFlag(wxPG_PROP_READONLY, true);

    wxArrayString signalLabels;
    signalLabels.Add("0");
    signalLabels.Add("1");
    signalLabels.Add("X（不定）");
    signalLabels.Add("Z（高阻）");
    m_grid->Append(
        new wxEnumProperty("信号", "signal", signalLabels, wxArrayInt(), static_cast<int>(wire->signal)));

    m_grid->Append(new wxPropertyCategory("两端"));
    const wxString leftText = wire->left.Attached()
                                  ? wxString::Format("元件 #%d 引脚 %d", wire->left.compId, wire->left.pinIndex)
                                  : wxString("悬空");
    const wxString rightText = wire->right.Attached()
                                   ? wxString::Format("元件 #%d 引脚 %d", wire->right.compId, wire->right.pinIndex)
                                   : wxString("悬空");
    wxPGProperty* lp = m_grid->Append(new wxStringProperty("起点", wxPG_LABEL, leftText));
    lp->ChangeFlag(wxPG_PROP_READONLY, true);
    wxPGProperty* rp = m_grid->Append(new wxStringProperty("终点", wxPG_LABEL, rightText));
    rp->ChangeFlag(wxPG_PROP_READONLY, true);

    m_grid->Thaw();

    if (m_rotateBtn) m_rotateBtn->Enable(false);
    if (m_mirrorBtn) m_mirrorBtn->Enable(false);
    if (m_deleteBtn) m_deleteBtn->Enable(true);
}

void PropertyPanel::BuildMulti(size_t compCount, size_t wireCount)
{
    m_componentId = -1;
    m_wireId = -1;

    if (m_header != nullptr)
        m_header->SetTitle(wxString::Format("多选：%zu 个元件 / %zu 根导线", compCount, wireCount));

    m_grid->Freeze();
    m_grid->Clear();
    m_grid->Append(new wxPropertyCategory("多选"));
    m_grid->Append(new wxStringProperty("说明", wxPG_LABEL, "使用下方按钮批量操作，或单选查看详细属性"));
    m_grid->Thaw();

    if (m_rotateBtn) m_rotateBtn->Enable(compCount > 0);
    if (m_mirrorBtn) m_mirrorBtn->Enable(compCount > 0);
    if (m_deleteBtn) m_deleteBtn->Enable(true);
}

// -----------------------------------------------------------------------------
//  事件
// -----------------------------------------------------------------------------
void PropertyPanel::OnPropertyChanged(wxPropertyGridEvent& event)
{
    if (m_services == nullptr) return;

    const wxString name = event.GetPropertyName();
    wxString value = event.GetPropertyValue().GetString();

    // 枚举属性交上来的是序号，翻译回四值逻辑的 0/1/X/Z
    if (name == "signal")
    {
        long idx = 0;
        if (value.ToLong(&idx) && idx >= 0 && idx <= 3)
            value = LogicSignalName(static_cast<LogicSignal>(idx));
    }

    // 强制电平走仿真命令（不进撤销栈），不要落进下面的「改属性」分支
    if (name == "force" && m_componentId >= 0)
    {
        m_services->SimForcePin(m_componentId, event.GetPropertyValue().GetBool() ? "1" : "");
        return;
    }

    if (m_componentId >= 0)
    {
        if (name == "posx")
            m_services->EditComponent(m_componentId, "x", value);
        else if (name == "posy")
            m_services->EditComponent(m_componentId, "y", value);
        else if (!name.IsEmpty())
            m_services->EditComponent(m_componentId, name, value);
    }
    else if (m_wireId >= 0 && name == "signal")
    {
        m_services->EditWire(m_wireId, "signal", value);
    }
}

}  // namespace sf
