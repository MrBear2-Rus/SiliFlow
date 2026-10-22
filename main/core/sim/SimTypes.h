#pragma once

// =============================================================================
//  SiliFlow · core/sim/SimTypes.h
//  仿真的基础类型与四值逻辑运算。
//
//  四值：0 / 1 / X（不定）/ Z（无驱动）。
//
//  两条约定：
//   1. **不做强度仲裁**。一个网络上有多个驱动源且值不一致，一律判 X，不区分谁强谁弱。
//   2. **Z 只在网络层面表示「没人驱动」**；一旦 Z 进到元件的输入引脚，就按 X 处理
//      （悬空输入没法知道电平）。这一转换只在 GateEval 里做一次，别处不用管。
//
//  LogicSignal 定义在 core/Wire.h —— 导线着色一直在用它，本文件只补运算，
//  Not / And / Or 三个沿用 Wire.cpp 里的实现（已经是标准四值表）。
// =============================================================================

#include "core/Wire.h"

#include <vector>

namespace sf
{
namespace sim
{

// -----------------------------------------------------------------------------
//  四值运算
// -----------------------------------------------------------------------------
// 说明：And / Or 带支配律 —— `0 AND X = 0`、`1 OR X = 1`，这与 IEEE 1364 一致，
// 也是 Wire.cpp 里 LogicAnd / LogicOr 的行为。Xor 没有支配律：任一操作数不定就不定。

/// 缓冲：Z（悬空）当 X 处理，其余原样。
LogicSignal Buf(LogicSignal a);

LogicSignal Xor(LogicSignal a, LogicSignal b);
LogicSignal Nand(LogicSignal a, LogicSignal b);
LogicSignal Nor(LogicSignal a, LogicSignal b);
LogicSignal Xnor(LogicSignal a, LogicSignal b);

/// 多输入折叠。空向量按单位元返回（AndAll → One，其余 → Zero）。
LogicSignal AndAll(const std::vector<LogicSignal>& v);
LogicSignal OrAll(const std::vector<LogicSignal>& v);
LogicSignal XorAll(const std::vector<LogicSignal>& v);

/// 二选一：sel 为 0 选 a、为 1 选 b；sel 不定时只有 a == b 才敢给确定值。
LogicSignal Mux(LogicSignal sel, LogicSignal a, LogicSignal b);

/// 一位全加器的结果
struct AdderResult
{
    LogicSignal sum = LogicSignal::X;
    LogicSignal carry = LogicSignal::X;
};

/// 一位全加：sum = a ^ b ^ cin，carry = 三取二多数。
AdderResult FullAdd(LogicSignal a, LogicSignal b, LogicSignal cin);

/// 把一个网络上所有驱动源的输出归并成一个逻辑值。
/// · 没有任何有效驱动（空向量，或全是 Z）→ Z
/// · 多个驱动且值不一致 → X（冲突）
LogicSignal ResolveDrivers(const std::vector<LogicSignal>& drivers);

// -----------------------------------------------------------------------------
//  引脚角色 / 元件求值类别
// -----------------------------------------------------------------------------

/// 一个引脚在仿真里扮演什么角色
enum class PinRole
{
    Unused,  ///< 不参与求值（inout、或元件本身不驱动）
    Driver,  ///< 输出引脚：它驱动所在网络
    Load     ///< 输入引脚：它读取所在网络的电平
};

wxString PinRoleName(PinRole r);

/// 元件的求值类别，由 ComponentDef 的 gate / kind 推出来（见 GateEval.cpp）。
enum class EvalKind
{
    None,        ///< 不认识 / 不参与（led、output 端口、无标签元件）
    Gate,        ///< 组合门：按 gate 名折叠输入
    Constant,    ///< 常量驱动：const0 / const1 / ground
    InputPort,   ///< 可翻转的输入端口
    Clock,       ///< 时钟源
    Tunnel,      ///< 命名隧道（网络别名，建网时已合流，求值时不做事）
    Splitter,    ///< 位分离器（本期 1 位透传）
    Mux,         ///< 二选一
    FullAdder,   ///< 一位全加器
    Dff          ///< D 触发器（时序，S2 才接入状态）
};

wxString EvalKindName(EvalKind k);

}  // namespace sim
}  // namespace sf
