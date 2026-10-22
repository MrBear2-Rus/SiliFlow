#pragma once

// =============================================================================
//  SiliFlow · core/sim/SimEngine.h
//  仿真引擎：组合逻辑求值（v4 起支持多位总线）。
//
//  设计要点（对应设计方案 7.6 + v4 升级方案 U0）：
//   · 求值图直接复用 CircuitSheet::BuildNets() 的并查集结果 —— 网络就是等电位点，
//     求值只在「元件引脚 ↔ 网络」之间跑，不另建一张图。
//   · 网络的值是 LogicVector（位向量）。**位宽由引脚决定**：同一网络上所有引脚的
//     位宽必须一致，不一致就判 X 并在状态栏报出来（和 Logisim 一样）。
//   · worklist 传播到不动点；组合环由结构上的 DetectLoops() 找。
//   · 多驱动冲突逐位判 X，不做强度仲裁。
//   · 全量重建，不做增量。
//
//  ★ 硬约束：运行时的值全部放在这里，一个字节都不写回 ComponentInstance / Wire。
//    撤销栈是图纸级 JSON 快照，仿真值一旦进图纸数据，用户每点一次输入端口都会压
//    一张快照，撤销就变成「往回撤电平」。
// =============================================================================

#include "GateEval.h"
#include "SimTypes.h"

#include "core/Circuit.h"

#include <unordered_map>
#include <vector>

namespace sf
{
namespace sim
{

/// 一个元件的运行时状态
struct CompState
{
    CompEval eval;                       ///< 静态求值信息（建图时算一次）
    std::vector<int> pinNet;             ///< pins 下标 → 网络下标；-1 表示该引脚悬空
    std::vector<LogicVector> outPins;    ///< pins 下标 → 该引脚当前驱动值（只有 Driver 位有效）
};

class SimEngine
{
public:
    SimEngine() = default;
    ~SimEngine() = default;

    SimEngine(const SimEngine&) = delete;
    SimEngine& operator=(const SimEngine&) = delete;

    // -------------------------------------------------------------------------
    //  生命周期
    // -------------------------------------------------------------------------

    void Attach(CircuitSheet* sheet);  ///< 切到另一张图纸（清空全部运行时状态与强制电平）
    void MarkDirty();                  ///< 图纸被编辑过，下次访问时重建
    void Reset();                      ///< 清空强制电平 + 复位计数，并立刻重算

    bool IsAttached() const { return m_sheet != nullptr; }
    bool IsRunning() const { return m_running; }
    void SetRunning(bool on) { m_running = on; }

    // -------------------------------------------------------------------------
    //  交互
    // -------------------------------------------------------------------------

    bool TogglePin(int compId);           ///< 翻转一个输入端口的电平（0 ↔ 1）
    bool IsToggleable(int compId) const;  ///< 这个元件能不能被鼠标翻转

    bool SetForced(int compId, const LogicVector& v);
    void ClearForced(int compId);
    bool ForcedValue(int compId, LogicVector* out) const;

    // -------------------------------------------------------------------------
    //  推进
    // -------------------------------------------------------------------------

    void Evaluate();  ///< 组合求值到不动点
    void Tick();      ///< 走一拍（S2 才真正推动时序元件）

    // -------------------------------------------------------------------------
    //  查询（只读，供渲染器 / 面板用；不会触发重建）
    // -------------------------------------------------------------------------

    /// 导线所属网络的值。没有仿真结果返回 false。
    bool TryWireValue(int wireId, LogicVector& out) const;
    /// 兼容层：只取第 0 位
    bool TryWireSignal(int wireId, LogicSignal& out) const;

    /// 引脚所在网络的值。取不到返回 1 位 Z。
    LogicVector PinValue(int compId, int pinIndex) const;
    /// 兼容层：只取第 0 位
    LogicSignal PinSignal(int compId, int pinIndex) const;

    /// 引脚自身的位宽（来自元件模板与实例参数）
    int PinWidth(int compId, int pinIndex) const;
    /// 网络的位宽（1..32）。网络不存在返回 1。
    int NetWidth(int net) const;
    /// 导线所在网络的位宽；不知道返回 1。
    int WireBits(int wireId) const;

    int TickCount() const { return m_tick; }
    bool IsOscillating() const { return m_oscillating; }
    int LastConflictCount() const { return m_conflicts; }
    int WidthMismatchCount() const { return m_widthBad; }

    /// 组合环（反馈回路）的处数。D 触发器算「打断点」，穿过它不算环。
    int LoopCount() const { return m_loopCount; }
    bool HasCombinationalLoop() const { return m_loopCount > 0; }

    size_t NetCount() const { return m_nets.size(); }
    size_t ComponentCount() const { return m_comps.size(); }
    const wxString& StatusText() const { return m_status; }

private:
    /// 惰性重建。标成 const 是为了让只读查询（画布取导线电平）也能自给自足 ——
    /// 「建图」只是把图纸翻译成求值用的缓存，不改变引擎的对外语义。
    void EnsureBuilt() const;
    void Build() const;
    void MergeTunnels() const;
    void DetectLoops() const;
    void ComputeNetWidths() const;

    LogicVector ResolveNet(int net) const;
    int NetOfPin(int compId, int pinIndex) const;
    const CompState* CompOf(int compId) const;
    void CountConflicts();

    CircuitSheet* m_sheet = nullptr;

    // ---- 惰性缓存（const 查询会重建，所以是 mutable）----
    mutable bool m_dirty = true;
    mutable bool m_built = false;
    mutable std::vector<Net> m_nets;
    mutable std::vector<LogicVector> m_netValue;
    mutable std::vector<int> m_netWidth;      ///< 每个网络的位宽
    mutable std::vector<char> m_netWidthBad;  ///< 该网络上的引脚位宽不一致
    mutable std::unordered_map<int, int> m_netOfWire;
    mutable std::unordered_map<int, CompState> m_comps;
    mutable int m_loopCount = 0;
    mutable int m_widthBad = 0;
    mutable bool m_hasResult = false;

    // ---- 求值结果 ----
    bool m_running = false;
    bool m_oscillating = false;
    int m_conflicts = 0;
    int m_tick = 0;

    /// 用户强制的引脚值。与 CompState 分开存，这样重建求值图不会丢用户的设置。
    std::unordered_map<int, LogicVector> m_forced;

    wxString m_status;
};

}  // namespace sim
}  // namespace sf
