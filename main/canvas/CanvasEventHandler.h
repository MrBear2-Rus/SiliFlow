#pragma once

// =============================================================================
//  SiliFlow · canvas/CanvasEventHandler.h
//  把手势翻译成模型变更。
//
//  事件矩阵（简表）：
//    工具      LeftDown                    LeftUp                Motion
//    DRAG      进入画布拖拽                结束拖拽              平移画布
//    SELECT    命中→拖动选中物；空白→框选   提交移动 / 提交框选     画选框 / 拖动
//              命中输入端口时先记下待翻转
//    COMPONENT 放置元件（可连续放置）        —                     更新预览
//    WIRE      吸附引脚→开始拉线            落点吸附→完成一根       橡皮筋预览
//    TEXT      创建/编辑文本               提交                    —
//    DRAWING   起点                         终点                   预览
//    ERASER    点擦                         框擦提交               画擦除框
//
//  撤销事务边界：一次完整手势 = 一个事务（LeftDown 取 before，LeftUp 取 after）。
//  画布平移 / 缩放**不记入**撤销栈。
//  翻转输入端口电平也**不记入**撤销栈 —— 撤销只该撤图纸编辑，不该往回撤电平。
// =============================================================================

#include "ToolStateMachine.h"
#include "core/Command.h"
#include "core/Shape.h"
#include "core/Types.h"
#include "core/Wire.h"

#include <wx/string.h>

#include <memory>
#include <utility>
#include <vector>

class wxMouseEvent;
class wxKeyEvent;

namespace sf
{

class CanvasPanel;
class CircuitSheet;
class ComponentInstance;

class CanvasEventHandler
{
public:
    explicit CanvasEventHandler(CanvasPanel* panel);

    // ---- 鼠标 ----
    void OnLeftDown(wxMouseEvent& event);
    void OnLeftUp(wxMouseEvent& event);
    void OnLeftDoubleClick(wxMouseEvent& event);
    void OnRightDown(wxMouseEvent& event);
    void OnRightUp(wxMouseEvent& event);
    void OnMiddleDown(wxMouseEvent& event);
    void OnMiddleUp(wxMouseEvent& event);
    void OnMotion(wxMouseEvent& event);
    void OnLeave(wxMouseEvent& event);
    void OnCaptureLost();

    /// 返回 true 表示事件已被消费，不再往下传。
    bool OnKeyDown(wxKeyEvent& event);

    void SetSpacePanning(bool on) { m_spacePanning = on; }
    bool IsSpacePanning() const { return m_spacePanning; }

    // ---- 供面板/菜单调用 ----
    /// 进入「放置元件」模式（由元件库面板触发）。
    void BeginPlacement(const wxString& defType);
    void CancelPlacement();
    wxString PlacementType() const { return m_placeType; }

    /// 绘图工具当前要画的形状
    enum class DrawKind
    {
        Line,
        Rectangle,
        Circle
    };
    void SetDrawKind(DrawKind k) { m_drawKind = k; }
    DrawKind GetDrawKind() const { return m_drawKind; }

    // ---- 编辑命令 ----
    void DeleteSelection();
    void SelectAll();
    void ClearSelection();
    void RotateSelection(int quarters);
    void MirrorSelection();
    void NudgeSelection(const Point& delta);

    /// 属性面板下发的元件属性修改（自带事务）
    void ApplyComponentProperty(int compId, const wxString& prop, const wxString& value);
    /// 属性面板下发的导线属性修改（自带事务）
    void ApplyWireProperty(int wireId, const wxString& prop, const wxString& value);

    /// 选中并滚动到某个元件（树面板双击定位）
    void FocusComponent(int compId);
    /// 高亮某个网络的全部导线（网络树 / 波形联动）
    void HighlightNet(const wxString& netName);

    /// 当前是否有未完成的手势（有则先取消）
    bool IsGestureActive() const;
    /// 取消当前手势并回滚到手势开始前的状态
    void CancelGesture();

private:
    CanvasPanel* Panel() const { return m_panel; }
    CircuitSheet* Sheet() const;

    Point DeviceToCanvas(const wxMouseEvent& event) const;
    Point SnappedPoint(const wxMouseEvent& event) const;

    void BeginTransaction(const wxString& name);
    void CommitTransaction();
    /// 放弃当前事务（不入撤销栈）
    void AbandonTransaction();

    void RefreshHover(const Point& canvasPos);
    void ResetTransientState();

    // ---- 各工具的处理 ----
    void HandleSelectDown(const Point& canvasPos, bool shift);
    void HandleWireDown(const Point& canvasPos);
    void HandleComponentDown(const Point& canvasPos);
    void HandleEraseDown(const Point& canvasPos);
    void HandleTextDown(const Point& canvasPos);
    void HandleDrawingDown(const Point& canvasPos);
    void HandleDrawingUp(const Point& canvasPos);

    void UpdatePlacementPreview(const Point& canvasPos);

    /// 单字母快捷键切换到某个工具（S/T/C/W/D/E）
    void SwitchToolFromShortcut(ToolType tool);

    CanvasPanel* m_panel = nullptr;

    /// 拖动前的导线原始状态（用于取消手势时精确还原）
    struct WireOriginal
    {
        Wire* wire = nullptr;
        std::vector<ControlPoint> pts;
        WireEndpoint left, right;
    };

    // ---- 手势状态 ----
    bool m_spacePanning = false;
    bool m_middlePanning = false;
    bool m_leftPanning = false;
    Point m_panAnchorDevice;

    bool m_dragSelecting = false;
    Point m_dragStartCanvas;
    std::vector<std::pair<ComponentInstance*, Point>> m_movingComponents;
    std::vector<WireOriginal> m_movingWires;

    /// 按下时命中的「可翻转元件」（输入端口）。松开时若几乎没移动就翻转它。
    int m_clickToggleCompId = -1;
    Point m_pressCanvasPos;

    bool m_rubberBanding = false;
    std::vector<Point> m_rubberBand;
    WireEndpoint m_pendingStart;
    bool m_hasPendingStart = false;

    int m_editingWireId = -1;
    int m_editingCpIndex = -1;
    std::vector<ControlPoint> m_editOriginalPts;
    WireEndpoint m_editOriginalLeft, m_editOriginalRight;

    bool m_drawingShape = false;
    Point m_drawStart;
    DrawKind m_drawKind = DrawKind::Rectangle;

    // ---- 瞬态绘制数据（渲染器通过 RenderOptions 拿到它们的地址）----
    Rect m_selectionRect;
    Rect m_eraserRect;
    Shape m_previewShape;
    bool m_hasPreviewShape = false;

    // ---- 放置模式 ----
    wxString m_placeType;
    std::unique_ptr<ComponentInstance> m_placementPreview;

    // ---- 撤销事务 ----
    std::unique_ptr<ScopeTransaction> m_tx;
};

}  // namespace sf
