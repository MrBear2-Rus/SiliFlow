#include "SimEngine.h"

#include <algorithm>
#include <deque>
#include <functional>
#include <map>
#include <set>
#include <utility>

namespace sf
{
namespace sim
{

namespace
{

/// 同一个网络在一次求值里最多被改写多少次；超过就认定它卷进了组合环。
constexpr int kMaxNetFlips = 64;

/// 内置库里只有 const1 是高电平，其余（const0 / ground）都是低。
bool IsHighConst(const wxString& type)
{
    return type == "const1";
}

/// 驱动源的默认填充电平（用户没有强制时用）
LogicSignal SourceFill(const ComponentInstance& c)
{
    const ComponentDef* def = c.def;
    if (def == nullptr) return LogicSignal::Z;

    if (def->kind == "const") return IsHighConst(def->type) ? LogicSignal::One : LogicSignal::Zero;
    return LogicSignal::Zero;  // input 端口 / 时钟默认低
}

}  // namespace

// -----------------------------------------------------------------------------
//  生命周期
// -----------------------------------------------------------------------------
void SimEngine::Attach(CircuitSheet* sheet)
{
    if (m_sheet == sheet) return;

    m_sheet = sheet;
    m_forced.clear();
    m_tick = 0;
    m_oscillating = false;
    m_conflicts = 0;
    m_hasResult = false;
    m_running = false;
    m_status.Clear();
    MarkDirty();
}

void SimEngine::MarkDirty()
{
    m_dirty = true;
    m_built = false;
    m_hasResult = false;
}

void SimEngine::Reset()
{
    m_forced.clear();
    m_tick = 0;
    m_oscillating = false;
    m_conflicts = 0;
    MarkDirty();
    EnsureBuilt();
    Evaluate();
}

void SimEngine::EnsureBuilt() const
{
    if (m_built && !m_dirty) return;
    Build();
}

// -----------------------------------------------------------------------------
//  交互
// -----------------------------------------------------------------------------
bool SimEngine::TogglePin(int compId)
{
    EnsureBuilt();
    if (!IsToggleable(compId)) return false;

    const CompState* cs = CompOf(compId);
    if (cs == nullptr || cs->eval.driverPins.empty()) return false;

    const int pi = cs->eval.driverPins[0];
    const int w = cs->outPins[pi].Width();

    // 当前值：先看强制值，没有就看它实际驱动的电平
    LogicVector cur(w, LogicSignal::Zero);
    const auto fit = m_forced.find(compId);
    if (fit != m_forced.end())
        cur = fit->second.Resize(w);
    else
        cur = PinValue(compId, pi).Resize(w);

    // 全 1 → 全 0，其余（含 X / Z）→ 全 1。多位就是这样整组翻，不逐位翻转。
    bool allOne = true;
    for (int b = 0; b < w; ++b)
        if (cur.Bit(b) != LogicSignal::One)
        {
            allOne = false;
            break;
        }

    m_forced[compId] = LogicVector(w, allOne ? LogicSignal::Zero : LogicSignal::One);
    Evaluate();
    return true;
}

bool SimEngine::IsToggleable(int compId) const
{
    EnsureBuilt();
    const CompState* cs = CompOf(compId);
    return cs != nullptr && sim::IsToggleable(cs->eval);
}

bool SimEngine::SetForced(int compId, const LogicVector& v)
{
    EnsureBuilt();

    const CompState* cs = CompOf(compId);
    if (cs == nullptr || !cs->eval.isSource) return false;

    m_forced[compId] = v;
    Evaluate();
    return true;
}

void SimEngine::ClearForced(int compId)
{
    m_forced.erase(compId);
    Evaluate();
}

bool SimEngine::ForcedValue(int compId, LogicVector* out) const
{
    const auto it = m_forced.find(compId);
    if (it == m_forced.end()) return false;
    if (out != nullptr) *out = it->second;
    return true;
}

// -----------------------------------------------------------------------------
//  建图
// -----------------------------------------------------------------------------
void SimEngine::Build() const
{
    m_nets.clear();
    m_netValue.clear();
    m_netWidth.clear();
    m_netWidthBad.clear();
    m_netOfWire.clear();
    m_comps.clear();
    m_loopCount = 0;
    m_widthBad = 0;

    m_built = true;
    m_dirty = false;
    m_hasResult = false;

    if (m_sheet == nullptr) return;

    m_nets = m_sheet->BuildNets();
    MergeTunnels();

    for (int i = 0; i < static_cast<int>(m_nets.size()); ++i)
        for (int wid : m_nets[i].wireIds) m_netOfWire[wid] = i;

    // 元件静态信息。outPins 顺便按引脚位宽初始化 —— 它同时充当「引脚位宽表」。
    for (const auto& c : m_sheet->components)
    {
        if (c == nullptr) continue;

        CompState cs;
        cs.eval = MakeCompEval(*c);
        cs.pinNet.assign(c->pins.size(), -1);

        cs.outPins.reserve(c->pins.size());
        for (const Pin& p : c->pins)
        {
            const int w = (p.width > 0) ? p.width : 1;
            cs.outPins.push_back(LogicVector(w));
        }

        m_comps[c->id] = std::move(cs);
    }

    // 引脚 → 网络
    for (int i = 0; i < static_cast<int>(m_nets.size()); ++i)
    {
        for (const PinRef& r : m_nets[i].pins)
        {
            const auto it = m_comps.find(r.compId);
            if (it == m_comps.end()) continue;
            if (r.pinIndex < 0 || r.pinIndex >= static_cast<int>(it->second.pinNet.size())) continue;
            it->second.pinNet[r.pinIndex] = i;
        }
    }

    ComputeNetWidths();
    DetectLoops();

    m_netValue.assign(m_nets.size(), LogicVector(1));
}

void SimEngine::ComputeNetWidths() const
{
    m_netWidth.assign(m_nets.size(), 1);
    m_netWidthBad.assign(m_nets.size(), 0);
    m_widthBad = 0;

    for (int i = 0; i < static_cast<int>(m_nets.size()); ++i)
    {
        int w = 0;
        bool bad = false;

        for (const PinRef& r : m_nets[i].pins)
        {
            const CompState* cs = CompOf(r.compId);
            if (cs == nullptr) continue;
            if (r.pinIndex < 0 || r.pinIndex >= static_cast<int>(cs->outPins.size())) continue;

            const int pw = cs->outPins[r.pinIndex].Width();
            if (w == 0)
                w = pw;
            else if (w != pw)
                bad = true;
        }

        if (w <= 0) w = 1;
        m_netWidth[i] = w;

        if (bad)
        {
            m_netWidthBad[i] = 1;
            ++m_widthBad;
        }
    }
}

void SimEngine::MergeTunnels() const
{
    if (m_sheet == nullptr || m_nets.empty()) return;

    std::map<std::pair<int, int>, int> pinNet;
    for (int i = 0; i < static_cast<int>(m_nets.size()); ++i)
        for (const PinRef& r : m_nets[i].pins) pinNet[std::make_pair(r.compId, r.pinIndex)] = i;

    std::vector<int> parent(m_nets.size());
    for (int i = 0; i < static_cast<int>(parent.size()); ++i) parent[i] = i;

    std::function<int(int)> findRoot = [&](int x) -> int
    {
        while (parent[x] != x)
        {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    };
    const auto unite = [&](int a, int b)
    {
        a = findRoot(a);
        b = findRoot(b);
        if (a != b) parent[b] = a;
    };

    // 同名 tunnel 的网络视为同一个网络（BuildNets 只用标签命名，并没有把它们并起来）
    std::map<wxString, int> byLabel;
    bool anyMerge = false;

    for (const auto& c : m_sheet->components)
    {
        if (c == nullptr || c->defType != "tunnel" || c->label.IsEmpty()) continue;
        if (c->pins.empty()) continue;

        const auto pit = pinNet.find(std::make_pair(c->id, 0));
        if (pit == pinNet.end()) continue;

        const auto lit = byLabel.find(c->label);
        if (lit == byLabel.end())
        {
            byLabel[c->label] = pit->second;
        }
        else
        {
            unite(pit->second, lit->second);
            anyMerge = true;
        }
    }

    if (!anyMerge) return;

    std::map<int, int> rootToNew;
    std::vector<Net> merged;

    for (int i = 0; i < static_cast<int>(m_nets.size()); ++i)
    {
        const int root = findRoot(i);
        const auto it = rootToNew.find(root);

        if (it == rootToNew.end())
        {
            const int idx = static_cast<int>(merged.size());
            rootToNew[root] = idx;
            merged.push_back(m_nets[i]);
        }
        else
        {
            Net& dst = merged[it->second];
            dst.pins.insert(dst.pins.end(), m_nets[i].pins.begin(), m_nets[i].pins.end());
            dst.wireIds.insert(dst.wireIds.end(), m_nets[i].wireIds.begin(), m_nets[i].wireIds.end());
        }
    }

    m_nets.swap(merged);
}

void SimEngine::DetectLoops() const
{
    // 「Z 进引脚按 X」会让反馈回路立刻稳定在 X，光靠翻转计数抓不到环。
    // 所以从结构上找：C1 → C2 表示 C1 的驱动引脚与 C2 的读取引脚在同一个网络上。
    // D 触发器是环的打断点，不参与 —— 否则任何状态机都会被误报。
    m_loopCount = 0;
    if (m_comps.empty() || m_nets.empty()) return;

    std::map<int, std::vector<int>> adj;

    for (const auto& kv : m_comps)
    {
        if (kv.second.eval.kind == EvalKind::Dff) continue;
        if (kv.second.eval.driverPins.empty()) continue;
        adj[kv.first];
    }
    if (adj.empty()) return;

    for (const Net& net : m_nets)
    {
        std::vector<int> drivers, loads;

        for (const PinRef& r : net.pins)
        {
            const CompState* cs = CompOf(r.compId);
            if (cs == nullptr) continue;
            if (cs->eval.kind == EvalKind::Dff) continue;
            if (r.pinIndex < 0 || r.pinIndex >= static_cast<int>(cs->eval.roles.size())) continue;

            const PinRole role = cs->eval.roles[r.pinIndex];
            if (role == PinRole::Driver)
                drivers.push_back(r.compId);
            else if (role == PinRole::Load)
                loads.push_back(r.compId);
        }

        for (int d : drivers)
            for (int l : loads) adj[d].push_back(l);
    }

    std::map<int, int> color;  // 0 未访问 / 1 在栈上 / 2 已完成
    std::vector<int> stack;
    std::vector<size_t> cursor;

    for (const auto& kv : adj)
    {
        const int start = kv.first;
        if (color[start] != 0) continue;

        stack.clear();
        cursor.clear();
        stack.push_back(start);
        cursor.push_back(0);
        color[start] = 1;

        while (!stack.empty())
        {
            const int cur = stack.back();
            size_t& i = cursor.back();
            const std::vector<int>& nbrs = adj[cur];

            if (i >= nbrs.size())
            {
                color[cur] = 2;
                stack.pop_back();
                cursor.pop_back();
                continue;
            }

            const int nxt = nbrs[i++];
            const auto cit = color.find(nxt);
            const int c = (cit == color.end()) ? 0 : cit->second;

            if (c == 1)
            {
                ++m_loopCount;
                continue;
            }
            if (c == 2) continue;

            color[nxt] = 1;
            stack.push_back(nxt);
            cursor.push_back(0);
        }
    }
}

// -----------------------------------------------------------------------------
//  求值
// -----------------------------------------------------------------------------
const CompState* SimEngine::CompOf(int compId) const
{
    const auto it = m_comps.find(compId);
    return (it == m_comps.end()) ? nullptr : &it->second;
}

LogicVector SimEngine::ResolveNet(int net) const
{
    if (net < 0 || net >= static_cast<int>(m_nets.size())) return LogicVector(1);

    const int w = m_netWidth[net];

    // 位宽对不上（比如 4 位输出接到 1 位输入）→ 整个网络判 X，别让用户看到假值
    if (m_netWidthBad[net]) return LogicVector(w, LogicSignal::X);

    std::vector<LogicVector> drivers;
    drivers.reserve(m_nets[net].pins.size());

    for (const PinRef& r : m_nets[net].pins)
    {
        const CompState* cs = CompOf(r.compId);
        if (cs == nullptr) continue;
        if (r.pinIndex < 0 || r.pinIndex >= static_cast<int>(cs->eval.roles.size())) continue;
        if (cs->eval.roles[r.pinIndex] != PinRole::Driver) continue;

        drivers.push_back(cs->outPins[r.pinIndex]);
    }

    return LogicVector::ResolveDrivers(drivers, w);
}

void SimEngine::Evaluate()
{
    EnsureBuilt();

    m_oscillating = false;
    m_conflicts = 0;

    if (m_sheet == nullptr)
    {
        m_status = "仿真：没有活动图纸";
        return;
    }

    if (m_comps.empty() && m_nets.empty())
    {
        m_hasResult = true;
        m_status = "仿真：图纸是空的";
        return;
    }

    // ---- 1. 驱动源先落地（常量 / 输入端口 / 时钟）----
    for (auto& kv : m_comps)
    {
        CompState& cs = kv.second;
        if (!cs.eval.isSource || cs.eval.driverPins.empty()) continue;

        const ComponentInstance* c = m_sheet->FindComponent(kv.first);
        if (c == nullptr || c->def == nullptr) continue;

        const int pi = cs.eval.driverPins[0];
        const int w = cs.outPins[pi].Width();

        const auto fit = m_forced.find(kv.first);
        if (fit != m_forced.end())
            cs.outPins[pi] = fit->second.Resize(w);
        else
            cs.outPins[pi] = LogicVector(w, SourceFill(*c));

        // 一个源只有第一个驱动引脚（内置库的端口 / 常量 / 时钟都是单脚）
    }

    // ---- 2. 网络初值 ----
    for (int i = 0; i < static_cast<int>(m_nets.size()); ++i) m_netValue[i] = ResolveNet(i);

    // ---- 3. worklist 传播到不动点 ----
    std::deque<int> queue;
    std::set<int> queued;
    const auto push = [&](int id)
    {
        if (queued.insert(id).second) queue.push_back(id);
    };

    for (const auto& kv : m_comps) push(kv.first);

    std::vector<int> netFlips(m_nets.size(), 0);
    bool oscillating = false;

    const long long maxEvals =
        200LL * static_cast<long long>(m_comps.size() + m_nets.size()) + 5000LL;
    long long evals = 0;

    while (!queue.empty())
    {
        if (++evals > maxEvals)
        {
            oscillating = true;
            break;
        }

        const int id = queue.front();
        queue.pop_front();
        queued.erase(id);

        const auto it = m_comps.find(id);
        if (it == m_comps.end()) continue;
        CompState& cs = it->second;

        const ComponentInstance* c = m_sheet->FindComponent(id);
        if (c == nullptr) continue;

        const size_t n = c->pins.size();
        if (n == 0 || cs.pinNet.size() != n) continue;

        std::vector<LogicVector> inPins(n, LogicVector(1));
        for (size_t i = 0; i < n; ++i)
        {
            if (cs.pinNet[i] < 0) continue;
            const LogicVector& v = m_netValue[cs.pinNet[i]];
            inPins[i] = (v.Width() == cs.outPins[i].Width()) ? v : v.Resize(cs.outPins[i].Width());
        }

        std::vector<LogicVector> outPins = cs.outPins;
        EvalComponent(*c, cs.eval, inPins, outPins);

        for (int pi : cs.eval.driverPins)
        {
            if (pi < 0 || pi >= static_cast<int>(cs.outPins.size())) continue;
            if (outPins[pi] == cs.outPins[pi]) continue;

            cs.outPins[pi] = outPins[pi];

            const int net = cs.pinNet[pi];
            if (net < 0) continue;

            const LogicVector nv = ResolveNet(net);
            if (nv == m_netValue[net]) continue;

            m_netValue[net] = nv;

            if (++netFlips[net] > kMaxNetFlips)
            {
                m_netValue[net] = LogicVector(m_netWidth[net], LogicSignal::X);
                oscillating = true;
                continue;
            }

            for (const PinRef& r : m_nets[net].pins)
            {
                const CompState* ccs = CompOf(r.compId);
                if (ccs == nullptr) continue;
                if (r.pinIndex < 0 || r.pinIndex >= static_cast<int>(ccs->eval.roles.size())) continue;
                if (ccs->eval.roles[r.pinIndex] == PinRole::Load) push(r.compId);
            }
        }
    }

    if (oscillating && evals > maxEvals)
    {
        for (int id : queue)
        {
            const CompState* cs = CompOf(id);
            if (cs == nullptr) continue;
            for (int net : cs->pinNet)
                if (net >= 0) m_netValue[net] = LogicVector(m_netWidth[net], LogicSignal::X);
        }
    }

    m_oscillating = oscillating;
    m_hasResult = true;

    CountConflicts();

    // ---- 4. 状态文案 ----
    wxString s = wxString::Format("仿真：%zu 元件 / %zu 网络 · tick %d", ComponentCount(), NetCount(), m_tick);
    if (m_loopCount > 0) s += wxString::Format(" · %d 处组合环（结果标 X）", m_loopCount);
    if (m_oscillating) s += " · 未收敛（相关网络已标 X）";
    if (m_conflicts > 0) s += wxString::Format(" · %d 个网络驱动冲突（已标 X）", m_conflicts);
    if (m_widthBad > 0) s += wxString::Format(" · %d 个网络位宽不一致（已标 X）", m_widthBad);

    bool hasDff = false;
    for (const auto& kv : m_comps)
        if (kv.second.eval.kind == EvalKind::Dff)
        {
            hasDff = true;
            break;
        }
    if (hasDff) s += " · 含 D 触发器：时序尚未接入，Q 输出 X";

    m_status = s;
}

void SimEngine::Tick()
{
    EnsureBuilt();
    ++m_tick;
    Evaluate();
}

void SimEngine::CountConflicts()
{
    m_conflicts = 0;

    for (int i = 0; i < static_cast<int>(m_nets.size()); ++i)
    {
        if (m_netWidthBad[i]) continue;  // 位宽不一致已经单独统计了

        bool anyX = false;
        for (int b = 0; b < m_netWidth[i]; ++b)
            if (m_netValue[i].Bit(b) == LogicSignal::X)
            {
                anyX = true;
                break;
            }
        if (!anyX) continue;

        LogicVector first(1);
        bool have = false;

        for (const PinRef& r : m_nets[i].pins)
        {
            const CompState* cs = CompOf(r.compId);
            if (cs == nullptr) continue;
            if (r.pinIndex < 0 || r.pinIndex >= static_cast<int>(cs->eval.roles.size())) continue;
            if (cs->eval.roles[r.pinIndex] != PinRole::Driver) continue;

            const LogicVector& v = cs->outPins[r.pinIndex];
            bool allZ = true;
            for (int b = 0; b < v.Width(); ++b)
                if (v.Bit(b) != LogicSignal::Z)
                {
                    allZ = false;
                    break;
                }
            if (allZ) continue;

            if (!have)
            {
                first = v;
                have = true;
                continue;
            }
            if (v != first)
            {
                ++m_conflicts;
                break;
            }
        }
    }
}

// -----------------------------------------------------------------------------
//  查询
// -----------------------------------------------------------------------------
int SimEngine::NetOfPin(int compId, int pinIndex) const
{
    const CompState* cs = CompOf(compId);
    if (cs == nullptr) return -1;
    if (pinIndex < 0 || pinIndex >= static_cast<int>(cs->pinNet.size())) return -1;
    return cs->pinNet[pinIndex];
}

LogicVector SimEngine::PinValue(int compId, int pinIndex) const
{
    EnsureBuilt();

    const int net = NetOfPin(compId, pinIndex);
    if (net < 0 || net >= static_cast<int>(m_netValue.size())) return LogicVector(1);
    return m_netValue[net];
}

LogicSignal SimEngine::PinSignal(int compId, int pinIndex) const
{
    return PinValue(compId, pinIndex).Bit(0);
}

int SimEngine::PinWidth(int compId, int pinIndex) const
{
    EnsureBuilt();
    const CompState* cs = CompOf(compId);
    if (cs == nullptr || pinIndex < 0 || pinIndex >= static_cast<int>(cs->outPins.size())) return 1;
    return cs->outPins[pinIndex].Width();
}

int SimEngine::NetWidth(int net) const
{
    EnsureBuilt();
    if (net < 0 || net >= static_cast<int>(m_netWidth.size())) return 1;
    return m_netWidth[net];
}

int SimEngine::WireBits(int wireId) const
{
    EnsureBuilt();
    const auto it = m_netOfWire.find(wireId);
    if (it == m_netOfWire.end()) return 1;
    return NetWidth(it->second);
}

bool SimEngine::TryWireValue(int wireId, LogicVector& out) const
{
    EnsureBuilt();
    if (!m_hasResult) return false;

    const auto it = m_netOfWire.find(wireId);
    if (it == m_netOfWire.end()) return false;
    if (it->second < 0 || it->second >= static_cast<int>(m_netValue.size())) return false;

    out = m_netValue[it->second];
    return true;
}

bool SimEngine::TryWireSignal(int wireId, LogicSignal& out) const
{
    LogicVector v;
    if (!TryWireValue(wireId, v)) return false;
    out = v.Bit(0);
    return true;
}

}  // namespace sim
}  // namespace sf
