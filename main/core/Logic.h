#pragma once

// =============================================================================
//  SiliFlow · core/Logic.h
//  四值逻辑的值类型。
//
//  LogicSignal —— 单个位：0 / 1 / X（不定）/ Z（无驱动）
//  LogicVector —— 位向量（总线）：width 位，每一位都是四值
//
//  为什么打包：求值时每个网络每次传播都要取值，如果用 vector<LogicSignal>
//  存，1000 个网络就是每帧上千次堆分配。这里用两个 uint32 存 32 位
//  （value / unknown 各 1 bit），零分配、可平凡拷贝。
//
//  每一位的编码：
//      0 → (v=0, u=0)
//      1 → (v=1, u=0)
//      X → (v=0, u=1)
//      Z → (v=1, u=1)
//
//  这三条是从 core/Wire.h 挪过来的（原处只留包含），对外名字不变。
// =============================================================================

#include "Json.h"
#include "Types.h"

#include <wx/string.h>

#include <cstdint>
#include <vector>

namespace sf
{

// -----------------------------------------------------------------------------
//  四值逻辑
// -----------------------------------------------------------------------------
enum class LogicSignal
{
    Zero,  ///< 0
    One,   ///< 1
    X,     ///< 不定
    Z      ///< 高阻 / 无驱动（默认）
};

wxString LogicSignalName(LogicSignal s);
LogicSignal LogicSignalFromName(const wxString& s, LogicSignal def = LogicSignal::Z);
wxColour LogicSignalColour(LogicSignal s);

/// 四值辅助函数。And / Or 带支配律（`0 AND X = 0`、`1 OR X = 1`），与 IEEE 1364 一致。
LogicSignal LogicNot(LogicSignal a);
LogicSignal LogicAnd(LogicSignal a, LogicSignal b);
LogicSignal LogicOr(LogicSignal a, LogicSignal b);

/// 位宽上限。够画 ALU / 寄存器堆，又不至于让真值表组合爆炸。
constexpr int kMaxBusWidth = 32;

// -----------------------------------------------------------------------------
//  位向量（总线）
// -----------------------------------------------------------------------------
class LogicVector
{
public:
    LogicVector() = default;  ///< 1 位 Z

    explicit LogicVector(int width);                   ///< 指定位宽，全 Z
    LogicVector(int width, LogicSignal fill);          ///< 指定位宽 + 填充值

    static LogicVector FromSignal(LogicSignal s);      ///< 1 位
    static LogicVector FromText(const wxString& bits, int width = 0);  ///< "1010"（左高右低）
    static LogicVector FromInt(long long v, int width);
    static Json ToJson(const LogicVector& v);
    static LogicVector FromJson(const Json& j);

    int Width() const { return m_width; }
    bool IsEmpty() const { return m_width <= 0; }

    /// 第 i 位（0 = 最低位）。越界返回 Z。
    LogicSignal Bit(int i) const;
    void SetBit(int i, LogicSignal s);

    /// 所有位都确定（不含 X / Z）
    bool AllKnown() const;

    /// 展宽（高位补 0）/ 截断（保留低位）
    LogicVector Resize(int width) const;
    /// 取 [lo, hi] 这一段（含两端，0 = 最低位）
    LogicVector Slice(int lo, int hi) const;
    /// 拼接：this 在高位，low 在低位
    LogicVector Concat(const LogicVector& low) const;

    /// 逐位取反 / 与 / 或 / 异或（位宽取两者较大者，窄的补 0）
    LogicVector Not() const;
    LogicVector And(const LogicVector& o) const;
    LogicVector Or(const LogicVector& o) const;
    LogicVector Xor(const LogicVector& o) const;

    bool Equals(const LogicVector& o) const;
    bool operator==(const LogicVector& o) const { return Equals(o); }
    bool operator!=(const LogicVector& o) const { return !Equals(o); }

    wxString ToText() const;  ///< "1010" / "10XZ"，左高右低
    /// 当作无符号整数。含 X / Z 的位按 0 处理（只用于显示）。
    unsigned long long ToUnsigned() const;
    /// 转整数；含 X / Z 返回 false。
    bool ToInt(long long* out) const;

    /// 整数显示用：width > 1 时用十六进制（"0xA5"），否则 "0/1/X/Z"
    wxString ToDisplay() const;

    /// 多驱动归并（逐位）：该位全 Z → Z；有冲突 → X
    static LogicVector ResolveDrivers(const std::vector<LogicVector>& drivers, int width);

private:
    uint32_t Mask() const;

    int m_width = 1;
    uint32_t m_value = 0;    ///< bit i = 该位的值
    uint32_t m_unknown = 0;  ///< bit i = 该位是否不定（X 或 Z）
};

}  // namespace sf
