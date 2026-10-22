#pragma once

// =============================================================================
//  SiliFlow · core/Wire.h
//  导线：控制点序列 + 两端端点 + 四值逻辑信号。
//
//  与 SigFlow 的关键差异：这里**没有** Cell 与仿真推演逻辑。
//  `signal` 本期只由两处写入——手工指定、导入 VCD 后由波形赋值。
//  四值枚举保留是为了导线着色，以及后续接仿真内核时能直接用。
// =============================================================================

#include "Json.h"
#include "Logic.h"
#include "Types.h"

#include <vector>

class wxGraphicsContext;

namespace sf
{

// -----------------------------------------------------------------------------
//  控制点
// -----------------------------------------------------------------------------
enum class CPType
{
    Pin,     ///< 端点（落在引脚上）
    Bend,    ///< 用户手工拖出来的拐点
    Free,    ///< 自动布线生成的拐点
    Branch   ///< 分支点（三通以上）
};

wxString CPTypeName(CPType t);
CPType CPTypeFromName(const wxString& s, CPType def = CPType::Free);

struct ControlPoint
{
    Point pos;
    CPType type = CPType::Free;
};

// -----------------------------------------------------------------------------
//  四值逻辑
//  LogicSignal / LogicVector 已经挪到 core/Logic.h（Wire.h 通过包含继续对外可见）。
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//  端点
// -----------------------------------------------------------------------------
struct WireEndpoint
{
    int compId = -1;    ///< -1 表示悬空
    int pinIndex = -1;  ///< -1 表示悬空
    Point pos;

    bool Attached() const { return compId >= 0 && pinIndex >= 0; }
    void Detach() { compId = -1; pinIndex = -1; }
};

// -----------------------------------------------------------------------------
//  导线
// -----------------------------------------------------------------------------
class Wire
{
public:
    int id = 0;
    std::vector<ControlPoint> pts;
    WireEndpoint left;   ///< 起点（通常接输出脚）
    WireEndpoint right;  ///< 终点（通常接输入脚）
    LogicSignal signal = LogicSignal::Z;  ///< 手工指定的电平（只对 1 位导线有效）
    bool selected = false;
    bool hovered = false;

    /// 描边粗细（像素）。**这不是信号位宽** —— 位宽由两端引脚的 `Pin::width` 决定，
    /// 见 Component.h。改名就是为了把 `width` 让给位宽，别再混用。
    int strokeWidth = 2;

    // ---- 几何 ----
    Point Start() const;
    Point End() const;
    Rect Bounds() const;
    int SegmentCount() const;
    int Length() const;  ///< 折线总长（像素）

    // ---- 布线 ----
    /// 默认 Z 形走线：水平 → 垂直 → 水平，中点取 (a.x+b.x)/2。
    /// a、b 同行或同列时退化为两点直线。
    static std::vector<ControlPoint> Route(const Point& a, const Point& b);

    /// 用若干必经点连成正交折线（相邻两点不对齐时插入一个拐角）。
    static std::vector<ControlPoint> OrthogonalPath(const std::vector<Point>& waypoints);

    // ---- 命中测试 ----
    bool HitTest(const Point& p, int tol = 5) const;
    int HitTestControlPoint(const Point& p, int tol = 6) const;  ///< 返回控制点下标，-1 表示未命中

    // ---- 编辑 ----
    void Move(const Point& delta);                                  ///< 整体平移
    void MovePoint(int idx, const Point& p);                        ///< 移动单个控制点
    void SetLeftEndpoint(const WireEndpoint& e);
    void SetRightEndpoint(const WireEndpoint& e);
    void ReshapeFromEndpoints();                                    ///< 保留 Bend 手工拐点，重算 Free 点
    void Simplify();                                                ///< 去重 + 去共线冗余点

    // ---- 绘制 ----
    /// 用导线自身的 selected / hovered 状态着色。
    void Draw(wxGraphicsContext* gc, bool drawHandles = false, int handleRadius = 3) const;

    /// 显式指定着色状态（渲染器用，避免为了高亮而去改模型）。
    void DrawStyled(wxGraphicsContext* gc, bool hovered, bool selected, bool drawHandles,
                    int handleRadius) const;

    /// 同上，但电平与描边粗细都由外部给定 —— 仿真开启时用引擎算出来的值，
    /// 总线（位宽 > 1）也要画粗一点，这两件事都不能靠改模型来实现。
    void DrawStyled(wxGraphicsContext* gc, bool hovered, bool selected, bool drawHandles,
                    int handleRadius, LogicSignal effectiveSignal, int penWidth) const;

    // ---- 序列化 ----
    Json ToJson() const;
    void FromJson(const Json& j);
};

}  // namespace sf
