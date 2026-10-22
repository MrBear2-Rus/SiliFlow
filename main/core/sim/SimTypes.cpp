#include "SimTypes.h"

namespace sf
{
namespace sim
{

// -----------------------------------------------------------------------------
//  四值运算
// -----------------------------------------------------------------------------
LogicSignal Buf(LogicSignal a)
{
    // 悬空输入没法知道电平，按不定处理
    return a == LogicSignal::Z ? LogicSignal::X : a;
}

LogicSignal Xor(LogicSignal a, LogicSignal b)
{
    const bool aKnown = (a == LogicSignal::Zero || a == LogicSignal::One);
    const bool bKnown = (b == LogicSignal::Zero || b == LogicSignal::One);
    if (!aKnown || !bKnown) return LogicSignal::X;

    return (a != b) ? LogicSignal::One : LogicSignal::Zero;
}

LogicSignal Nand(LogicSignal a, LogicSignal b)
{
    return LogicNot(LogicAnd(a, b));
}

LogicSignal Nor(LogicSignal a, LogicSignal b)
{
    return LogicNot(LogicOr(a, b));
}

LogicSignal Xnor(LogicSignal a, LogicSignal b)
{
    return LogicNot(Xor(a, b));
}

LogicSignal AndAll(const std::vector<LogicSignal>& v)
{
    LogicSignal acc = LogicSignal::One;
    for (LogicSignal s : v) acc = LogicAnd(acc, s);
    return acc;
}

LogicSignal OrAll(const std::vector<LogicSignal>& v)
{
    LogicSignal acc = LogicSignal::Zero;
    for (LogicSignal s : v) acc = LogicOr(acc, s);
    return acc;
}

LogicSignal XorAll(const std::vector<LogicSignal>& v)
{
    LogicSignal acc = LogicSignal::Zero;
    for (LogicSignal s : v) acc = Xor(acc, s);
    return acc;
}

LogicSignal Mux(LogicSignal sel, LogicSignal a, LogicSignal b)
{
    if (sel == LogicSignal::One) return Buf(b);
    if (sel == LogicSignal::Zero) return Buf(a);

    // 选择端不定：只有两路一致时才敢给出确定值
    const LogicSignal av = Buf(a);
    const LogicSignal bv = Buf(b);
    return (av == bv) ? av : LogicSignal::X;
}

AdderResult FullAdd(LogicSignal a, LogicSignal b, LogicSignal cin)
{
    AdderResult r;
    r.sum = XorAll({a, b, cin});

    // 三取二多数。用 And/Or 的支配律，含 X 时仍能得到确定结果
    // （比如 a=b=1 时无论 cin 是什么，carry 一定是 1）。
    const LogicSignal ab = LogicAnd(a, b);
    const LogicSignal bc = LogicAnd(b, cin);
    const LogicSignal ac = LogicAnd(a, cin);
    r.carry = OrAll({ab, bc, ac});
    return r;
}

LogicSignal ResolveDrivers(const std::vector<LogicSignal>& drivers)
{
    LogicSignal result = LogicSignal::Z;
    bool have = false;

    for (LogicSignal d : drivers)
    {
        if (d == LogicSignal::Z) continue;  // Z 不是驱动

        if (!have)
        {
            result = d;
            have = true;
            continue;
        }

        if (d != result) return LogicSignal::X;  // 冲突
    }

    return have ? result : LogicSignal::Z;
}

// -----------------------------------------------------------------------------
//  名字
// -----------------------------------------------------------------------------
wxString PinRoleName(PinRole r)
{
    switch (r)
    {
    case PinRole::Driver: return "驱动";
    case PinRole::Load:   return "读取";
    default:              return "不参与";
    }
}

wxString EvalKindName(EvalKind k)
{
    switch (k)
    {
    case EvalKind::Gate:       return "组合门";
    case EvalKind::Constant:   return "常量";
    case EvalKind::InputPort:  return "输入端口";
    case EvalKind::Clock:      return "时钟源";
    case EvalKind::Tunnel:     return "隧道";
    case EvalKind::Splitter:   return "位分离器";
    case EvalKind::Mux:        return "多路器";
    case EvalKind::FullAdder:  return "全加器";
    case EvalKind::Dff:        return "D 触发器";
    default:                   return "不参与";
    }
}

}  // namespace sim
}  // namespace sf
