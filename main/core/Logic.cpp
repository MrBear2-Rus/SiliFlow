#include "Logic.h"

#include <algorithm>

namespace sf
{

// -----------------------------------------------------------------------------
//  四值逻辑（从 Wire.cpp 挪过来，名字不变）
// -----------------------------------------------------------------------------
wxString LogicSignalName(LogicSignal s)
{
    switch (s)
    {
    case LogicSignal::Zero: return "0";
    case LogicSignal::One:  return "1";
    case LogicSignal::X:    return "X";
    case LogicSignal::Z:    return "Z";
    }
    return "Z";
}

LogicSignal LogicSignalFromName(const wxString& s, LogicSignal def)
{
    if (s == "0") return LogicSignal::Zero;
    if (s == "1") return LogicSignal::One;
    if (s.IsSameAs("X", false)) return LogicSignal::X;
    if (s.IsSameAs("Z", false)) return LogicSignal::Z;
    return def;
}

wxColour LogicSignalColour(LogicSignal s)
{
    switch (s)
    {
    case LogicSignal::Zero: return palette::kLogicZero;
    case LogicSignal::One:  return palette::kLogicOne;
    case LogicSignal::X:    return palette::kLogicX;
    case LogicSignal::Z:    return palette::kLogicZ;
    }
    return palette::kWireIdle;
}

LogicSignal LogicNot(LogicSignal a)
{
    switch (a)
    {
    case LogicSignal::Zero: return LogicSignal::One;
    case LogicSignal::One:  return LogicSignal::Zero;
    default:                return LogicSignal::X;  // X/Z 取反仍是 X
    }
}

LogicSignal LogicAnd(LogicSignal a, LogicSignal b)
{
    if (a == LogicSignal::Zero || b == LogicSignal::Zero) return LogicSignal::Zero;
    if (a == LogicSignal::One && b == LogicSignal::One) return LogicSignal::One;
    return LogicSignal::X;
}

LogicSignal LogicOr(LogicSignal a, LogicSignal b)
{
    if (a == LogicSignal::One || b == LogicSignal::One) return LogicSignal::One;
    if (a == LogicSignal::Zero && b == LogicSignal::Zero) return LogicSignal::Zero;
    return LogicSignal::X;
}

// -----------------------------------------------------------------------------
//  LogicVector
// -----------------------------------------------------------------------------
namespace
{

inline LogicSignal DecodeBit(uint32_t value, uint32_t unknown, int i)
{
    const bool u = ((unknown >> i) & 1u) != 0;
    const bool v = ((value >> i) & 1u) != 0;
    if (!u) return v ? LogicSignal::One : LogicSignal::Zero;
    return v ? LogicSignal::Z : LogicSignal::X;
}

inline void EncodeBit(LogicSignal s, int i, uint32_t& value, uint32_t& unknown)
{
    const uint32_t m = 1u << i;
    switch (s)
    {
    case LogicSignal::Zero:
        value &= ~m;
        unknown &= ~m;
        break;
    case LogicSignal::One:
        value |= m;
        unknown &= ~m;
        break;
    case LogicSignal::X:
        value &= ~m;
        unknown |= m;
        break;
    case LogicSignal::Z:
        value |= m;
        unknown |= m;
        break;
    }
}

inline int ClampWidth(int w)
{
    if (w < 1) return 1;
    if (w > kMaxBusWidth) return kMaxBusWidth;
    return w;
}

}  // namespace

uint32_t LogicVector::Mask() const
{
    if (m_width >= 32) return 0xFFFFFFFFu;
    if (m_width <= 0) return 0u;
    return (1u << m_width) - 1u;
}

LogicVector::LogicVector(int width) : m_width(ClampWidth(width))
{
    // 全 Z
    m_value = Mask();
    m_unknown = Mask();
}

LogicVector::LogicVector(int width, LogicSignal fill) : m_width(ClampWidth(width))
{
    for (int i = 0; i < m_width; ++i) EncodeBit(fill, i, m_value, m_unknown);
}

LogicVector LogicVector::FromSignal(LogicSignal s)
{
    LogicVector v(1);
    v.SetBit(0, s);
    return v;
}

LogicVector LogicVector::FromText(const wxString& bits, int width)
{
    const int n = static_cast<int>(bits.length());
    const int w = ClampWidth(width > 0 ? width : (n > 0 ? n : 1));

    LogicVector v(w);
    // 左边是最高位
    for (int i = 0; i < n && i < w; ++i)
    {
        const wxUniChar c = bits[n - 1 - i];
        LogicSignal s = LogicSignal::Z;
        if (c == '0')
            s = LogicSignal::Zero;
        else if (c == '1')
            s = LogicSignal::One;
        else if (c == 'x' || c == 'X')
            s = LogicSignal::X;
        else
            s = LogicSignal::Z;
        v.SetBit(i, s);
    }
    return v;
}

LogicVector LogicVector::FromInt(long long value, int width)
{
    LogicVector v(width);
    const uint32_t mask = v.Mask();
    const uint32_t bits = static_cast<uint32_t>(static_cast<unsigned long long>(value) & mask);
    v.m_value = bits;
    v.m_unknown = 0;
    return v;
}

Json LogicVector::ToJson(const LogicVector& v)
{
    Json j;
    j["width"] = v.m_width;
    j["bits"] = ToUtf8(v.ToText());
    return j;
}

LogicVector LogicVector::FromJson(const Json& j)
{
    const int w = GetInt(j, "width", 1);
    return FromText(GetStr(j, "bits"), w);
}

LogicSignal LogicVector::Bit(int i) const
{
    if (i < 0 || i >= m_width) return LogicSignal::Z;
    return DecodeBit(m_value, m_unknown, i);
}

void LogicVector::SetBit(int i, LogicSignal s)
{
    if (i < 0 || i >= m_width) return;
    EncodeBit(s, i, m_value, m_unknown);
}

bool LogicVector::AllKnown() const
{
    return (m_unknown & Mask()) == 0u;
}

LogicVector LogicVector::Resize(int width) const
{
    const int w = ClampWidth(width);
    LogicVector v(w);

    const int n = (w < m_width) ? w : m_width;
    for (int i = 0; i < n; ++i) v.SetBit(i, Bit(i));
    // 展宽的高位保持 Z（表示未连接），而不是 0 —— 免得把「没接」显示成「接 0」
    return v;
}

LogicVector LogicVector::Slice(int lo, int hi) const
{
    if (lo > hi) std::swap(lo, hi);
    if (lo < 0) lo = 0;
    if (hi >= m_width) hi = m_width - 1;
    if (hi < lo) return LogicVector(1);

    LogicVector v(hi - lo + 1);
    for (int i = 0; i <= hi - lo; ++i) v.SetBit(i, Bit(lo + i));
    return v;
}

LogicVector LogicVector::Concat(const LogicVector& low) const
{
    const int w = ClampWidth(m_width + low.m_width);
    LogicVector v(w);

    for (int i = 0; i < low.m_width && i < w; ++i) v.SetBit(i, low.Bit(i));
    for (int i = 0; i < m_width && (low.m_width + i) < w; ++i) v.SetBit(low.m_width + i, Bit(i));
    return v;
}

LogicVector LogicVector::Not() const
{
    LogicVector v(m_width);
    for (int i = 0; i < m_width; ++i) v.SetBit(i, LogicNot(Bit(i)));
    return v;
}

LogicVector LogicVector::And(const LogicVector& o) const
{
    const int w = ClampWidth(m_width > o.m_width ? m_width : o.m_width);
    LogicVector v(w);
    for (int i = 0; i < w; ++i) v.SetBit(i, LogicAnd(Bit(i), o.Bit(i)));
    return v;
}

LogicVector LogicVector::Or(const LogicVector& o) const
{
    const int w = ClampWidth(m_width > o.m_width ? m_width : o.m_width);
    LogicVector v(w);
    for (int i = 0; i < w; ++i) v.SetBit(i, LogicOr(Bit(i), o.Bit(i)));
    return v;
}

LogicVector LogicVector::Xor(const LogicVector& o) const
{
    const int w = ClampWidth(m_width > o.m_width ? m_width : o.m_width);
    LogicVector v(w);

    for (int i = 0; i < w; ++i)
    {
        const LogicSignal a = Bit(i);
        const LogicSignal b = o.Bit(i);

        const bool aKnown = (a == LogicSignal::Zero || a == LogicSignal::One);
        const bool bKnown = (b == LogicSignal::Zero || b == LogicSignal::One);
        if (!aKnown || !bKnown)
            v.SetBit(i, LogicSignal::X);
        else
            v.SetBit(i, (a != b) ? LogicSignal::One : LogicSignal::Zero);
    }
    return v;
}

bool LogicVector::Equals(const LogicVector& o) const
{
    if (m_width != o.m_width) return false;

    const uint32_t mask = Mask();
    return (m_value & mask) == (o.m_value & mask) && (m_unknown & mask) == (o.m_unknown & mask);
}

wxString LogicVector::ToText() const
{
    wxString s;
    for (int i = m_width - 1; i >= 0; --i) s += LogicSignalName(Bit(i));
    return s;
}

unsigned long long LogicVector::ToUnsigned() const
{
    // 不定位按 0 处理：Z 编码是 (v=1,u=1)，X 是 (v=0,u=1)，所以 v & ~u 正好过滤掉
    return static_cast<unsigned long long>(m_value & ~m_unknown & Mask());
}

bool LogicVector::ToInt(long long* out) const
{
    if (!AllKnown()) return false;
    if (out != nullptr) *out = static_cast<long long>(ToUnsigned());
    return true;
}

wxString LogicVector::ToDisplay() const
{
    if (m_width <= 1) return LogicSignalName(Bit(0));
    if (!AllKnown()) return ToText();
    return wxString::Format("0x%llX", ToUnsigned());
}

LogicVector LogicVector::ResolveDrivers(const std::vector<LogicVector>& drivers, int width)
{
    const int w = ClampWidth(width);
    LogicVector v(w);

    for (int i = 0; i < w; ++i)
    {
        LogicSignal result = LogicSignal::Z;
        bool have = false;

        for (const LogicVector& d : drivers)
        {
            const LogicSignal s = d.Bit(i);
            if (s == LogicSignal::Z) continue;

            if (!have)
            {
                result = s;
                have = true;
                continue;
            }
            if (s != result)
            {
                result = LogicSignal::X;  // 冲突
                break;
            }
        }

        v.SetBit(i, have ? result : LogicSignal::Z);
    }

    return v;
}

}  // namespace sf
