#include "CanvasEventHandler.h"

#include "CanvasPanel.h"
#include "core/Library.h"
#include "core/Services.h"
#include "core/sim/SimEngine.h"

#include <wx/cursor.h>
#include <wx/msgdlg.h>
#include <wx/textdlg.h>

#include <algorithm>
#include <cmath>

namespace sf
{

CanvasEventHandler::CanvasEventHandler(CanvasPanel* panel) : m_panel(panel)
{
    m_rubberBand.reserve(8);
}

CircuitSheet* CanvasEventHandler::Sheet() const
{
    return m_panel == nullptr ? nullptr : m_panel->Sheet();
}

Point CanvasEventHandler::DeviceToCanvas(const wxMouseEvent& event) const
{
    return m_panel->DeviceToCanvas(Point(event.GetX(), event.GetY()));
}

Point CanvasEventHandler::SnappedPoint(const wxMouseEvent& event) const
{
    return m_panel->Snap(DeviceToCanvas(event), event.ShiftDown());
}

// -----------------------------------------------------------------------------
//  事务
// -----------------------------------------------------------------------------
void CanvasEventHandler::BeginTransaction(const wxString& name)
{
    // 已有未提交的事务就沿用（例如拉线是「两次点击」，两次点击属于同一手势）
    if (m_tx != nullptr) return;

    CircuitSheet* sheet = Sheet();
    if (sheet == nullptr) return;

    Services* s = m_panel->GetServices();
    m_tx = std::make_unique<ScopeTransaction>(*sheet, s ? s->undo : nullptr, s ? s->library : nullptr, name);
}

void CanvasEventHandler::CommitTransaction()
{
    if (m_tx != nullptr)
    {
        m_tx->Commit();
        m_tx.reset();
    }
}

void CanvasEventHandler::AbandonTransaction()
{
    if (m_tx != nullptr)
    {
        m_tx->Abort();
        m_tx.reset();
    }
}

void CanvasEventHandler::ResetTransientState()
{
    m_selectionRect = Rect(0, 0, 0, 0);
    m_eraserRect = Rect(0, 0, 0, 0);
    m_hasPreviewShape = false;
    m_rubberBand.clear();

    RenderOptions& opts = m_panel->Options();
    opts.selectionRect = nullptr;
    opts.eraserRect = nullptr;
    opts.rubberBand = nullptr;
    opts.previewShape = nullptr;
}

// -----------------------------------------------------------------------------
//  悬停
// -----------------------------------------------------------------------------
void CanvasEventHandler::RefreshHover(const Point& canvasPos)
{
    CircuitSheet* sheet = Sheet();
    if (sheet == nullptr) return;

    int pin = -1;
    ComponentInstance* comp = sheet->HitComponent(canvasPos, 3, &pin);
    Wire* wire = (comp != nullptr) ? nullptr : sheet->HitWire(canvasPos, 4);

    m_panel->SetHover(comp != nullptr ? comp->id : -1, comp != nullptr ? pin : -1,
                      wire != nullptr ? wire->id : -1);
}

// -----------------------------------------------------------------------------
//  放置模式
// -----------------------------------------------------------------------------
void CanvasEventHandler::BeginPlacement(const wxString& defType)
{
    Services* s = m_panel->GetServices();
    const ComponentDef* def = (s && s->library) ? s->library->Find(defType) : nullptr;
    if (def == nullptr)
    {
        if (s) s->Log("找不到元件类型：" + defType);
        return;
    }

    m_placeType = defType;
    m_placementPreview = std::make_unique<ComponentInstance>();
    m_placementPreview->id = -1;  // 预览不占用真实 id
    m_placementPreview->Rebuild(*def);

    m_panel->Tools().SetTool(ToolType::Component);
    m_panel->Options().previewComponent = m_placementPreview.get();
    m_panel->SetCursor(wxCursor(wxCURSOR_CROSS));
    m_panel->Refresh(false);

    if (s) s->Log(wxString::Format("放置模式：%s（单击落点，右键或 Esc 退出）", def->name));
}

void CanvasEventHandler::CancelPlacement()
{
    m_placeType.Clear();
    m_placementPreview.reset();
    m_panel->Options().previewComponent = nullptr;
    m_panel->Tools().SetTool(ToolType::Select);
    m_panel->SetCursor(wxCursor(wxCURSOR_ARROW));
    m_panel->Refresh(false);
}

void CanvasEventHandler::UpdatePlacementPreview(const Point& canvasPos)
{
    if (m_placementPreview == nullptr) return;

    // 让鼠标落在元件中心，手感更自然
    m_placementPreview->pos =
        Point(canvasPos.x - m_placementPreview->boxSize.x / 2, canvasPos.y - m_placementPreview->boxSize.y / 2);
    m_panel->Refresh(false);
}

// -----------------------------------------------------------------------------
//  左键按下
// -----------------------------------------------------------------------------
void CanvasEventHandler::OnLeftDown(wxMouseEvent& event)
{
    m_panel->SetFocus();

    // 每次按下都重新判定「这一下要不要翻转电平」
    m_clickToggleCompId = -1;

    const Point device(event.GetX(), event.GetY());
    const Point canvasPos = m_panel->DeviceToCanvas(device);
    const Point snapped = m_panel->Snap(canvasPos, event.ShiftDown());

    if (m_spacePanning)
    {
        m_leftPanning = true;
        m_panAnchorDevice = device;
        m_panel->SetCursor(wxCursor(wxCURSOR_HAND));
        if (!m_panel->HasCapture()) m_panel->CaptureMouse();
        return;
    }

    if (!m_panel->HasCapture())
    {
        // 文本工具会弹模态对话框，元件工具不需要拖拽，这两种都不抓鼠标
        const ToolType tool = m_panel->Tools().Tool();
        if (tool != ToolType::Text && tool != ToolType::Component) m_panel->CaptureMouse();
    }

    switch (m_panel->Tools().Tool())
    {
    case ToolType::Drag:
        m_leftPanning = true;
        m_panAnchorDevice = device;
        m_panel->SetCursor(wxCursor(wxCURSOR_HAND));
        break;

    case ToolType::Select:    HandleSelectDown(snapped, event.ShiftDown()); break;
    case ToolType::Wire:      HandleWireDown(snapped); break;
    case ToolType::Component: HandleComponentDown(snapped); break;
    case ToolType::Eraser:    HandleEraseDown(snapped); break;
    case ToolType::Text:      HandleTextDown(snapped); break;
    case ToolType::Drawing:   HandleDrawingDown(snapped); break;
    }
}

void CanvasEventHandler::OnLeftUp(wxMouseEvent& event)
{
    CircuitSheet* sheet = Sheet();

    if (m_panel->HasCapture()) m_panel->ReleaseMouse();

    if (m_leftPanning)
    {
        m_leftPanning = false;
        m_panel->SetCursor(wxCursor(m_panel->Tools().Tool() == ToolType::Drag ? wxCURSOR_HAND : wxCURSOR_ARROW));
        return;
    }

    if (sheet == nullptr) return;

    const Point snapped = SnappedPoint(event);
    const Point rawCanvas = DeviceToCanvas(event);

    switch (m_panel->Tools().Tool())
    {
    case ToolType::Select:
        // 「单击」而不是拖动：命中输入端口就翻转电平。
        // 这条必须排在拖动提交前面 —— 翻转电平不进撤销栈，所以按下的那笔事务要作废。
        if (m_clickToggleCompId >= 0)
        {
            const int tdx = rawCanvas.x - m_pressCanvasPos.x;
            const int tdy = rawCanvas.y - m_pressCanvasPos.y;

            if (tdx * tdx + tdy * tdy <= 9)  // 画布坐标 3px 以内算单击
            {
                const int id = m_clickToggleCompId;
                m_clickToggleCompId = -1;

                m_dragSelecting = false;
                m_movingComponents.clear();
                m_movingWires.clear();
                m_panel->Tools().SetSelect(SelectState::Idle);
                AbandonTransaction();

                if (Services* svc = m_panel->GetServices()) svc->SimTogglePin(id);

                m_panel->NotifySelectionChanged();
                return;
            }
            m_clickToggleCompId = -1;
        }

        if (m_dragSelecting && m_panel->Tools().Select() == SelectState::DragSelect)
        {
            // 提交移动：让挂在被移动元件上的导线跟过去
            for (auto& entry : m_movingComponents)
            {
                if (entry.first != nullptr) sheet->SyncWireEndpoints(entry.first->id);
            }
            m_dragSelecting = false;
            m_movingComponents.clear();
            m_movingWires.clear();
            m_panel->Tools().SetSelect(SelectState::Idle);
            CommitTransaction();
            m_panel->NotifyContentChanged();
        }
        else if (m_panel->Tools().Select() == SelectState::RectangleSelect)
        {
            const Rect r = NormalizeRect(m_dragStartCanvas, snapped);
            sheet->SelectInRect(r, event.ShiftDown());
            m_panel->Options().selectionRect = nullptr;
            m_panel->Tools().SetSelect(SelectState::Idle);
            CommitTransaction();
            m_panel->NotifySelectionChanged();
        }
        break;

    case ToolType::Wire:
        // 支持「拖一下就完成」：按下后移动超过阈值再松开即完成这根线
        if (m_panel->Tools().Wire() == WireState::WireDrawing && m_hasPendingStart)
        {
            const int dx = rawCanvas.x - m_pendingStart.pos.x;
            const int dy = rawCanvas.y - m_pendingStart.pos.y;
            if (dx * dx + dy * dy > 16 * 16) HandleWireDown(snapped);
        }
        break;

    case ToolType::Drawing:
        HandleDrawingUp(snapped);
        break;

    case ToolType::Eraser:
        if (m_panel->Tools().Eraser() == EraserState::RectangleEraser)
        {
            const Rect r = NormalizeRect(m_dragStartCanvas, snapped);
            m_panel->Options().eraserRect = nullptr;
            m_panel->Tools().SetEraser(EraserState::Idle);

            // 先收集 id 再删，避免级联删除把指针弄成悬空
            std::vector<int> compIds, wireIds;
            for (ComponentInstance* c : sheet->ComponentsIn(r)) compIds.push_back(c->id);
            for (Wire* w : sheet->WiresIn(r)) wireIds.push_back(w->id);

            if (r.width > 2 || r.height > 2)
            {
                for (int id : compIds) sheet->RemoveComponent(id);
                for (int id : wireIds) sheet->RemoveWire(id);
                CommitTransaction();
                m_panel->NotifyContentChanged();
                m_panel->NotifySelectionChanged();
            }
            else
                CommitTransaction();
        }
        break;

    default:
        break;
    }

    m_rubberBanding = false;
    m_panel->Refresh(false);
}

// -----------------------------------------------------------------------------
//  各工具的左键按下语义
// -----------------------------------------------------------------------------
void CanvasEventHandler::HandleSelectDown(const Point& canvasPos, bool shift)
{
    CircuitSheet* sheet = Sheet();
    if (sheet == nullptr) return;

    // ---- 1) 导线控制点：改拐点 ----
    int wireId = -1;
    const int cp = sheet->HitControlPoint(canvasPos, 6, &wireId);
    if (cp >= 0 && wireId >= 0)
    {
        if (Wire* w = sheet->FindWire(wireId))
        {
            BeginTransaction("拖动导线拐点");
            m_editingWireId = wireId;
            m_editingCpIndex = cp;
            m_editOriginalPts = w->pts;
            m_editOriginalLeft = w->left;
            m_editOriginalRight = w->right;

            if (!w->selected)
            {
                sheet->ClearSelection();
                w->selected = true;
            }
            m_panel->Tools().SetWire(WireState::WireEditing);
            m_panel->Tools().SetSelect(SelectState::DragSelect);
            m_panel->SetCursor(wxCursor(wxCURSOR_SIZING));
            m_panel->NotifySelectionChanged();
            return;
        }
    }

    // ---- 2) 元件或导线本体 ----
    int pin = -1;
    ComponentInstance* comp = sheet->HitComponent(canvasPos, 3, &pin);
    Wire* wire = (comp != nullptr) ? nullptr : sheet->HitWire(canvasPos, 4);

    if (comp != nullptr || wire != nullptr)
    {
        const bool alreadySelected = (comp != nullptr && comp->selected) || (wire != nullptr && wire->selected);

        if (shift)
        {
            if (comp != nullptr) comp->selected = !comp->selected;
            if (wire != nullptr) wire->selected = !wire->selected;
        }
        else if (!alreadySelected)
        {
            sheet->ClearSelection();
            if (comp != nullptr) comp->selected = true;
            if (wire != nullptr) wire->selected = true;
        }

        // 记下待翻转的元件：松开时若几乎没移动，就把这一下当成「单击」而不是拖动。
        // 只认输入端口 —— 常量、指示灯、输出端口都不该被点着翻转。
        if (comp != nullptr && !shift)
        {
            Services* svc = m_panel->GetServices();
            if (svc != nullptr && svc->sim != nullptr && svc->sim->IsToggleable(comp->id))
                m_clickToggleCompId = comp->id;
        }
        m_pressCanvasPos = canvasPos;

        m_panel->NotifySelectionChanged();

        BeginTransaction("移动");
        m_dragSelecting = true;
        m_dragStartCanvas = canvasPos;
        m_movingComponents.clear();
        m_movingWires.clear();

        for (ComponentInstance* c : sheet->SelectedComponents()) m_movingComponents.emplace_back(c, c->pos);
        for (Wire* w : sheet->SelectedWires())
        {
            WireOriginal o;
            o.wire = w;
            o.pts = w->pts;
            o.left = w->left;
            o.right = w->right;
            m_movingWires.push_back(std::move(o));
        }

        m_panel->Tools().SetSelect(SelectState::DragSelect);
        return;
    }

    // ---- 3) 空白处：框选 ----
    if (!shift)
    {
        sheet->ClearSelection();
        m_panel->NotifySelectionChanged();
    }

    BeginTransaction("框选");
    m_dragSelecting = false;
    m_dragStartCanvas = canvasPos;
    m_selectionRect = Rect(canvasPos, canvasPos);
    m_panel->Options().selectionRect = &m_selectionRect;
    m_panel->Tools().SetSelect(SelectState::RectangleSelect);
    m_panel->Refresh(false);
}

void CanvasEventHandler::HandleWireDown(const Point& canvasPos)
{
    CircuitSheet* sheet = Sheet();
    if (sheet == nullptr) return;

    Services* s = m_panel->GetServices();
    const int snapRadius = 12;

    auto resolveEndpoint = [&](const Point& p) -> WireEndpoint
    {
        WireEndpoint ep;
        if (sheet->SnapToPin(p, snapRadius, ep)) return ep;

        ep.compId = -1;
        ep.pinIndex = -1;
        ep.pos = p;
        return ep;
    };

    // 第二次点击：完成这根线
    if (m_hasPendingStart)
    {
        const WireEndpoint end = resolveEndpoint(canvasPos);

        if (end.pos == m_pendingStart.pos)
        {
            // 原地点击两次视为取消
            m_hasPendingStart = false;
            m_rubberBanding = false;
            m_panel->Options().rubberBand = nullptr;
            m_panel->Tools().SetWire(WireState::Idle);
            CommitTransaction();
            m_panel->Refresh(false);
            return;
        }

        CircuitSheet* target = sheet;
        Wire* created = target->AddWire(m_pendingStart, end);
        if (created != nullptr)
        {
            if (s) s->Log(wxString::Format("已连接导线 #%d（%d 段）", created->id, created->SegmentCount()));
        }

        m_hasPendingStart = false;
        m_rubberBanding = false;
        m_panel->Options().rubberBand = nullptr;
        m_panel->Tools().SetWire(WireState::Idle);
        CommitTransaction();
        m_panel->NotifyContentChanged();
        m_panel->Refresh(false);
        return;
    }

    // 第一次点击：开始
    BeginTransaction("拉一根导线");
    m_pendingStart = resolveEndpoint(canvasPos);
    m_hasPendingStart = true;
    m_rubberBanding = true;
    m_rubberBand.clear();
    m_rubberBand.push_back(m_pendingStart.pos);
    m_rubberBand.push_back(m_pendingStart.pos);
    m_panel->Options().rubberBand = &m_rubberBand;
    m_panel->Tools().SetWire(WireState::WireDrawing);
    m_panel->Refresh(false);
}

void CanvasEventHandler::HandleComponentDown(const Point& canvasPos)
{
    CircuitSheet* sheet = Sheet();
    Services* s = m_panel->GetServices();
    if (sheet == nullptr || s == nullptr || s->library == nullptr) return;

    if (m_placeType.IsEmpty())
    {
        s->Log("请先在左侧元件库中选择一个元件。");
        return;
    }

    const ComponentDef* def = s->library->Find(m_placeType);
    if (def == nullptr) return;

    BeginTransaction("放置元件");
    const Point origin(canvasPos.x - def->size.x / 2, canvasPos.y - def->size.y / 2);
    ComponentInstance* comp = sheet->AddComponent(*def, SnapPoint(origin, grid::kFine));
    CommitTransaction();

    if (comp != nullptr) s->Log(wxString::Format("放置 %s（%s）", def->name, comp->label));

    m_panel->NotifyContentChanged();
}

void CanvasEventHandler::HandleEraseDown(const Point& canvasPos)
{
    CircuitSheet* sheet = Sheet();
    if (sheet == nullptr) return;

    int pin = -1;
    ComponentInstance* comp = sheet->HitComponent(canvasPos, 3, &pin);
    Wire* wire = (comp != nullptr) ? nullptr : sheet->HitWire(canvasPos, 4);

    if (comp != nullptr || wire != nullptr)
    {
        BeginTransaction("删除");
        if (comp != nullptr) sheet->RemoveComponent(comp->id);
        if (wire != nullptr) sheet->RemoveWire(wire->id);
        CommitTransaction();
        m_panel->NotifyContentChanged();
        m_panel->NotifySelectionChanged();
        return;
    }

    BeginTransaction("框擦");
    m_dragStartCanvas = canvasPos;
    m_eraserRect = Rect(canvasPos, canvasPos);
    m_panel->Options().eraserRect = &m_eraserRect;
    m_panel->Tools().SetEraser(EraserState::RectangleEraser);
}

void CanvasEventHandler::HandleTextDown(const Point& canvasPos)
{
    CircuitSheet* sheet = Sheet();
    if (sheet == nullptr) return;

    int pin = -1;
    ComponentInstance* comp = sheet->HitComponent(canvasPos, 3, &pin);

    if (comp != nullptr)
    {
        m_panel->Tools().SetText(TextState::ComponentLabelEditing);
        wxTextEntryDialog dlg(m_panel, "元件标签：", "编辑标签", comp->label);
        if (dlg.ShowModal() == wxID_OK)
        {
            BeginTransaction("修改标签");
            comp->label = dlg.GetValue();
            CommitTransaction();
            m_panel->NotifyContentChanged();
        }
        m_panel->Tools().SetText(TextState::Idle);
        return;
    }

    m_panel->Tools().SetText(TextState::CanvasTextEditing);
    wxTextEntryDialog dlg(m_panel, "注释内容：", "添加注释", "注释");
    if (dlg.ShowModal() == wxID_OK && !dlg.GetValue().IsEmpty())
    {
        BeginTransaction("添加注释");
        sheet->annotations.push_back(MakeText(canvasPos, dlg.GetValue(), 12, palette::kText, false));
        CommitTransaction();
        m_panel->NotifyContentChanged();
    }
    m_panel->Tools().SetText(TextState::Idle);
}

void CanvasEventHandler::HandleDrawingDown(const Point& canvasPos)
{
    m_drawingShape = true;
    m_drawStart = canvasPos;

    switch (m_drawKind)
    {
    case DrawKind::Line:      m_panel->Tools().SetDrawing(DrawingState::DrawingLine); break;
    case DrawKind::Circle:    m_panel->Tools().SetDrawing(DrawingState::DrawingCircle); break;
    default:                  m_panel->Tools().SetDrawing(DrawingState::DrawingRectangle); break;
    }
}

void CanvasEventHandler::HandleDrawingUp(const Point& canvasPos)
{
    if (!m_drawingShape)
    {
        m_panel->Tools().SetDrawing(DrawingState::Idle);
        return;
    }

    CircuitSheet* sheet = Sheet();
    m_drawingShape = false;
    m_hasPreviewShape = false;
    m_panel->Options().previewShape = nullptr;
    m_panel->Tools().SetDrawing(DrawingState::Idle);

    if (sheet == nullptr) return;

    const Rect r = NormalizeRect(m_drawStart, canvasPos);
    if (r.width < 2 && r.height < 2) return;

    Shape shape;
    switch (m_drawKind)
    {
    case DrawKind::Line:   shape = MakeLine(m_drawStart, canvasPos); break;
    case DrawKind::Circle: shape = MakeCircle(Point(r.x + r.width / 2, r.y + r.height / 2), std::max(r.width, r.height) / 2); break;
    default:               shape = MakeRect(r); break;
    }

    BeginTransaction("绘制图形");
    sheet->annotations.push_back(shape);
    CommitTransaction();
    m_panel->NotifyContentChanged();
}

// -----------------------------------------------------------------------------
//  鼠标移动
// -----------------------------------------------------------------------------
void CanvasEventHandler::OnMotion(wxMouseEvent& event)
{
    const Point device(event.GetX(), event.GetY());
    const Point canvasPos = m_panel->DeviceToCanvas(device);
    const Point snapped = m_panel->Snap(canvasPos, event.ShiftDown());

    // ---- 平移画布（不记入撤销栈）----
    if (m_leftPanning || m_middlePanning)
    {
        const Point delta(device.x - m_panAnchorDevice.x, device.y - m_panAnchorDevice.y);
        m_panAnchorDevice = device;
        m_panel->PanBy(delta);
        return;
    }

    CircuitSheet* sheet = Sheet();

    // ---- 拖动选中物 ----
    if (m_dragSelecting && m_panel->Tools().Select() == SelectState::DragSelect)
    {
        if (sheet != nullptr)
        {
            const Point delta(snapped.x - m_dragStartCanvas.x, snapped.y - m_dragStartCanvas.y);

            for (auto& entry : m_movingComponents)
            {
                if (entry.first == nullptr) continue;
                entry.first->pos = Point(entry.second.x + delta.x, entry.second.y + delta.y);
            }

            for (const WireOriginal& o : m_movingWires)
            {
                if (o.wire == nullptr) continue;
                for (size_t i = 0; i < o.wire->pts.size() && i < o.pts.size(); ++i)
                    o.wire->pts[i].pos = Point(o.pts[i].pos.x + delta.x, o.pts[i].pos.y + delta.y);
                o.wire->left.pos = Point(o.left.pos.x + delta.x, o.left.pos.y + delta.y);
                o.wire->right.pos = Point(o.right.pos.x + delta.x, o.right.pos.y + delta.y);
            }

            // 让挂在移动元件上的导线跟着走
            for (auto& entry : m_movingComponents)
            {
                if (entry.first != nullptr) sheet->SyncWireEndpoints(entry.first->id);
            }

            m_panel->Refresh(false);
        }
        return;
    }

    // ---- 拖动导线拐点 ----
    if (m_panel->Tools().Wire() == WireState::WireEditing && sheet != nullptr)
    {
        if (Wire* w = sheet->FindWire(m_editingWireId))
        {
            w->MovePoint(m_editingCpIndex, snapped);
            m_panel->Refresh(false);
        }
        return;
    }

    // ---- 框选 / 框擦 ----
    if (m_panel->Tools().Select() == SelectState::RectangleSelect)
    {
        m_selectionRect = NormalizeRect(m_dragStartCanvas, snapped);
        m_panel->Refresh(false);
        return;
    }
    if (m_panel->Tools().Eraser() == EraserState::RectangleEraser)
    {
        m_eraserRect = NormalizeRect(m_dragStartCanvas, snapped);
        m_panel->Refresh(false);
        return;
    }

    // ---- 拉线橡皮筋 ----
    if (m_panel->Tools().Wire() == WireState::WireDrawing && m_hasPendingStart)
    {
        WireEndpoint probe;
        const bool snappedToPin = sheet != nullptr && sheet->SnapToPin(snapped, 12, probe);
        const Point endPos = snappedToPin ? probe.pos : snapped;

        m_rubberBand.clear();
        m_rubberBand.push_back(m_pendingStart.pos);
        if (m_pendingStart.pos.x != endPos.x && m_pendingStart.pos.y != endPos.y)
            m_rubberBand.push_back(Point((m_pendingStart.pos.x + endPos.x) / 2, m_pendingStart.pos.y));
        m_rubberBand.push_back(endPos);

        m_panel->Options().rubberBand = &m_rubberBand;
        m_panel->Refresh(false);
        return;
    }

    // ---- 绘图预览 ----
    if (m_drawingShape)
    {
        const Rect r = NormalizeRect(m_drawStart, snapped);
        switch (m_drawKind)
        {
        case DrawKind::Line:   m_previewShape = MakeLine(m_drawStart, snapped); break;
        case DrawKind::Circle: m_previewShape = MakeCircle(Point(r.x + r.width / 2, r.y + r.height / 2), std::max(r.width, r.height) / 2); break;
        default:               m_previewShape = MakeRect(r); break;
        }
        m_hasPreviewShape = true;
        m_panel->Options().previewShape = &m_previewShape;
        m_panel->Refresh(false);
        return;
    }

    // ---- 放置预览 ----
    if (m_panel->Tools().Tool() == ToolType::Component && m_placementPreview != nullptr)
    {
        UpdatePlacementPreview(snapped);
        RefreshHover(canvasPos);
        return;
    }

    RefreshHover(canvasPos);
}

void CanvasEventHandler::OnLeave(wxMouseEvent& WXUNUSED(event))
{
    // 手势进行中不要清悬停，否则拖动时会闪
    if (IsGestureActive()) return;
    m_panel->ClearHover();
}

void CanvasEventHandler::OnCaptureLost()
{
    m_leftPanning = false;
    m_middlePanning = false;
}

// -----------------------------------------------------------------------------
//  右键 / 中键 / 双击
// -----------------------------------------------------------------------------
void CanvasEventHandler::OnRightDown(wxMouseEvent& event)
{
    m_panel->SetFocus();

    const Point canvasPos = DeviceToCanvas(event);

    if (m_panel->Tools().Tool() == ToolType::Component || m_hasPendingStart || IsGestureActive())
    {
        CancelGesture();
        return;
    }

    // 右键命中元件时直接弹出元件菜单
    CircuitSheet* sheet = Sheet();
    if (sheet == nullptr) return;

    int pin = -1;
    if (ComponentInstance* comp = sheet->HitComponent(canvasPos, 3, &pin))
    {
        if (!comp->selected)
        {
            sheet->ClearSelection();
            comp->selected = true;
            m_panel->NotifySelectionChanged();
        }
    }
    else if (Wire* w = sheet->HitWire(canvasPos, 4))
    {
        if (!w->selected)
        {
            sheet->ClearSelection();
            w->selected = true;
            m_panel->NotifySelectionChanged();
        }
    }
    else
    {
        sheet->ClearSelection();
        m_panel->NotifySelectionChanged();
    }

    wxUnusedVar(event);  // 菜单由 MainFrame 绑定，这里只负责设置选中集
}

void CanvasEventHandler::OnRightUp(wxMouseEvent& WXUNUSED(event)) {}

void CanvasEventHandler::OnMiddleDown(wxMouseEvent& event)
{
    m_panel->SetFocus();
    m_middlePanning = true;
    m_panAnchorDevice = Point(event.GetX(), event.GetY());
    m_panel->SetCursor(wxCursor(wxCURSOR_HAND));
    if (!m_panel->HasCapture()) m_panel->CaptureMouse();
}

void CanvasEventHandler::OnMiddleUp(wxMouseEvent& WXUNUSED(event))
{
    m_middlePanning = false;
    if (m_panel->HasCapture()) m_panel->ReleaseMouse();
    m_panel->SetCursor(wxCursor(wxCURSOR_ARROW));
}

void CanvasEventHandler::OnLeftDoubleClick(wxMouseEvent& event)
{
    CircuitSheet* sheet = Sheet();
    if (sheet == nullptr) return;

    const Point canvasPos = DeviceToCanvas(event);

    int pin = -1;
    ComponentInstance* comp = sheet->HitComponent(canvasPos, 3, &pin);
    if (comp == nullptr) return;

    m_panel->Tools().SetText(TextState::ComponentLabelEditing);
    wxTextEntryDialog dlg(m_panel, "元件标签：", "编辑标签", comp->label);
    if (dlg.ShowModal() == wxID_OK)
    {
        BeginTransaction("修改标签");
        comp->label = dlg.GetValue();
        CommitTransaction();
        m_panel->NotifyContentChanged();
    }
    m_panel->Tools().SetText(TextState::Idle);
}

// -----------------------------------------------------------------------------
//  键盘
// -----------------------------------------------------------------------------
bool CanvasEventHandler::OnKeyDown(wxKeyEvent& event)
{
    const int code = event.GetKeyCode();

    if (code == WXK_ESCAPE)
    {
        CancelGesture();
        return true;
    }

    if (code == WXK_DELETE || code == WXK_BACK)
    {
        DeleteSelection();
        return true;
    }

    if (event.ControlDown())
    {
        switch (code)
        {
        case 'A':
        case 'a':
            SelectAll();
            return true;
        case 'R':
        case 'r':
            RotateSelection(event.ShiftDown() ? 3 : 1);
            return true;
        case 'M':
        case 'm':
            MirrorSelection();
            return true;
        default:
            break;
        }
        return false;
    }

    const int step = event.ShiftDown() ? 1 : grid::kFine;
    switch (code)
    {
    case WXK_LEFT:  NudgeSelection(Point(-step, 0)); return true;
    case WXK_RIGHT: NudgeSelection(Point(step, 0)); return true;
    case WXK_UP:    NudgeSelection(Point(0, -step)); return true;
    case WXK_DOWN:  NudgeSelection(Point(0, step)); return true;
    default: break;
    }

    // 单字母切换工具
    switch (code)
    {
    case 'S': case 's': SwitchToolFromShortcut(ToolType::Select); return true;
    case 'T': case 't': SwitchToolFromShortcut(ToolType::Text); return true;
    case 'C': case 'c': SwitchToolFromShortcut(ToolType::Component); return true;
    case 'W': case 'w': SwitchToolFromShortcut(ToolType::Wire); return true;
    case 'D': case 'd': SwitchToolFromShortcut(ToolType::Drawing); return true;
    case 'E': case 'e': SwitchToolFromShortcut(ToolType::Eraser); return true;
    case 'H': case 'h': SwitchToolFromShortcut(ToolType::Drag); return true;
    default: break;
    }

    return false;
}

void CanvasEventHandler::SwitchToolFromShortcut(ToolType tool)
{
    if (tool != ToolType::Component) CancelPlacement();

    m_panel->Tools().SetTool(tool);
    m_panel->SetCursor(wxCursor(tool == ToolType::Drag ? wxCURSOR_HAND : wxCURSOR_ARROW));
    m_panel->Refresh(false);
}

// -----------------------------------------------------------------------------
//  编辑命令
// -----------------------------------------------------------------------------
void CanvasEventHandler::DeleteSelection()
{
    CircuitSheet* sheet = Sheet();
    if (sheet == nullptr) return;

    std::vector<int> compIds, wireIds;
    for (ComponentInstance* c : sheet->SelectedComponents()) compIds.push_back(c->id);
    for (Wire* w : sheet->SelectedWires()) wireIds.push_back(w->id);
    if (compIds.empty() && wireIds.empty()) return;

    BeginTransaction("删除");
    for (int id : compIds) sheet->RemoveComponent(id);
    for (int id : wireIds) sheet->RemoveWire(id);
    CommitTransaction();

    m_panel->NotifyContentChanged();
    m_panel->NotifySelectionChanged();
}

void CanvasEventHandler::SelectAll()
{
    CircuitSheet* sheet = Sheet();
    if (sheet == nullptr) return;

    for (auto& c : sheet->components) c->selected = true;
    for (auto& w : sheet->wires) w->selected = true;

    m_panel->NotifySelectionChanged();
}

void CanvasEventHandler::ClearSelection()
{
    CircuitSheet* sheet = Sheet();
    if (sheet == nullptr) return;

    sheet->ClearSelection();
    m_panel->NotifySelectionChanged();
}

void CanvasEventHandler::RotateSelection(int quarters)
{
    CircuitSheet* sheet = Sheet();
    if (sheet == nullptr) return;

    const auto comps = sheet->SelectedComponents();
    if (comps.empty()) return;

    BeginTransaction("旋转元件");
    for (ComponentInstance* c : comps)
    {
        if (quarters >= 0)
        {
            for (int i = 0; i < quarters; ++i) c->RotateCw();
        }
        else
        {
            for (int i = 0; i < -quarters; ++i) c->RotateCcw();
        }
    }
    for (ComponentInstance* c : comps) sheet->SyncWireEndpoints(c->id);
    CommitTransaction();

    m_panel->NotifyContentChanged();
}

void CanvasEventHandler::MirrorSelection()
{
    CircuitSheet* sheet = Sheet();
    if (sheet == nullptr) return;

    const auto comps = sheet->SelectedComponents();
    if (comps.empty()) return;

    BeginTransaction("镜像元件");
    for (ComponentInstance* c : comps) c->ToggleMirror();
    for (ComponentInstance* c : comps) sheet->SyncWireEndpoints(c->id);
    CommitTransaction();

    m_panel->NotifyContentChanged();
}

void CanvasEventHandler::NudgeSelection(const Point& delta)
{
    CircuitSheet* sheet = Sheet();
    if (sheet == nullptr) return;

    const auto comps = sheet->SelectedComponents();
    const auto wires = sheet->SelectedWires();
    if (comps.empty() && wires.empty()) return;

    BeginTransaction("移动");
    for (ComponentInstance* c : comps) c->pos = Point(c->pos.x + delta.x, c->pos.y + delta.y);
    for (Wire* w : wires) w->Move(delta);
    for (ComponentInstance* c : comps) sheet->SyncWireEndpoints(c->id);
    CommitTransaction();

    m_panel->NotifyContentChanged();
}

// -----------------------------------------------------------------------------
//  属性面板下发的修改
// -----------------------------------------------------------------------------
void CanvasEventHandler::ApplyComponentProperty(int compId, const wxString& prop, const wxString& value)
{
    CircuitSheet* sheet = Sheet();
    if (sheet == nullptr) return;

    ComponentInstance* comp = sheet->FindComponent(compId);
    if (comp == nullptr) return;

    BeginTransaction("修改属性");

    if (prop == "label")
        comp->label = value;
    else if (prop == "value")
        comp->value = value;
    else if (prop == "x")
    {
        long v = 0;
        if (value.ToLong(&v)) comp->pos.x = static_cast<int>(v);
    }
    else if (prop == "y")
    {
        long v = 0;
        if (value.ToLong(&v)) comp->pos.y = static_cast<int>(v);
    }
    else if (prop == "rotation")
    {
        long v = 0;
        if (value.ToLong(&v) && comp->def != nullptr)
        {
            comp->rotation = ((static_cast<int>(v) % 4) + 4) % 4;
            comp->Rebuild(*comp->def);
        }
    }
    else if (prop == "width")
    {
        long v = 0;
        if (value.ToLong(&v) && comp->def != nullptr)
        {
            int w = static_cast<int>(v);
            if (w < 1) w = 1;
            if (w > kMaxBusWidth) w = kMaxBusWidth;
            comp->width = w;
            comp->Rebuild(*comp->def);  // 引脚位宽跟着解析出来
        }
    }
    else if (prop == "mirrored")
    {
        const bool wanted = (value == "1" || value.IsSameAs("true", false));
        if (comp->mirrored != wanted) comp->ToggleMirror();
    }

    sheet->SyncWireEndpoints(compId);
    CommitTransaction();

    m_panel->NotifyContentChanged();
}

void CanvasEventHandler::ApplyWireProperty(int wireId, const wxString& prop, const wxString& value)
{
    CircuitSheet* sheet = Sheet();
    if (sheet == nullptr) return;

    Wire* wire = sheet->FindWire(wireId);
    if (wire == nullptr) return;

    if (prop != "signal") return;

    BeginTransaction("修改导线信号");
    wire->signal = LogicSignalFromName(value, wire->signal);
    CommitTransaction();

    m_panel->NotifyContentChanged();
}

// -----------------------------------------------------------------------------
//  定位 / 高亮
// -----------------------------------------------------------------------------
void CanvasEventHandler::FocusComponent(int compId)
{
    CircuitSheet* sheet = Sheet();
    if (sheet == nullptr) return;

    ComponentInstance* comp = sheet->FindComponent(compId);
    if (comp == nullptr) return;

    sheet->ClearSelection();
    comp->selected = true;

    m_panel->CenterOn(comp->Center());
    m_panel->NotifySelectionChanged();
}

void CanvasEventHandler::HighlightNet(const wxString& netName)
{
    CircuitSheet* sheet = Sheet();
    if (sheet == nullptr) return;

    const std::vector<int> ids = sheet->WireIdsOfNet(netName);
    if (ids.empty())
    {
        Services* s = m_panel->GetServices();
        if (s) s->Log("没有找到网络：" + netName);
        return;
    }

    sheet->ClearSelection();

    Rect unionRect(0, 0, 0, 0);
    bool first = true;

    for (int id : ids)
    {
        Wire* w = sheet->FindWire(id);
        if (w == nullptr) continue;
        w->selected = true;

        if (first)
        {
            unionRect = w->Bounds();
            first = false;
        }
        else
            unionRect = unionRect.Union(w->Bounds());
    }

    if (!first) m_panel->CenterOn(Point(unionRect.x + unionRect.width / 2, unionRect.y + unionRect.height / 2));

    m_panel->NotifySelectionChanged();
}

// -----------------------------------------------------------------------------
//  手势取消
// -----------------------------------------------------------------------------
bool CanvasEventHandler::IsGestureActive() const
{
    return m_dragSelecting || m_rubberBanding || m_drawingShape || m_leftPanning || m_middlePanning ||
           m_panel->Tools().IsBusy() || !m_placeType.IsEmpty() || m_tx != nullptr;
}

void CanvasEventHandler::CancelGesture()
{
    CircuitSheet* sheet = Sheet();

    // 事务直接丢弃，不写入撤销栈
    if (m_tx != nullptr)
    {
        m_tx->Abort();
        m_tx.reset();
    }

    // 还原被拖动的元件与导线
    if (sheet != nullptr)
    {
        for (auto& entry : m_movingComponents)
            if (entry.first != nullptr) entry.first->pos = entry.second;

        for (const WireOriginal& o : m_movingWires)
        {
            if (o.wire == nullptr) continue;
            o.wire->pts = o.pts;
            o.wire->left = o.left;
            o.wire->right = o.right;
        }

        if (m_editingWireId >= 0)
        {
            if (Wire* w = sheet->FindWire(m_editingWireId))
            {
                w->pts = m_editOriginalPts;
                w->left = m_editOriginalLeft;
                w->right = m_editOriginalRight;
                w->ReshapeFromEndpoints();
            }
        }
    }

    m_movingComponents.clear();
    m_movingWires.clear();
    m_editingWireId = -1;
    m_editingCpIndex = -1;
    m_dragSelecting = false;
    m_rubberBanding = false;
    m_hasPendingStart = false;
    m_drawingShape = false;
    m_leftPanning = false;
    m_middlePanning = false;

    ResetTransientState();
    CancelPlacement();
    m_panel->Tools().ResetToIdle();
    m_panel->SetCursor(wxCursor(wxCURSOR_ARROW));
    m_panel->Refresh(false);
}

}  // namespace sf
