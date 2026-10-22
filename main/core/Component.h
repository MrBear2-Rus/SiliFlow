#pragma once

// =============================================================================
//  SiliFlow · core/Component.h
//  元件模板（ComponentDef）与元件实例（ComponentInstance）。
//
//  两条关键约定：
//   1. 引脚的「定义坐标」是相对元件左上角的局部坐标；
//      实例上的引脚坐标是经过「绕中心 → 镜像 → 旋转 90°×q → 平移」变换后的
//      最终局部坐标（仍然是相对实例左上角）。
//   2. 变换只有一份实现（Rebuild 内部的 TransformLocal），别处不得重算。
//      旋转/镜像必须保持元件中心不动，否则元件会在图上「跑位」。
// =============================================================================

#include "Json.h"
#include "Logic.h"
#include "Shape.h"
#include "Types.h"

#include <memory>
#include <vector>

namespace sf
{

class Library;

// -----------------------------------------------------------------------------
//  引脚
// -----------------------------------------------------------------------------
enum class PinDir
{
    Input,
    Output,
    InOut
};

wxString PinDirName(PinDir dir);
PinDir PinDirFromName(const wxString& s);

struct Pin
{
    Point pos;                ///< 变换后的局部坐标（相对元件左上角）
    PinDir dir = PinDir::Input;
    wxString identifier;      ///< "A" / "B" / "Y" / "CLK"
    int index = 0;            ///< 在 ComponentInstance::pins 中的下标
    bool inverted = false;    ///< 反相小圆圈（只影响绘制）

    /// 数据位宽。**0 表示「跟随元件的位宽参数」**（`ComponentInstance::width`）；
    /// 固定值（比如时钟脚恒定 1 位）则写死。`Rebuild()` 时解析成确定值。
    int width = 0;
};

// -----------------------------------------------------------------------------
//  元件模板
// -----------------------------------------------------------------------------
class ComponentDef
{
public:
    wxString type;         ///< 唯一标识，如 "and"
    wxString name;         ///< 显示名，如 "与门"
    wxString category;     ///< 分类，如 "逻辑门"
    wxString description;
    wxSize size = wxSize(0, 0);  ///< 官方尺寸（来自 "bounds"），缺省时由 LocalBounds() 推导

    std::vector<Pin> inputs, outputs, inouts;
    std::vector<Shape> shapes;

    wxString gate;  ///< Verilog 原语："and" / "or" / "not" / "nand" ... 空表示无对应
    wxString kind;  ///< "port" / "clock" / "const" / "dff" / "mux" / "led" / ""

    /// 图元 + 引脚的并集包围盒，平移到以 (0,0) 为左上角。
    Rect LocalBounds() const;

    /// 按标识符找引脚（不区分大小写）。找不到返回 nullptr。
    const Pin* FindPin(const wxString& id) const;

    /// 全部引脚（inputs → outputs → inouts 顺序）。
    std::vector<const Pin*> AllPins() const;

    Json ToJson() const;
    static std::unique_ptr<ComponentDef> FromJson(const Json& j);

    /// 便捷判定
    bool IsGate() const { return !gate.IsEmpty(); }
    bool IsPort() const { return kind == "port"; }
};

// -----------------------------------------------------------------------------
//  元件实例
// -----------------------------------------------------------------------------
class ComponentInstance
{
public:
    int id = 0;                 ///< 图纸内唯一
    wxString defType;           ///< 指向 ComponentDef::type
    Point pos = Point(0, 0);    ///< 画布坐标下的左上角
    int rotation = 0;           ///< 顺时针 90° 的次数，0..3
    bool mirrored = false;
    wxString label;             ///< "U1" / "clk" / "a"
    wxString value;             ///< "1" / "8" / 参数
    bool selected = false;

    /// 数据位宽参数（1..kMaxBusWidth）。模板里位宽写 0 的引脚跟随它。
    int width = 1;

    // ---- 以下由 Rebuild() 派生，不要手工设置 ----
    const ComponentDef* def = nullptr;
    std::vector<Pin> pins;      ///< 顺序：inputs…outputs…inouts
    std::vector<Shape> xShapes; ///< 变换后的图元（局部坐标）
    wxSize boxSize = wxSize(0, 0);
    int inputCount = 0, outputCount = 0, inoutCount = 0;

    /// 唯一刷新入口：绑定模板并重算引脚 / 图元 / 尺寸。
    void Rebuild(const ComponentDef& d);

    Rect Bounds() const;
    Rect CanvasRect() const { return Bounds(); }

    /// 引脚在画布坐标系下的位置。
    Point PinPos(int idx) const;

    /// 命中测试（画布坐标）。
    bool HitTestBody(const Point& p, int tol = 0) const;
    int HitTestPin(const Point& p, int tol = 6) const;  ///< 返回引脚下标，-1 表示未命中

    /// 旋转 / 镜像：都以元件中心为轴，保持视觉位置不动。
    void RotateCw();
    void RotateCcw();
    void ToggleMirror();

    /// 画布坐标 → 元件局部坐标（未变换的定义坐标），用于本地化编辑。
    Point CanvasToLocal(const Point& p) const;

    /// 元件中心（画布坐标）。
    Point Center() const;

    Json ToJson() const;
    /// library 可为空：为空时只恢复 defType，需要调用方稍后补 Rebuild。
    void FromJson(const Json& j, const Library* library);

private:
    /// 定义局部坐标 → 最终局部坐标（唯一实现）。
    Point TransformLocal(const Point& lp) const;
};

}  // namespace sf
