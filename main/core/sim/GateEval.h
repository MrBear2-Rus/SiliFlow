#pragma once

// =============================================================================
//  SiliFlow · core/sim/GateEval.h
//  元件求值：按 ComponentDef 的 gate / kind 标签分派。
//
//  这里是「不为每个元件写类」的落点 —— 内置库已经给每个元件打好了
//    gate：and / and3 / nand / or / nor / xor / xnor / not / buf / mux2 / add
//    kind：port / const / clock / dff / splitter / tunnel / led
//  所以仿真器只需要一张分派表，而不是 25 个元件子类。
//
//  没有标签的元件判为「不驱动」（输出悬空 = Z），引擎会记一条消息，
//  绝不静默当成 0。
// =============================================================================

#include "SimTypes.h"

#include "core/Component.h"

#include <vector>

namespace sf
{
namespace sim
{

/// 归一化后的门操作（去掉 3 入变体的数字后缀）
enum class GateOp
{
    None,
    And,
    Or,
    Not,
    Nand,
    Nor,
    Xor,
    Xnor,
    Buf
};

/// 元件在仿真里的静态信息：建图时算一次，之后只查表。
struct CompEval
{
    EvalKind kind = EvalKind::None;
    GateOp op = GateOp::None;

    std::vector<PinRole> roles;   ///< 与 ComponentInstance::pins 同序
    std::vector<int> driverPins;  ///< 驱动引脚（输出）在 pins 里的下标
    std::vector<int> loadPins;    ///< 读取引脚（输入）在 pins 里的下标

    /// 需要引擎先喂值的元件：input 端口 / 常量 / 时钟
    bool isSource = false;

    /// 有输入但没有任何行为可言的元件（未知标签），引擎据此报提示
    bool unknownBehaviour = false;
};

/// 由元件模板推出求值类别。
EvalKind KindOf(const ComponentDef& def);

/// 归一化 gate 名。
GateOp GateOpOf(const wxString& gate);

/// 为实例算出静态信息。def 为空时返回一份「不参与」的空表。
CompEval MakeCompEval(const ComponentInstance& inst);

/// 纯组合求值（支持多位总线）。
/// inPins / outPins 都按 ComponentInstance::pins 的下标索引，长度等于 pins.size()。
/// 只写 outPins 里 driverPins 指到的位置，其余位置保持原样。
/// 输出会被夹到对应引脚自身的位宽 —— 让「引脚位宽」成为唯一真源。
void EvalComponent(const ComponentInstance& inst, const CompEval& e,
                   const std::vector<LogicVector>& inPins, std::vector<LogicVector>& outPins);

/// 能不能用鼠标翻转它的电平（只有 input 端口可以）。
bool IsToggleable(const CompEval& e);

}  // namespace sim
}  // namespace sf
