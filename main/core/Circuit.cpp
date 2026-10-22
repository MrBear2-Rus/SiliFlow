#include "Circuit.h"

#include "Library.h"

#include <functional>
#include <map>
#include <set>

namespace sf
{

// -----------------------------------------------------------------------------
//  增删
// -----------------------------------------------------------------------------
ComponentInstance* CircuitSheet::AddComponent(const ComponentDef& def, const Point& pos)
{
    auto comp = std::make_unique<ComponentInstance>();
    comp->id = nextCompId++;
    comp->pos = pos;
    comp->Rebuild(def);

    // 默认标签：连接器类用 type+序号（用户会改成 a/b/clk），其余用 U+序号
    if (comp->label.IsEmpty())
    {
        const bool named = def.IsPort() || def.kind == "clock" || def.kind == "const" ||
                           def.kind == "tunnel" || def.kind == "led";
        comp->label = named ? def.type + wxString::Format("%d", comp->id)
                            : wxString::Format("U%d", comp->id);
    }

    ComponentInstance* raw = comp.get();
    components.push_back(std::move(comp));
    return raw;
}

Wire* CircuitSheet::AddWire(const WireEndpoint& a, const WireEndpoint& b)
{
    auto wire = std::make_unique<Wire>();
    wire->id = nextWireId++;
    wire->left = a;
    wire->right = b;
    wire->pts = Wire::Route(a.pos, b.pos);

    Wire* raw = wire.get();
    wires.push_back(std::move(wire));
    return raw;
}

bool CircuitSheet::RemoveComponent(int id)
{
    // 先级联删掉挂在这个元件上的导线
    std::vector<int> doomed;
    for (const auto& w : wires)
    {
        if (w->left.compId == id || w->right.compId == id) doomed.push_back(w->id);
    }
    for (int wid : doomed) RemoveWire(wid);

    auto it = std::find_if(components.begin(), components.end(),
                           [id](const std::unique_ptr<ComponentInstance>& c) { return c->id == id; });
    if (it == components.end()) return false;

    components.erase(it);
    return true;
}

bool CircuitSheet::RemoveWire(int id)
{
    auto it = std::find_if(wires.begin(), wires.end(),
                           [id](const std::unique_ptr<Wire>& w) { return w->id == id; });
    if (it == wires.end()) return false;

    wires.erase(it);
    return true;
}

// -----------------------------------------------------------------------------
//  查找
// -----------------------------------------------------------------------------
ComponentInstance* CircuitSheet::FindComponent(int id) const
{
    for (const auto& c : components)
        if (c->id == id) return c.get();
    return nullptr;
}

Wire* CircuitSheet::FindWire(int id) const
{
    for (const auto& w : wires)
        if (w->id == id) return w.get();
    return nullptr;
}

// -----------------------------------------------------------------------------
//  命中测试
// -----------------------------------------------------------------------------
ComponentInstance* CircuitSheet::HitComponent(const Point& p, int tol, int* whichPin) const
{
    if (whichPin != nullptr) *whichPin = -1;

    // 1) 先找引脚：引脚比本体更容易点中，优先级更高
    ComponentInstance* bestComp = nullptr;
    int bestPin = -1;
    long long bestD2 = -1;

    for (const auto& c : components)
    {
        for (int i = 0; i < static_cast<int>(c->pins.size()); ++i)
        {
            const Point pp = c->PinPos(i);
            const long long dx = pp.x - p.x;
            const long long dy = pp.y - p.y;
            const long long d2 = dx * dx + dy * dy;

            const int pinTol = std::max(tol, 6);
            if (d2 <= static_cast<long long>(pinTol) * pinTol && (bestD2 < 0 || d2 < bestD2))
            {
                bestD2 = d2;
                bestComp = c.get();
                bestPin = i;
            }
        }
    }

    if (bestComp != nullptr)
    {
        if (whichPin != nullptr) *whichPin = bestPin;
        return bestComp;
    }

    // 2) 再找本体：后加入的（更靠上层）优先
    for (auto it = components.rbegin(); it != components.rend(); ++it)
    {
        if ((*it)->HitTestBody(p, tol)) return it->get();
    }
    return nullptr;
}

Wire* CircuitSheet::HitWire(const Point& p, int tol) const
{
    for (auto it = wires.rbegin(); it != wires.rend(); ++it)
    {
        if ((*it)->HitTest(p, tol)) return it->get();
    }
    return nullptr;
}

int CircuitSheet::HitControlPoint(const Point& p, int tol, int* whichWireId) const
{
    if (whichWireId != nullptr) *whichWireId = -1;

    int bestCp = -1;
    int bestWire = -1;
    long long bestD2 = -1;

    for (const auto& w : wires)
    {
        for (int i = 0; i < static_cast<int>(w->pts.size()); ++i)
        {
            const long long dx = w->pts[i].pos.x - p.x;
            const long long dy = w->pts[i].pos.y - p.y;
            const long long d2 = dx * dx + dy * dy;
            if (d2 <= static_cast<long long>(tol) * tol && (bestD2 < 0 || d2 < bestD2))
            {
                bestD2 = d2;
                bestCp = i;
                bestWire = w->id;
            }
        }
    }

    if (whichWireId != nullptr) *whichWireId = bestWire;
    return bestCp;
}

// -----------------------------------------------------------------------------
//  区域查询
// -----------------------------------------------------------------------------
std::vector<ComponentInstance*> CircuitSheet::ComponentsIn(const Rect& r) const
{
    std::vector<ComponentInstance*> out;
    for (const auto& c : components)
        if (r.Intersects(c->Bounds())) out.push_back(c.get());
    return out;
}

std::vector<Wire*> CircuitSheet::WiresIn(const Rect& r) const
{
    std::vector<Wire*> out;
    for (const auto& w : wires)
        if (r.Intersects(w->Bounds())) out.push_back(w.get());
    return out;
}

// -----------------------------------------------------------------------------
//  端点吸附
// -----------------------------------------------------------------------------
bool CircuitSheet::SnapToPin(const Point& p, int radius, WireEndpoint& out) const
{
    const ComponentInstance* comp = nullptr;
    int pin = -1;
    long long bestD2 = static_cast<long long>(radius) * radius;

    for (const auto& c : components)
    {
        for (int i = 0; i < static_cast<int>(c->pins.size()); ++i)
        {
            const Point pp = c->PinPos(i);
            const long long dx = pp.x - p.x;
            const long long dy = pp.y - p.y;
            const long long d2 = dx * dx + dy * dy;
            if (d2 <= bestD2)
            {
                bestD2 = d2;
                comp = c.get();
                pin = i;
            }
        }
    }

    if (comp == nullptr) return false;

    out.compId = comp->id;
    out.pinIndex = pin;
    out.pos = comp->PinPos(pin);
    return true;
}

int CircuitSheet::WireCountAtPin(const PinRef& ref) const
{
    int n = 0;
    for (const auto& w : wires)
    {
        if (w->left.compId == ref.compId && w->left.pinIndex == ref.pinIndex) ++n;
        if (w->right.compId == ref.compId && w->right.pinIndex == ref.pinIndex) ++n;
    }
    return n;
}

// -----------------------------------------------------------------------------
//  一致性维护
// -----------------------------------------------------------------------------
void CircuitSheet::SyncWireEndpoints(int compId)
{
    ComponentInstance* comp = FindComponent(compId);
    if (comp == nullptr) return;

    for (auto& w : wires)
    {
        bool touched = false;
        if (w->left.compId == compId && w->left.pinIndex >= 0 &&
            w->left.pinIndex < static_cast<int>(comp->pins.size()))
        {
            w->left.pos = comp->PinPos(w->left.pinIndex);
            touched = true;
        }
        if (w->right.compId == compId && w->right.pinIndex >= 0 &&
            w->right.pinIndex < static_cast<int>(comp->pins.size()))
        {
            w->right.pos = comp->PinPos(w->right.pinIndex);
            touched = true;
        }
        if (touched) w->ReshapeFromEndpoints();
    }
}

// -----------------------------------------------------------------------------
//  连线关系推导
// -----------------------------------------------------------------------------
std::vector<Net> CircuitSheet::BuildNets() const
{
    std::vector<PinRef> nodes;
    std::map<std::pair<int, int>, int> nodeIndex;
    std::vector<int> parent;

    auto nodeOf = [&](const PinRef& r) -> int
    {
        const auto key = std::make_pair(r.compId, r.pinIndex);
        const auto it = nodeIndex.find(key);
        if (it != nodeIndex.end()) return it->second;

        const int id = static_cast<int>(nodes.size());
        nodes.push_back(r);
        nodeIndex[key] = id;
        parent.push_back(id);
        return id;
    };

    std::function<int(int)> findRoot = [&](int x) -> int
    {
        while (parent[x] != x)
        {
            parent[x] = parent[parent[x]];  // 路径压缩
            x = parent[x];
        }
        return x;
    };

    auto unite = [&](int a, int b)
    {
        a = findRoot(a);
        b = findRoot(b);
        if (a != b) parent[b] = a;
    };

    // 导线两端都接上引脚时，把两个引脚并起来
    for (const auto& w : wires)
    {
        if (!w->left.Attached() || !w->right.Attached()) continue;
        unite(nodeOf(PinRef{w->left.compId, w->left.pinIndex}),
              nodeOf(PinRef{w->right.compId, w->right.pinIndex}));
    }

    // 按根分组
    std::map<int, int> rootToNet;  // 根 -> nets 下标
    std::vector<Net> nets;

    for (int i = 0; i < static_cast<int>(nodes.size()); ++i)
    {
        const int root = findRoot(i);
        auto it = rootToNet.find(root);
        if (it == rootToNet.end())
        {
            const int idx = static_cast<int>(nets.size());
            rootToNet[root] = idx;
            nets.push_back(Net{});
            it = rootToNet.find(root);
        }
        nets[it->second].pins.push_back(nodes[i]);
    }

    // 把导线归入对应的网络
    for (const auto& w : wires)
    {
        int target = -1;
        if (w->left.Attached())
        {
            const auto it = nodeIndex.find(std::make_pair(w->left.compId, w->left.pinIndex));
            if (it != nodeIndex.end()) target = findRoot(it->second);
        }
        if (target < 0 && w->right.Attached())
        {
            const auto it = nodeIndex.find(std::make_pair(w->right.compId, w->right.pinIndex));
            if (it != nodeIndex.end()) target = findRoot(it->second);
        }
        if (target < 0) continue;

        const auto it = rootToNet.find(target);
        if (it != rootToNet.end()) nets[it->second].wireIds.push_back(w->id);
    }

    // 命名：tunnel 标签 > port 名 > const 名 > n_<序号>
    for (size_t i = 0; i < nets.size(); ++i)
    {
        wxString tunnelName, portName, constName, anyLabel;

        for (const PinRef& ref : nets[i].pins)
        {
            const ComponentInstance* c = FindComponent(ref.compId);
            if (c == nullptr || c->def == nullptr) continue;

            if (c->defType == "tunnel" && tunnelName.IsEmpty()) tunnelName = c->label;
            if (c->def->IsPort() && portName.IsEmpty() && !c->label.IsEmpty()) portName = c->label;
            if (c->def->kind == "const" && constName.IsEmpty() && !c->label.IsEmpty()) constName = c->label;
            if (anyLabel.IsEmpty() && !c->label.IsEmpty()) anyLabel = c->label;
        }

        if (!tunnelName.IsEmpty())
            nets[i].name = tunnelName;
        else if (!portName.IsEmpty())
            nets[i].name = portName;
        else if (!constName.IsEmpty())
            nets[i].name = constName;
        else if (!anyLabel.IsEmpty())
            nets[i].name = anyLabel + "_net";
        else
            nets[i].name = wxString::Format("n_%zu", i + 1);
    }

    return nets;
}

std::vector<int> CircuitSheet::WireIdsOfNet(const wxString& netName) const
{
    std::vector<int> out;
    for (const Net& n : BuildNets())
    {
        if (n.name.IsSameAs(netName, false))
            out.insert(out.end(), n.wireIds.begin(), n.wireIds.end());
    }
    return out;
}

// -----------------------------------------------------------------------------
//  选中集
// -----------------------------------------------------------------------------
std::vector<ComponentInstance*> CircuitSheet::SelectedComponents() const
{
    std::vector<ComponentInstance*> out;
    for (const auto& c : components)
        if (c->selected) out.push_back(c.get());
    return out;
}

std::vector<Wire*> CircuitSheet::SelectedWires() const
{
    std::vector<Wire*> out;
    for (const auto& w : wires)
        if (w->selected) out.push_back(w.get());
    return out;
}

void CircuitSheet::ClearSelection()
{
    for (auto& c : components) c->selected = false;
    for (auto& w : wires) w->selected = false;
}

size_t CircuitSheet::SelectionCount() const
{
    size_t n = 0;
    for (const auto& c : components)
        if (c->selected) ++n;
    for (const auto& w : wires)
        if (w->selected) ++n;
    return n;
}

int CircuitSheet::SelectInRect(const Rect& r, bool additive)
{
    if (!additive) ClearSelection();

    int count = 0;
    for (auto& c : components)
    {
        if (r.Intersects(c->Bounds()))
        {
            if (!c->selected)
            {
                c->selected = true;
                ++count;
            }
        }
    }
    for (auto& w : wires)
    {
        if (r.Intersects(w->Bounds()))
        {
            if (!w->selected)
            {
                w->selected = true;
                ++count;
            }
        }
    }
    return count;
}

// -----------------------------------------------------------------------------
//  其他
// -----------------------------------------------------------------------------
Rect CircuitSheet::Bounds() const
{
    bool any = false;
    Rect acc(0, 0, 0, 0);

    auto merge = [&any, &acc](const Rect& r)
    {
        if (!any)
        {
            acc = r;
            any = true;
        }
        else
            acc = acc.Union(r);
    };

    for (const auto& c : components) merge(c->Bounds());
    for (const auto& w : wires) merge(w->Bounds());
    for (const Shape& s : annotations) merge(ShapeBounds(s));

    return any ? acc : Rect(0, 0, 0, 0);
}

void CircuitSheet::Clear()
{
    components.clear();
    wires.clear();
    annotations.clear();
    nextCompId = 1;
    nextWireId = 1;
}

// -----------------------------------------------------------------------------
//  序列化
// -----------------------------------------------------------------------------
Json CircuitSheet::ToJson() const
{
    Json j;
    j["name"] = ToUtf8(name);
    j["nextCompId"] = nextCompId;
    j["nextWireId"] = nextWireId;

    Json comps = Json::array();
    for (const auto& c : components) comps.push_back(c->ToJson());
    j["components"] = comps;

    Json ws = Json::array();
    for (const auto& w : wires) ws.push_back(w->ToJson());
    j["wires"] = ws;

    Json ann = Json::array();
    for (const Shape& s : annotations) ann.push_back(ShapeToJson(s));
    j["annotations"] = ann;

    return j;
}

void CircuitSheet::FromJson(const Json& j, const Library* library)
{
    Clear();

    name = GetStr(j, "name", "main");
    nextCompId = GetInt(j, "nextCompId", 1);
    nextWireId = GetInt(j, "nextWireId", 1);

    if (Has(j, "components") && j["components"].is_array())
    {
        for (const Json& e : j["components"])
        {
            auto comp = std::make_unique<ComponentInstance>();
            comp->FromJson(e, library);
            nextCompId = std::max(nextCompId, comp->id + 1);
            components.push_back(std::move(comp));
        }
    }

    if (Has(j, "wires") && j["wires"].is_array())
    {
        for (const Json& e : j["wires"])
        {
            auto wire = std::make_unique<Wire>();
            wire->FromJson(e);
            nextWireId = std::max(nextWireId, wire->id + 1);
            wires.push_back(std::move(wire));
        }
    }

    if (Has(j, "annotations") && j["annotations"].is_array())
        for (const Json& e : j["annotations"]) annotations.push_back(ShapeFromJson(e));
}

}  // namespace sf
