#include "GateEval.h"

namespace sf
{
namespace sim
{

// -----------------------------------------------------------------------------
//  类别判定
// -----------------------------------------------------------------------------
EvalKind KindOf(const ComponentDef& def)
{
    // 先看 kind —— 它是「结构性」标签，比 gate 更具体
    if (def.kind == "const")    return EvalKind::Constant;
    if (def.kind == "clock")    return EvalKind::Clock;
    if (def.kind == "dff")      return EvalKind::Dff;
    if (def.kind == "splitter") return EvalKind::Splitter;
    if (def.kind == "tunnel")   return EvalKind::Tunnel;
    if (def.kind == "led")      return EvalKind::None;  // 只显示，不驱动

    if (def.kind == "port")
    {
        // 有输出引脚的是「输入端口」（向外驱动），否则是「输出端口」（只观察）
        return def.outputs.empty() ? EvalKind::None : EvalKind::InputPort;
    }

    // 再看 gate
    if (def.gate == "mux2") return EvalKind::Mux;
    if (def.gate == "add")  return EvalKind::FullAdder;
    if (!def.gate.IsEmpty()) return EvalKind::Gate;

    return EvalKind::None;
}

GateOp GateOpOf(const wxString& gate)
{
    wxString g = gate;

    // 去掉 3 入变体的数字后缀：and3 → and、nand3 → nand
    while (!g.IsEmpty())
    {
        const wxUniChar c = g.Last();
        if (c < '0' || c > '9') break;
        g.RemoveLast();
    }

    if (g == "and")  return GateOp::And;
    if (g == "or")   return GateOp::Or;
    if (g == "not")  return GateOp::Not;
    if (g == "nand") return GateOp::Nand;
    if (g == "nor")  return GateOp::Nor;
    if (g == "xor")  return GateOp::Xor;
    if (g == "xnor") return GateOp::Xnor;
    if (g == "buf")  return GateOp::Buf;
    return GateOp::None;
}

// -----------------------------------------------------------------------------
//  静态信息
// -----------------------------------------------------------------------------
CompEval MakeCompEval(const ComponentInstance& inst)
{
    CompEval e;
    const ComponentDef* def = inst.def;
    if (def == nullptr) return e;

    e.kind = KindOf(*def);
    e.op = (e.kind == EvalKind::Gate) ? GateOpOf(def->gate) : GateOp::None;

    const size_t nPins = inst.pins.size();
    e.roles.assign(nPins, PinRole::Unused);

    // pins 的顺序约定是 inputs…outputs…inouts（见 Component.h）
    const size_t nIn = def->inputs.size();
    const size_t nOut = def->outputs.size();

    for (size_t i = 0; i < nIn && i < nPins; ++i)
    {
        e.roles[i] = PinRole::Load;
        e.loadPins.push_back(static_cast<int>(i));
    }

    for (size_t i = 0; i < nOut; ++i)
    {
        const size_t pi = nIn + i;
        if (pi >= nPins) break;
        e.roles[pi] = PinRole::Driver;
        e.driverPins.push_back(static_cast<int>(pi));
    }

    e.isSource = (e.kind == EvalKind::Constant || e.kind == EvalKind::InputPort ||
                  e.kind == EvalKind::Clock);

    // 「没有仿真行为」只针对真正没定义的元件：led 是纯显示、output 端口是纯观察，
    // 这两个不驱动是设计如此，不该报警。
    e.unknownBehaviour = (e.kind == EvalKind::None && !e.loadPins.empty() &&
                          def->kind != "led" && def->kind != "port");

    return e;
}

bool IsToggleable(const CompEval& e)
{
    return e.kind == EvalKind::InputPort;
}

// -----------------------------------------------------------------------------
//  求值
// -----------------------------------------------------------------------------
void EvalComponent(const ComponentInstance& inst, const CompEval& e,
                   const std::vector<LogicVector>& inPins, std::vector<LogicVector>& outPins)
{
    (void)inst;

    // 读第 loadIdx 个输入引脚。悬空（Z）逐位按不定处理 —— 见 SimTypes 的说明。
    const auto in = [&](int loadIdx) -> LogicVector
    {
        if (loadIdx < 0 || loadIdx >= static_cast<int>(e.loadPins.size())) return LogicVector(1);
        const int pi = e.loadPins[loadIdx];
        if (pi < 0 || pi >= static_cast<int>(inPins.size())) return LogicVector(1);

        const LogicVector& src = inPins[pi];
        LogicVector v(src.Width());
        for (int b = 0; b < src.Width(); ++b)
        {
            const LogicSignal s = src.Bit(b);
            v.SetBit(b, (s == LogicSignal::Z) ? LogicSignal::X : s);
        }
        return v;
    };

    // 写第 driverIdx 个输出引脚，并按该引脚自身位宽收口
    const auto set = [&](int driverIdx, const LogicVector& v)
    {
        if (driverIdx < 0 || driverIdx >= static_cast<int>(e.driverPins.size())) return;
        const int pi = e.driverPins[driverIdx];
        if (pi < 0 || pi >= static_cast<int>(outPins.size())) return;

        const int w = outPins[pi].Width();
        outPins[pi] = (v.Width() == w) ? v : v.Resize(w);
    };

    const int nIn = static_cast<int>(e.loadPins.size());

    switch (e.kind)
    {
    case EvalKind::Gate:
    {
        if (nIn <= 0)
        {
            set(0, LogicVector(1, LogicSignal::X));
            break;
        }

        LogicVector acc = in(0);
        switch (e.op)
        {
        case GateOp::And:
        case GateOp::Nand:
            for (int i = 1; i < nIn; ++i) acc = acc.And(in(i));
            if (e.op == GateOp::Nand) acc = acc.Not();
            break;
        case GateOp::Or:
        case GateOp::Nor:
            for (int i = 1; i < nIn; ++i) acc = acc.Or(in(i));
            if (e.op == GateOp::Nor) acc = acc.Not();
            break;
        case GateOp::Xor:
        case GateOp::Xnor:
            for (int i = 1; i < nIn; ++i) acc = acc.Xor(in(i));
            if (e.op == GateOp::Xnor) acc = acc.Not();
            break;
        case GateOp::Not: acc = acc.Not(); break;
        case GateOp::Buf: break;
        default:          acc = LogicVector(1, LogicSignal::X); break;
        }
        set(0, acc);
        break;
    }

    case EvalKind::Mux:
    {
        // 引脚顺序：A(0) B(1) S(2)
        const LogicVector av = in(0);
        const LogicVector bv = in(1);
        const LogicSignal sel = in(2).Bit(0);

        const int w = av.Width() > bv.Width() ? av.Width() : bv.Width();
        LogicVector r(w);
        for (int b = 0; b < w; ++b)
        {
            const LogicSignal a = av.Bit(b);
            const LogicSignal bb = bv.Bit(b);
            LogicSignal v = LogicSignal::X;
            if (sel == LogicSignal::One)
                v = bb;
            else if (sel == LogicSignal::Zero)
                v = a;
            else
                v = (a == bb) ? a : LogicSignal::X;
            r.SetBit(b, v);
        }
        set(0, r);
        break;
    }

    case EvalKind::FullAdder:
    {
        // 全加器固定 1 位（多位加法器由 U2 的 addsub 承担）
        const AdderResult r = FullAdd(in(0).Bit(0), in(1).Bit(0), in(2).Bit(0));
        set(0, LogicVector::FromSignal(r.sum));
        set(1, LogicVector::FromSignal(r.carry));
        break;
    }

    case EvalKind::Splitter:
    {
        const LogicVector v = in(0);
        const int w = v.Width();

        if (w <= 1)
        {
            // 1 位输入是退化情形：两路都给同一位，免得老图纸突然变成悬空
            set(0, v);
            set(1, v);
            break;
        }

        // 二路等分：低位走 Y0，高位走 Y1（余数归低位那侧，和 Rebuild 的算法必须一致）
        const int lo = w / 2;
        set(0, v.Slice(0, lo - 1));
        set(1, v.Slice(lo, w - 1));
        break;
    }

    case EvalKind::Dff:
        // 时序元件还没有锁存状态，Q / QN 全 X
        if (!e.driverPins.empty())
            set(0, LogicVector(outPins[e.driverPins[0]].Width(), LogicSignal::X));
        if (e.driverPins.size() > 1)
            set(1, LogicVector(outPins[e.driverPins[1]].Width(), LogicSignal::X));
        break;

    default:
        // None / Constant / Clock / InputPort / Tunnel：驱动值由引擎喂，这里不动
        break;
    }
}

}  // namespace sim
}  // namespace sf
