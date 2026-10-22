#include "Component.h"

#include "Library.h"

#include <algorithm>
#include <cmath>

namespace sf
{

// -----------------------------------------------------------------------------
//  引脚方向
// -----------------------------------------------------------------------------
wxString PinDirName(PinDir dir)
{
    switch (dir)
    {
    case PinDir::Input:  return "input";
    case PinDir::Output: return "output";
    case PinDir::InOut:  return "inout";
    }
    return "input";
}

PinDir PinDirFromName(const wxString& s)
{
    wxString v = s;
    v.MakeLower();
    if (v == "output") return PinDir::Output;
    if (v == "inout" || v == "bidir") return PinDir::InOut;
    return PinDir::Input;
}

namespace
{

/// 读一个引脚数组，兼容 SigFlow 的 canvas_elements.json 写法。
void ReadPins(const Json& j, const char* key, PinDir dir, std::vector<Pin>& out)
{
    if (!Has(j, key) || !j[key].is_array()) return;

    int idx = 0;
    for (const Json& e : j[key])
    {
        Pin p;
        p.pos = GetPoint(e, "pos", Point(GetInt(e, "x", 0), GetInt(e, "y", 0)));
        p.dir = dir;
        p.inverted = GetBool(e, "inverted", false);
        p.width = GetInt(e, "width", 0);  // 0 = 跟随元件位宽参数

        // 标识符：优先 "id"/"name"/"identifier"，否则 A/B/C…（输出脚用 Y/Z…）
        p.identifier = GetStr(e, "identifier", GetStr(e, "id", GetStr(e, "name")));
        if (p.identifier.IsEmpty())
        {
            const char base = (dir == PinDir::Output) ? 'Y' : 'A';
            p.identifier = wxString(wxUniChar(base + (idx % 24)));
        }
        p.index = idx++;
        out.push_back(p);
    }
}

}  // namespace

// -----------------------------------------------------------------------------
//  ComponentDef
// -----------------------------------------------------------------------------
std::vector<const Pin*> ComponentDef::AllPins() const
{
    std::vector<const Pin*> all;
    all.reserve(inputs.size() + outputs.size() + inouts.size());
    for (const Pin& p : inputs) all.push_back(&p);
    for (const Pin& p : outputs) all.push_back(&p);
    for (const Pin& p : inouts) all.push_back(&p);
    return all;
}

Rect ComponentDef::LocalBounds() const
{
    bool any = false;
    Rect acc(0, 0, 0, 0);

    auto merge = [&any, &acc](const Rect& r)
    {
        if (r.width <= 0 && r.height <= 0) return;
        if (!any)
        {
            acc = r;
            any = true;
        }
        else
            acc = acc.Union(r);
    };

    for (const Shape& s : shapes) merge(ShapeBounds(s));
    for (const Pin& p : inputs) merge(Rect(p.pos.x - 1, p.pos.y - 1, 2, 2));
    for (const Pin& p : outputs) merge(Rect(p.pos.x - 1, p.pos.y - 1, 2, 2));
    for (const Pin& p : inouts) merge(Rect(p.pos.x - 1, p.pos.y - 1, 2, 2));

    return any ? acc : Rect(0, 0, 0, 0);
}

const Pin* ComponentDef::FindPin(const wxString& id) const
{
    for (const Pin& p : inputs)
        if (p.identifier.IsSameAs(id, false)) return &p;
    for (const Pin& p : outputs)
        if (p.identifier.IsSameAs(id, false)) return &p;
    for (const Pin& p : inouts)
        if (p.identifier.IsSameAs(id, false)) return &p;
    return nullptr;
}

Json ComponentDef::ToJson() const
{
    Json j;
    j["type"] = ToUtf8(type);
    j["name"] = ToUtf8(name);
    j["category"] = ToUtf8(category);
    if (!description.IsEmpty()) j["description"] = ToUtf8(description);
    j["bounds"] = Json::array({size.x, size.y});
    if (!gate.IsEmpty()) j["gate"] = ToUtf8(gate);
    if (!kind.IsEmpty()) j["kind"] = ToUtf8(kind);

    auto dumpPins = [](const std::vector<Pin>& pins)
    {
        Json arr = Json::array();
        for (const Pin& p : pins)
        {
            Json e{{"x", p.pos.x}, {"y", p.pos.y}, {"id", ToUtf8(p.identifier)}};
            if (p.inverted) e["inverted"] = true;
            if (p.width != 0) e["width"] = p.width;  // 0 = 跟随元件位宽，省略
            arr.push_back(e);
        }
        return arr;
    };

    j["inputPins"] = dumpPins(inputs);
    j["outputPins"] = dumpPins(outputs);
    if (!inouts.empty()) j["inoutPins"] = dumpPins(inouts);

    Json shapeArr = Json::array();
    for (const Shape& s : shapes) shapeArr.push_back(ShapeToJson(s));
    j["shapes"] = shapeArr;

    return j;
}

std::unique_ptr<ComponentDef> ComponentDef::FromJson(const Json& j)
{
    auto def = std::make_unique<ComponentDef>();

    def->type = GetStr(j, "type");
    if (def->type.IsEmpty()) return nullptr;  // type 是唯一标识，不能缺

    def->name = GetStr(j, "name", def->type);
    def->category = GetStr(j, "category", "未分类");
    def->description = GetStr(j, "description");
    def->gate = GetStr(j, "gate");
    def->kind = GetStr(j, "kind");

    ReadPins(j, "inputPins", PinDir::Input, def->inputs);
    ReadPins(j, "outputPins", PinDir::Output, def->outputs);
    ReadPins(j, "inoutPins", PinDir::InOut, def->inouts);

    if (Has(j, "shapes") && j["shapes"].is_array())
        for (const Json& e : j["shapes"]) def->shapes.push_back(ShapeFromJson(e));

    // bounds 支持 [w,h] 与 {"w":..,"h":..} 两种写法
    int bw = 0, bh = 0;
    if (Has(j, "bounds"))
    {
        const Json& b = j["bounds"];
        if (b.is_array() && b.size() >= 2)
        {
            if (b[0].is_number()) bw = b[0].get<int>();
            if (b[1].is_number()) bh = b[1].get<int>();
        }
        else if (b.is_object())
        {
            bw = GetInt(b, "w", GetInt(b, "width", 0));
            bh = GetInt(b, "h", GetInt(b, "height", 0));
        }
    }

    // 归一化：把图元+引脚整体平移到左上角为 (0,0)，后续变换才有一致的中心
    const Rect lb = def->LocalBounds();
    if (lb.x != 0 || lb.y != 0)
    {
        for (Shape& s : def->shapes) TranslateShape(s, -lb.x, -lb.y);
        auto shift = [&lb](std::vector<Pin>& pins)
        {
            for (Pin& p : pins) p.pos = Point(p.pos.x - lb.x, p.pos.y - lb.y);
        };
        shift(def->inputs);
        shift(def->outputs);
        shift(def->inouts);
    }

    if (bw <= 0 || bh <= 0)
    {
        bw = std::max(10, lb.width);
        bh = std::max(10, lb.height);
    }
    def->size = wxSize(bw, bh);

    return def;
}

// -----------------------------------------------------------------------------
//  ComponentInstance
// -----------------------------------------------------------------------------
namespace
{

/// 旋转奇数次时，元件在画布上的宽高互换。
wxSize RotatedSize(const wxSize& s, int rotation)
{
    const int q = ((rotation % 4) + 4) % 4;
    return (q % 2) ? wxSize(s.y, s.x) : s;
}

/// 变换后需要补的平移量，让内容回到「左上角为原点」。
Point RotateOffset(const wxSize& s, int rotation)
{
    const int q = ((rotation % 4) + 4) % 4;
    return (q % 2) ? Point(s.y / 2, s.x / 2) : Point(s.x / 2, s.y / 2);
}

}  // namespace

Point ComponentInstance::TransformLocal(const Point& lp) const
{
    const wxSize ds = def ? def->size : wxSize(1, 1);
    const int w = std::max(1, ds.x);
    const int h = std::max(1, ds.y);

    int x = lp.x - w / 2;
    int y = lp.y - h / 2;
    if (mirrored) x = -x;

    Point p = RotatePoint90(Point(x, y), rotation);
    const Point off = RotateOffset(ds, rotation);
    return Point(p.x + off.x, p.y + off.y);
}

Point ComponentInstance::CanvasToLocal(const Point& p) const
{
    const wxSize ds = def ? def->size : wxSize(1, 1);
    const int w = std::max(1, ds.x);
    const int h = std::max(1, ds.y);
    const Point off = RotateOffset(ds, rotation);

    Point r(p.x - pos.x - off.x, p.y - pos.y - off.y);
    r = RotatePoint90(r, -rotation);
    if (mirrored) r.x = -r.x;
    return Point(r.x + w / 2, r.y + h / 2);
}

void ComponentInstance::Rebuild(const ComponentDef& d)
{
    def = &d;
    defType = d.type;

    pins.clear();
    pins.reserve(d.inputs.size() + d.outputs.size() + d.inouts.size());

    // splitter 是唯一「输出位宽不等于元件位宽」的元件：二路等分，余数给低位那半
    const bool isSplitter = (d.kind == "splitter");
    int splitterOutIdx = 0;

    auto append = [&](const Pin& src, PinDir dir)
    {
        Pin p = src;
        p.dir = dir;
        p.pos = TransformLocal(src.pos);
        p.index = static_cast<int>(pins.size());

        // 位宽：0 表示跟随元件参数，负数/越界一律夹到合法范围
        int resolved = (src.width > 0) ? src.width : width;

        if (isSplitter && dir == PinDir::Output)
        {
            const int lo = width / 2;
            resolved = (splitterOutIdx == 0) ? std::max(1, lo) : std::max(1, width - lo);
            ++splitterOutIdx;
        }

        p.width = (resolved < 1) ? 1 : ((resolved > kMaxBusWidth) ? kMaxBusWidth : resolved);

        pins.push_back(p);
    };

    for (const Pin& p : d.inputs) append(p, PinDir::Input);
    inputCount = static_cast<int>(d.inputs.size());
    for (const Pin& p : d.outputs) append(p, PinDir::Output);
    outputCount = static_cast<int>(d.outputs.size());
    for (const Pin& p : d.inouts) append(p, PinDir::InOut);
    inoutCount = static_cast<int>(d.inouts.size());

    const wxSize ds = wxSize(std::max(1, d.size.x), std::max(1, d.size.y));
    const Point off = RotateOffset(ds, rotation);

    xShapes = d.shapes;
    for (Shape& s : xShapes)
    {
        TranslateShape(s, -ds.x / 2, -ds.y / 2);
        if (mirrored) MirrorShapeX(s);
        RotateShape90(s, rotation);
        TranslateShape(s, off.x, off.y);
    }

    boxSize = RotatedSize(ds, rotation);
}

Rect ComponentInstance::Bounds() const
{
    return Rect(pos.x, pos.y, boxSize.x, boxSize.y);
}

Point ComponentInstance::Center() const
{
    return Point(pos.x + boxSize.x / 2, pos.y + boxSize.y / 2);
}

Point ComponentInstance::PinPos(int idx) const
{
    if (idx < 0 || idx >= static_cast<int>(pins.size())) return pos;
    return Point(pos.x + pins[idx].pos.x, pos.y + pins[idx].pos.y);
}

bool ComponentInstance::HitTestBody(const Point& p, int tol) const
{
    return Bounds().Inflate(tol, tol).Contains(p);
}

int ComponentInstance::HitTestPin(const Point& p, int tol) const
{
    int best = -1;
    long long bestD2 = static_cast<long long>(tol) * tol;

    for (int i = 0; i < static_cast<int>(pins.size()); ++i)
    {
        const Point pp = PinPos(i);
        const long long dx = pp.x - p.x;
        const long long dy = pp.y - p.y;
        const long long d2 = dx * dx + dy * dy;
        if (d2 <= bestD2)
        {
            bestD2 = d2;
            best = i;
        }
    }
    return best;
}

void ComponentInstance::RotateCw()
{
    if (def == nullptr) return;

    const Point c = Center();
    rotation = ((rotation + 1) % 4 + 4) % 4;
    Rebuild(*def);
    pos = Point(c.x - boxSize.x / 2, c.y - boxSize.y / 2);  // 保持中心不动
}

void ComponentInstance::RotateCcw()
{
    if (def == nullptr) return;

    const Point c = Center();
    rotation = ((rotation - 1) % 4 + 4) % 4;
    Rebuild(*def);
    pos = Point(c.x - boxSize.x / 2, c.y - boxSize.y / 2);
}

void ComponentInstance::ToggleMirror()
{
    if (def == nullptr) return;

    mirrored = !mirrored;
    Rebuild(*def);  // 关于竖直中轴镜像，包围盒不变，pos 无需调整
}

Json ComponentInstance::ToJson() const
{
    return Json{
        {"id", id},
        {"type", ToUtf8(defType)},
        {"x", pos.x},
        {"y", pos.y},
        {"rotation", rotation},
        {"mirrored", mirrored},
        {"label", ToUtf8(label)},
        {"value", ToUtf8(value)},
        {"width", width},
    };
}

void ComponentInstance::FromJson(const Json& j, const Library* library)
{
    id = GetInt(j, "id", 0);
    defType = GetStr(j, "type");
    pos = Point(GetInt(j, "x", 0), GetInt(j, "y", 0));
    rotation = ((GetInt(j, "rotation", 0) % 4) + 4) % 4;
    mirrored = GetBool(j, "mirrored", false);
    label = GetStr(j, "label");
    value = GetStr(j, "value");

    // 缺省 1 位 —— v1 的工程文件没有这个字段，读进来就是 1 位，行为不变
    width = GetInt(j, "width", 1);
    if (width < 1) width = 1;
    if (width > kMaxBusWidth) width = kMaxBusWidth;

    if (library != nullptr)
    {
        if (const ComponentDef* d = library->Find(defType)) Rebuild(*d);
    }
}

}  // namespace sf
