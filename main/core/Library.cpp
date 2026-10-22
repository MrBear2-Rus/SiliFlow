#include "Library.h"

#include "Types.h"

#include <wx/file.h>
#include <wx/filename.h>

#include <algorithm>

namespace sf
{

// =============================================================================
//  内置元件的外观构造助手
// =============================================================================
namespace
{

/// 与门/与非门本体：左边竖线 + 上下横线 + 右侧半圆（半径 = h/2）。
/// 要求 w == 1.5 * h，右侧半圆的顶点恰好落在 x = w 处。
void AddAndBody(ComponentDef& d, int w, int h)
{
    const int r = h / 2;
    const int straightX = w - r;

    d.shapes.push_back(MakeLine(Point(0, 0), Point(straightX, 0)));
    d.shapes.push_back(MakeLine(Point(0, h), Point(straightX, h)));
    d.shapes.push_back(MakeLine(Point(0, 0), Point(0, h)));
    d.shapes.push_back(MakeArc(Point(straightX, r), r, -90.0, 90.0));
}

/// 或门/异或门本体：上下两条汇聚到 (w, h/2) 的曲线 + 右侧凹背弧。
void AddOrBody(ComponentDef& d, int w, int h)
{
    const int midY = h / 2;
    const int cx1 = w / 4;
    const int cx2 = (w * 3) / 5;

    d.shapes.push_back(MakeBezier(Point(0, 0), Point(cx1, h / 8), Point(cx2, h / 3), Point(w, midY)));
    d.shapes.push_back(MakeBezier(Point(0, h), Point(cx1, h - h / 8), Point(cx2, h - h / 3), Point(w, midY)));
    d.shapes.push_back(MakeBezier(Point(0, 0), Point(w / 5, h / 4), Point(w / 5, (h * 3) / 4), Point(0, h)));
}

/// 异或门比或门多一条左侧背弧。
void AddXorBackArc(ComponentDef& d, int h)
{
    d.shapes.push_back(MakeBezier(Point(-6, 0), Point(8, h / 4), Point(8, (h * 3) / 4), Point(-6, h)));
}

/// 输出端的反相小圆圈。
void AddBubble(ComponentDef& d, int w, int h)
{
    d.shapes.push_back(MakeCircle(Point(w + 4, h / 2), 4));
}

/// 二输入 / 三输入引脚的通用摆放
void AddGatePins(ComponentDef& d, int outX, bool inverting, bool threeInput)
{
    if (threeInput)
    {
        d.inputs.push_back(Pin{Point(0, 10), PinDir::Input, "A", 0, false});
        d.inputs.push_back(Pin{Point(0, 30), PinDir::Input, "B", 1, false});
        d.inputs.push_back(Pin{Point(0, 50), PinDir::Input, "C", 2, false});
    }
    else
    {
        d.inputs.push_back(Pin{Point(0, 15), PinDir::Input, "A", 0, false});
        d.inputs.push_back(Pin{Point(0, 45), PinDir::Input, "B", 1, false});
    }

    Pin y;
    y.pos = Point(outX, d.size.y / 2);
    y.dir = PinDir::Output;
    y.identifier = "Y";
    y.index = 0;
    y.inverted = inverting;
    d.outputs.push_back(y);
}

// ---- 各类门的整体构造 ----

std::unique_ptr<ComponentDef> MakeGate(const wxString& type, const wxString& name, const wxString& gate,
                                       const wxString& shapeKind, bool inverting, bool threeInput)
{
    auto d = std::make_unique<ComponentDef>();
    d->type = type;
    d->name = name;
    d->category = "逻辑门";
    d->gate = gate;
    d->description = name + wxString("（") + gate + "）";

    const int h = 60;
    const int w = 80;
    const int outX = inverting ? w + 8 : w;
    const int boxW = inverting ? w + 8 : w;

    d->size = wxSize(boxW, h);

    if (shapeKind == "and")
        AddAndBody(*d, w, h);
    else
    {
        AddOrBody(*d, w, h);
        if (shapeKind == "xor") AddXorBackArc(*d, h);
    }

    if (inverting) AddBubble(*d, w, h);

    AddGatePins(*d, outX, inverting, threeInput);
    return d;
}

std::unique_ptr<ComponentDef> MakeBuffer(const wxString& type, const wxString& name, bool inverting)
{
    auto d = std::make_unique<ComponentDef>();
    d->type = type;
    d->name = name;
    d->category = "逻辑门";
    d->gate = type;
    d->description = name;

    const int h = 50;
    const int apex = 50;
    const int w = inverting ? 62 : 52;
    d->size = wxSize(w, h);

    d->shapes.push_back(MakePoly({Point(0, 0), Point(apex, h / 2), Point(0, h)}, true));
    if (inverting) d->shapes.push_back(MakeCircle(Point(apex + 5, h / 2), 5));

    d->inputs.push_back(Pin{Point(0, h / 2), PinDir::Input, "A", 0, false});
    d->outputs.push_back(Pin{Point(inverting ? apex + 10 : apex, h / 2), PinDir::Output, "Y", 0, false});
    return d;
}

std::unique_ptr<ComponentDef> MakePort(const wxString& type, const wxString& name, bool isInput)
{
    auto d = std::make_unique<ComponentDef>();
    d->type = type;
    d->name = name;
    d->category = "输入输出";
    d->kind = "port";
    d->description = name;
    d->size = wxSize(40, 30);

    if (isInput)
    {
        d->shapes.push_back(MakePoly({Point(0, 0), Point(30, 0), Point(40, 15), Point(30, 30), Point(0, 30)}, true));
        d->outputs.push_back(Pin{Point(40, 15), PinDir::Output, "Y", 0, false});
    }
    else
    {
        d->shapes.push_back(MakePoly({Point(40, 0), Point(10, 0), Point(0, 15), Point(10, 30), Point(40, 30)}, true));
        d->inputs.push_back(Pin{Point(0, 15), PinDir::Input, "A", 0, false});
    }
    return d;
}

std::unique_ptr<ComponentDef> MakeConst(const wxString& type, const wxString& name, const wxString& value)
{
    auto d = std::make_unique<ComponentDef>();
    d->type = type;
    d->name = name;
    d->category = "输入输出";
    d->kind = "const";
    d->description = name;
    d->size = wxSize(40, 24);

    d->shapes.push_back(MakeRect(Rect(0, 0, 40, 24)));
    d->shapes.push_back(MakeText(Point(20, 12), value, 12, palette::kText, true));
    d->outputs.push_back(Pin{Point(40, 12), PinDir::Output, "Y", 0, false, 1});
    return d;
}

}  // namespace

// =============================================================================
//  Library
// =============================================================================
Library::Library() = default;

bool Library::Add(std::unique_ptr<ComponentDef> def)
{
    if (def == nullptr || def->type.IsEmpty()) return false;

    const std::string key = ToUtf8(def->type);
    const bool overwrite = m_defs.find(key) != m_defs.end();
    if (!overwrite) m_order.push_back(def->type);

    m_defs[key] = std::move(def);
    return overwrite;
}

bool Library::Remove(const wxString& type)
{
    const std::string key = ToUtf8(type);
    const auto it = m_defs.find(key);
    if (it == m_defs.end()) return false;

    m_defs.erase(it);
    m_order.erase(std::remove(m_order.begin(), m_order.end(), type), m_order.end());
    return true;
}

const ComponentDef* Library::Find(const wxString& type) const
{
    const auto it = m_defs.find(ToUtf8(type));
    return it == m_defs.end() ? nullptr : it->second.get();
}

std::vector<const ComponentDef*> Library::All() const
{
    std::vector<const ComponentDef*> out;
    out.reserve(m_order.size());
    for (const wxString& t : m_order)
    {
        const auto it = m_defs.find(ToUtf8(t));
        if (it != m_defs.end()) out.push_back(it->second.get());
    }
    return out;
}

std::vector<wxString> Library::Categories() const
{
    std::vector<wxString> cats;
    for (const ComponentDef* d : All())
        if (std::find(cats.begin(), cats.end(), d->category) == cats.end()) cats.push_back(d->category);
    return cats;
}

std::vector<const ComponentDef*> Library::ByCategory(const wxString& category) const
{
    std::vector<const ComponentDef*> out;
    for (const ComponentDef* d : All())
        if (d->category == category) out.push_back(d);
    return out;
}

std::vector<const ComponentDef*> Library::Search(const wxString& keyword) const
{
    std::vector<const ComponentDef*> out;
    if (keyword.IsEmpty()) return All();

    for (const ComponentDef* d : All())
    {
        if (d->type.Contains(keyword) || d->name.Contains(keyword) || d->category.Contains(keyword) ||
            d->gate.Contains(keyword))
            out.push_back(d);
    }
    return out;
}

Json Library::ToJson() const
{
    Json arr = Json::array();
    for (const ComponentDef* d : All()) arr.push_back(d->ToJson());
    return arr;
}

bool Library::LoadFromString(const std::string& utf8, bool merge, wxString* err)
{
    Json root;
    try
    {
        root = Json::parse(utf8);
    }
    catch (const std::exception& e)
    {
        if (err != nullptr) *err = wxString("JSON 解析失败：") + e.what();
        return false;
    }

    const Json* list = nullptr;
    if (root.is_array())
        list = &root;
    else if (root.is_object())
    {
        if (Has(root, "elements")) list = &root["elements"];
        else if (Has(root, "components")) list = &root["components"];
    }

    if (list == nullptr || !list->is_array())
    {
        if (err != nullptr) *err = "JSON 里找不到元件数组（应为数组，或含 elements/components 字段的对象）";
        return false;
    }

    if (!merge) Clear();

    int ok = 0, bad = 0;
    for (const Json& e : *list)
    {
        auto def = ComponentDef::FromJson(e);
        if (def == nullptr)
        {
            ++bad;
            continue;
        }
        Add(std::move(def));
        ++ok;
    }

    if (ok == 0)
    {
        if (err != nullptr) *err = wxString::Format("没有读到任何有效元件（跳过 %d 条）", bad);
        return false;
    }
    return true;
}

bool Library::LoadFromFile(const wxString& path, bool merge, wxString* err)
{
    if (!wxFileExists(path))
    {
        if (err != nullptr) *err = "文件不存在：" + path;
        return false;
    }

    wxFile file;
    if (!file.Open(path, wxFile::read))
    {
        if (err != nullptr) *err = "无法打开文件：" + path;
        return false;
    }

    const wxFileOffset len = file.Length();
    std::string buf;
    buf.resize(static_cast<size_t>(std::max<wxFileOffset>(0, len)));
    if (len > 0) file.Read(&buf[0], static_cast<size_t>(len));
    file.Close();

    return LoadFromString(buf, merge, err);
}

bool Library::SaveToFile(const wxString& path, wxString* err) const
{
    const std::string text = ToJson().dump(2);

    wxFile file;
    if (!file.Create(path, true) || !file.Open(path, wxFile::write))
    {
        if (err != nullptr) *err = "无法写入文件：" + path;
        return false;
    }

    const bool ok = file.Write(text.c_str(), text.size()) == text.size();
    file.Close();

    if (!ok && err != nullptr) *err = "写入不完整：" + path;
    return ok;
}

// =============================================================================
//  内置元件库
// =============================================================================
std::unique_ptr<Library> Library::CreateBuiltin()
{
    auto lib = std::make_unique<Library>();

    // ---- 逻辑门 ----
    lib->Add(MakeGate("and", "与门", "and", "and", false, false));
    lib->Add(MakeGate("and3", "三输入与门", "and", "and", false, true));
    lib->Add(MakeGate("nand", "与非门", "nand", "and", true, false));
    lib->Add(MakeGate("nand3", "三输入与非门", "nand", "and", true, true));
    lib->Add(MakeGate("or", "或门", "or", "or", false, false));
    lib->Add(MakeGate("or3", "三输入或门", "or", "or", false, true));
    lib->Add(MakeGate("nor", "或非门", "nor", "or", true, false));
    lib->Add(MakeGate("nor3", "三输入或非门", "nor", "or", true, true));
    lib->Add(MakeGate("xor", "异或门", "xor", "xor", false, false));
    lib->Add(MakeGate("xor3", "三输入异或门", "xor", "xor", false, true));
    lib->Add(MakeGate("xnor", "同或门", "xnor", "xor", true, false));
    lib->Add(MakeBuffer("not", "非门", true));
    lib->Add(MakeBuffer("buf", "缓冲器", false));

    // ---- 输入输出 ----
    lib->Add(MakePort("input", "输入引脚", true));
    lib->Add(MakePort("output", "输出引脚", false));
    lib->Add(MakeConst("const0", "常量 0", "0"));
    lib->Add(MakeConst("const1", "常量 1", "1"));

    // 接地
    {
        auto d = std::make_unique<ComponentDef>();
        d->type = "ground";
        d->name = "接地";
        d->category = "输入输出";
        d->kind = "const";
        d->description = "逻辑地（常量 0）";
        d->size = wxSize(30, 30);
        d->shapes.push_back(MakeLine(Point(15, 0), Point(15, 18)));
        d->shapes.push_back(MakeLine(Point(2, 18), Point(28, 18)));
        d->shapes.push_back(MakeLine(Point(7, 23), Point(23, 23)));
        d->shapes.push_back(MakeLine(Point(12, 28), Point(18, 28)));
        // 接地是「常量 0 驱动源」，引脚必须是输出 —— 否则仿真会把它当成负载，
        // 网络就没有驱动了。绘制位置与原来一致。
        d->outputs.push_back(Pin{Point(15, 0), PinDir::Output, "Y", 0, false, 1});
        lib->Add(std::move(d));
    }

    // 指示灯
    {
        auto d = std::make_unique<ComponentDef>();
        d->type = "led";
        d->name = "指示灯";
        d->category = "输入输出";
        d->kind = "led";
        d->description = "高电平点亮的指示灯（纯显示，无 Verilog 映射）";
        d->size = wxSize(30, 40);
        d->shapes.push_back(MakeLine(Point(15, 40), Point(15, 30)));
        d->shapes.push_back(MakeCircle(Point(15, 20), 10));
        d->inputs.push_back(Pin{Point(15, 40), PinDir::Input, "A", 0, false, 1});
        lib->Add(std::move(d));
    }

    // 时钟源
    {
        auto d = std::make_unique<ComponentDef>();
        d->type = "clock";
        d->name = "时钟源";
        d->category = "输入输出";
        d->kind = "clock";
        d->description = "时钟信号源";
        d->size = wxSize(60, 40);
        d->shapes.push_back(MakeRect(Rect(0, 0, 60, 40)));
        d->shapes.push_back(MakePoly({Point(6, 30), Point(16, 30), Point(16, 10), Point(28, 10), Point(28, 30),
                                      Point(40, 30), Point(40, 10), Point(52, 10), Point(52, 30)},
                                     false));
        d->outputs.push_back(Pin{Point(60, 20), PinDir::Output, "Y", 0, false, 1});
        lib->Add(std::move(d));
    }

    // ---- 常用器件 ----
    // 二选一多路器
    {
        auto d = std::make_unique<ComponentDef>();
        d->type = "mux2";
        d->name = "二选一多路器";
        d->category = "常用器件";
        d->gate = "mux2";
        d->description = "S=0 选 A，S=1 选 B";
        d->size = wxSize(60, 80);
        d->shapes.push_back(
            MakePoly({Point(0, 0), Point(40, 0), Point(50, 20), Point(50, 60), Point(40, 80), Point(0, 80)}, true));
        d->shapes.push_back(MakeText(Point(22, 40), "MUX", 8, palette::kTextDim, true));
        d->inputs.push_back(Pin{Point(0, 15), PinDir::Input, "A", 0, false});
        d->inputs.push_back(Pin{Point(0, 65), PinDir::Input, "B", 1, false});
        d->inputs.push_back(Pin{Point(20, 80), PinDir::Input, "S", 2, false, 1});
        d->outputs.push_back(Pin{Point(50, 40), PinDir::Output, "Y", 0, false});
        lib->Add(std::move(d));
    }

    // D 触发器
    {
        auto d = std::make_unique<ComponentDef>();
        d->type = "dff";
        d->name = "D 触发器";
        d->category = "常用器件";
        d->kind = "dff";
        d->description = "上升沿触发的 D 触发器";
        d->size = wxSize(70, 80);
        d->shapes.push_back(MakeRect(Rect(0, 0, 70, 80)));
        d->shapes.push_back(MakePoly({Point(0, 58), Point(10, 63), Point(0, 68)}, false));
        d->shapes.push_back(MakeText(Point(35, 20), "D", 9, palette::kTextDim, true));
        d->shapes.push_back(MakeText(Point(58, 20), "Q", 9, palette::kTextDim, true));
        d->inputs.push_back(Pin{Point(0, 20), PinDir::Input, "D", 0, false});
        d->inputs.push_back(Pin{Point(0, 63), PinDir::Input, "CLK", 1, false, 1});
        d->outputs.push_back(Pin{Point(70, 20), PinDir::Output, "Q", 0, false});
        d->outputs.push_back(Pin{Point(70, 63), PinDir::Output, "QN", 1, false});
        lib->Add(std::move(d));
    }

    // 全加器
    {
        auto d = std::make_unique<ComponentDef>();
        d->type = "addr_full";
        d->name = "全加器";
        d->category = "常用器件";
        d->gate = "add";
        d->description = "一位全加器：S = A ^ B ^ Cin，Cout = 多数";
        d->size = wxSize(70, 80);
        d->shapes.push_back(MakeRect(Rect(0, 0, 70, 80)));
        d->shapes.push_back(MakeText(Point(35, 40), "FA", 11, palette::kTextDim, true));
        d->inputs.push_back(Pin{Point(0, 15), PinDir::Input, "A", 0, false, 1});
        d->inputs.push_back(Pin{Point(0, 40), PinDir::Input, "B", 1, false, 1});
        d->inputs.push_back(Pin{Point(0, 65), PinDir::Input, "CIN", 2, false, 1});
        d->outputs.push_back(Pin{Point(70, 25), PinDir::Output, "S", 0, false, 1});
        d->outputs.push_back(Pin{Point(70, 55), PinDir::Output, "COUT", 1, false, 1});
        lib->Add(std::move(d));
    }

    // ---- 布线 ----
    // 位分离器
    {
        auto d = std::make_unique<ComponentDef>();
        d->type = "splitter";
        d->name = "位分离器";
        d->category = "布线";
        d->kind = "splitter";
        d->description = "把一条总线拆成多条（本期为简化实现，见方案 R2）";
        d->size = wxSize(50, 60);
        d->shapes.push_back(MakeRect(Rect(0, 0, 50, 60)));
        d->shapes.push_back(MakeText(Point(25, 30), "SPL", 8, palette::kTextDim, true));
        d->inputs.push_back(Pin{Point(0, 30), PinDir::Input, "IN", 0, false});
        d->outputs.push_back(Pin{Point(50, 15), PinDir::Output, "Y0", 0, false});
        d->outputs.push_back(Pin{Point(50, 45), PinDir::Output, "Y1", 1, false});
        lib->Add(std::move(d));
    }

    // 命名隧道
    {
        auto d = std::make_unique<ComponentDef>();
        d->type = "tunnel";
        d->name = "命名隧道";
        d->category = "布线";
        d->kind = "tunnel";
        d->description = "同名隧道视为同一网络，用于显式命名";
        d->size = wxSize(60, 30);
        d->shapes.push_back(MakePoly({Point(0, 0), Point(52, 0), Point(60, 15), Point(52, 30), Point(0, 30)}, true));
        d->inouts.push_back(Pin{Point(0, 15), PinDir::InOut, "P", 0, false});
        lib->Add(std::move(d));
    }

    return lib;
}

}  // namespace sf
