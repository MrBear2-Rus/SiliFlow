#pragma once

// =============================================================================
//  SiliFlow · core/Circuit.h
//  图纸（Sheet）：一张原理图的全部内容，以及围绕它的命中测试、区域查询、
//  端点吸附、连线关系推导（BuildNets，供树面板与后续 Verilog 导出使用）。
// =============================================================================

#include "Component.h"
#include "Json.h"
#include "Shape.h"
#include "Wire.h"

#include <memory>
#include <vector>

namespace sf
{

class Library;

/// 引脚引用：图纸内唯一定位一个引脚
struct PinRef
{
    int compId = -1;
    int pinIndex = -1;

    bool Valid() const { return compId >= 0 && pinIndex >= 0; }
    bool operator==(const PinRef& o) const { return compId == o.compId && pinIndex == o.pinIndex; }
    bool operator<(const PinRef& o) const
    {
        return compId != o.compId ? compId < o.compId : pinIndex < o.pinIndex;
    }
};

/// 一个电气网络（由导线把若干引脚连在一起）
struct Net
{
    wxString name;
    std::vector<PinRef> pins;
    std::vector<int> wireIds;
};

class CircuitSheet
{
public:
    wxString name = "main";

    std::vector<std::unique_ptr<ComponentInstance>> components;
    std::vector<std::unique_ptr<Wire>> wires;
    std::vector<Shape> annotations;  ///< 自由文本 / 装饰图形

    int nextCompId = 1;
    int nextWireId = 1;

    // ---- 增删 ----
    ComponentInstance* AddComponent(const ComponentDef& def, const Point& pos);
    Wire* AddWire(const WireEndpoint& a, const WireEndpoint& b);
    bool RemoveComponent(int id);  ///< 级联删除相连的导线
    bool RemoveWire(int id);

    // ---- 查找 ----
    ComponentInstance* FindComponent(int id) const;
    Wire* FindWire(int id) const;

    // ---- 命中测试 ----
    /// 返回优先级最高的对象：先找引脚（更容易点中），再找本体。
    /// whichPin 非空时写出命中的引脚下标。
    ComponentInstance* HitComponent(const Point& p, int tol = 0, int* whichPin = nullptr) const;
    Wire* HitWire(const Point& p, int tol = 5) const;
    int HitControlPoint(const Point& p, int tol = 6, int* whichWireId = nullptr) const;

    // ---- 区域查询（框选 / 框擦）----
    std::vector<ComponentInstance*> ComponentsIn(const Rect& r) const;
    std::vector<Wire*> WiresIn(const Rect& r) const;

    // ---- 端点吸附 ----
    /// 找距 p 最近的可连接引脚。找到返回 true 并写出端点。
    bool SnapToPin(const Point& p, int radius, WireEndpoint& out) const;

    /// 已连到该引脚的导线数量（用于判断是否可再连线）。
    int WireCountAtPin(const PinRef& ref) const;

    // ---- 一致性维护 ----
    /// 元件移动/旋转后，刷新挂在它上面的导线端点并重新布线。
    void SyncWireEndpoints(int compId);

    // ---- 连线关系 ----
    /// 并查集推导网络。网络名优先级：port 元件标签 > 其他元件标签 > n_<序号>。
    std::vector<Net> BuildNets() const;

    /// 按网络名找导线（波形 ↔ 画布联动用）。
    std::vector<int> WireIdsOfNet(const wxString& netName) const;

    // ---- 选中集 ----
    std::vector<ComponentInstance*> SelectedComponents() const;
    std::vector<Wire*> SelectedWires() const;
    void ClearSelection();
    size_t SelectionCount() const;
    /// 框选中选中对象，additive 为 true 时不清理原有选择。返回新增选中数。
    int SelectInRect(const Rect& r, bool additive);

    // ---- 其他 ----
    Rect Bounds() const;  ///< 内容包围盒（用于「适应窗口」），空图返回空矩形
    bool IsEmpty() const { return components.empty() && wires.empty() && annotations.empty(); }
    void Clear();

    // ---- 序列化 ----
    Json ToJson() const;
    void FromJson(const Json& j, const Library* library);
};

}  // namespace sf
