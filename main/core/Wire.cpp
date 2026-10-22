#include "Wire.h"

#include <wx/brush.h>
#include <wx/graphics.h>
#include <wx/pen.h>

#include <algorithm>
#include <cmath>

namespace sf
{

// -----------------------------------------------------------------------------
//  控制点类型
// -----------------------------------------------------------------------------
wxString CPTypeName(CPType t)
{
    switch (t)
    {
    case CPType::Pin:    return "pin";
    case CPType::Bend:   return "bend";
    case CPType::Free:   return "free";
    case CPType::Branch: return "branch";
    }
    return "free";
}

CPType CPTypeFromName(const wxString& s, CPType def)
{
    if (s == "pin") return CPType::Pin;
    if (s == "bend") return CPType::Bend;
    if (s == "free") return CPType::Free;
    if (s == "branch") return CPType::Branch;
    return def;
}

// -----------------------------------------------------------------------------
//  四值逻辑
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
//  几何
// -----------------------------------------------------------------------------
Point Wire::Start() const
{
    if (!pts.empty()) return pts.front().pos;
    return left.pos;
}

Point Wire::End() const
{
    if (!pts.empty()) return pts.back().pos;
    return right.pos;
}

Rect Wire::Bounds() const
{
    if (pts.empty()) return NormalizeRect(left.pos, right.pos).Inflate(strokeWidth, strokeWidth);

    int minX = pts[0].pos.x, maxX = pts[0].pos.x;
    int minY = pts[0].pos.y, maxY = pts[0].pos.y;
    for (const ControlPoint& cp : pts)
    {
        minX = std::min(minX, cp.pos.x);
        maxX = std::max(maxX, cp.pos.x);
        minY = std::min(minY, cp.pos.y);
        maxY = std::max(maxY, cp.pos.y);
    }
    return Rect(minX, minY, maxX - minX, maxY - minY).Inflate(strokeWidth, strokeWidth);
}

int Wire::SegmentCount() const
{
    return pts.size() < 2 ? 0 : static_cast<int>(pts.size()) - 1;
}

int Wire::Length() const
{
    double total = 0.0;
    for (size_t i = 1; i < pts.size(); ++i)
    {
        const double dx = pts[i].pos.x - pts[i - 1].pos.x;
        const double dy = pts[i].pos.y - pts[i - 1].pos.y;
        total += std::sqrt(dx * dx + dy * dy);
    }
    return static_cast<int>(std::lround(total));
}

// -----------------------------------------------------------------------------
//  布线
// -----------------------------------------------------------------------------
std::vector<ControlPoint> Wire::Route(const Point& a, const Point& b)
{
    std::vector<ControlPoint> out;
    out.push_back(ControlPoint{a, CPType::Pin});

    // 同行或同列：直接两点直线
    if (a.x == b.x || a.y == b.y)
    {
        out.push_back(ControlPoint{b, CPType::Pin});
        return out;
    }

    const int midX = (a.x + b.x) / 2;
    out.push_back(ControlPoint{Point(midX, a.y), CPType::Free});
    out.push_back(ControlPoint{Point(midX, b.y), CPType::Free});
    out.push_back(ControlPoint{b, CPType::Pin});
    return out;
}

std::vector<ControlPoint> Wire::OrthogonalPath(const std::vector<Point>& waypoints)
{
    std::vector<ControlPoint> out;
    if (waypoints.empty()) return out;

    out.push_back(ControlPoint{waypoints.front(), CPType::Pin});

    for (size_t i = 1; i < waypoints.size(); ++i)
    {
        const Point a = waypoints[i - 1];
        const Point b = waypoints[i];

        // 两点不对齐时插一个拐角（先水平后垂直）
        if (a.x != b.x && a.y != b.y) out.push_back(ControlPoint{Point(b.x, a.y), CPType::Free});

        const bool last = (i + 1 == waypoints.size());
        out.push_back(ControlPoint{b, last ? CPType::Pin : CPType::Bend});
    }
    return out;
}

// -----------------------------------------------------------------------------
//  命中测试
// -----------------------------------------------------------------------------
bool Wire::HitTest(const Point& p, int tol) const
{
    if (pts.size() == 1) return NearlyEqual(pts[0].pos, p, tol);

    for (size_t i = 1; i < pts.size(); ++i)
        if (PointOnSegment(p, pts[i - 1].pos, pts[i].pos, tol)) return true;

    return false;
}

int Wire::HitTestControlPoint(const Point& p, int tol) const
{
    int best = -1;
    long long bestD2 = static_cast<long long>(tol) * tol;

    for (size_t i = 0; i < pts.size(); ++i)
    {
        const long long dx = pts[i].pos.x - p.x;
        const long long dy = pts[i].pos.y - p.y;
        const long long d2 = dx * dx + dy * dy;
        if (d2 <= bestD2)
        {
            bestD2 = d2;
            best = static_cast<int>(i);
        }
    }
    return best;
}

// -----------------------------------------------------------------------------
//  编辑
// -----------------------------------------------------------------------------
void Wire::Move(const Point& delta)
{
    for (ControlPoint& cp : pts) cp.pos = Point(cp.pos.x + delta.x, cp.pos.y + delta.y);
    left.pos = Point(left.pos.x + delta.x, left.pos.y + delta.y);
    right.pos = Point(right.pos.x + delta.x, right.pos.y + delta.y);
}

void Wire::MovePoint(int idx, const Point& p)
{
    if (idx < 0 || idx >= static_cast<int>(pts.size())) return;

    const Point old = pts[idx].pos;
    pts[idx].pos = p;

    // 拖动中间点即认定为用户手工拐点
    if (idx > 0 && idx + 1 < static_cast<int>(pts.size())) pts[idx].type = CPType::Bend;

    // 让相邻点跟着走，保持横平竖直
    if (idx > 0)
    {
        if (pts[idx - 1].pos.x == old.x) pts[idx - 1].pos.x = p.x;
        if (pts[idx - 1].pos.y == old.y) pts[idx - 1].pos.y = p.y;
    }
    if (idx + 1 < static_cast<int>(pts.size()))
    {
        if (pts[idx + 1].pos.x == old.x) pts[idx + 1].pos.x = p.x;
        if (pts[idx + 1].pos.y == old.y) pts[idx + 1].pos.y = p.y;
    }

    if (idx == 0) left.pos = p;
    if (idx + 1 == static_cast<int>(pts.size())) right.pos = p;
}

void Wire::SetLeftEndpoint(const WireEndpoint& e)
{
    left = e;
    if (pts.empty())
        pts = Route(left.pos, right.pos);
    else
    {
        pts.front().pos = left.pos;
        pts.front().type = CPType::Pin;
    }
    ReshapeFromEndpoints();
}

void Wire::SetRightEndpoint(const WireEndpoint& e)
{
    right = e;
    if (pts.empty())
        pts = Route(left.pos, right.pos);
    else
    {
        pts.back().pos = right.pos;
        pts.back().type = CPType::Pin;
    }
    ReshapeFromEndpoints();
}

void Wire::ReshapeFromEndpoints()
{
    std::vector<Point> wp;
    wp.push_back(left.pos);

    // 保留用户手工拖出来的拐点
    for (const ControlPoint& cp : pts)
        if (cp.type == CPType::Bend) wp.push_back(cp.pos);

    wp.push_back(right.pos);

    pts = OrthogonalPath(wp);
    Simplify();
}

void Wire::Simplify()
{
    if (pts.empty()) return;

    // 1) 去相邻重复点
    std::vector<ControlPoint> compact;
    for (const ControlPoint& cp : pts)
    {
        if (!compact.empty() && NearlyEqual(compact.back().pos, cp.pos, 0)) continue;
        compact.push_back(cp);
    }

    // 2) 去共线冗余中间点（手工拐点不删）
    if (compact.size() >= 3)
    {
        std::vector<ControlPoint> res;
        res.push_back(compact.front());
        for (size_t i = 1; i + 1 < compact.size(); ++i)
        {
            const Point& a = res.back().pos;
            const Point& b = compact[i].pos;
            const Point& c = compact[i + 1].pos;

            const bool collinear = (a.x == b.x && b.x == c.x) || (a.y == b.y && b.y == c.y);
            if (collinear && compact[i].type != CPType::Bend) continue;
            res.push_back(compact[i]);
        }
        res.push_back(compact.back());
        compact.swap(res);
    }

    pts.swap(compact);
}

// -----------------------------------------------------------------------------
//  绘制
// -----------------------------------------------------------------------------
void Wire::Draw(wxGraphicsContext* gc, bool drawHandles, int handleRadius) const
{
    DrawStyled(gc, hovered, selected, drawHandles, handleRadius);
}

void Wire::DrawStyled(wxGraphicsContext* gc, bool isHovered, bool isSelected, bool drawHandles,
                      int handleRadius) const
{
    DrawStyled(gc, isHovered, isSelected, drawHandles, handleRadius, signal, strokeWidth);
}

void Wire::DrawStyled(wxGraphicsContext* gc, bool isHovered, bool isSelected, bool drawHandles,
                      int handleRadius, LogicSignal effectiveSignal, int penWidth) const
{
    if (gc == nullptr || pts.size() < 2) return;

    // 逻辑电平有意义时用「电平色」，未驱动（Z）时用常规导线色
    wxColour color =
        (effectiveSignal == LogicSignal::Z) ? palette::kWireIdle : LogicSignalColour(effectiveSignal);
    if (isHovered && !isSelected) color = palette::kWireHover;
    if (isSelected) color = palette::kWireSelect;

    wxPen pen(color, std::max(1, penWidth));
    pen.SetCap(wxCAP_ROUND);
    pen.SetJoin(wxJOIN_ROUND);
    gc->SetPen(pen);
    gc->SetBrush(*wxTRANSPARENT_BRUSH);

    wxGraphicsPath path = gc->CreatePath();
    path.MoveToPoint(pts[0].pos.x, pts[0].pos.y);
    for (size_t i = 1; i < pts.size(); ++i) path.AddLineToPoint(pts[i].pos.x, pts[i].pos.y);
    gc->StrokePath(path);

    // 分支点画小实心圆
    for (const ControlPoint& cp : pts)
    {
        if (cp.type != CPType::Branch) continue;
        gc->SetBrush(wxBrush(color));
        gc->DrawEllipse(cp.pos.x - 3, cp.pos.y - 3, 6, 6);
    }

    // 控制手柄
    if (drawHandles)
    {
        gc->SetBrush(wxBrush(*wxWHITE));
        gc->SetPen(wxPen(palette::kSelect, 1));
        for (const ControlPoint& cp : pts)
            gc->DrawEllipse(cp.pos.x - handleRadius, cp.pos.y - handleRadius, handleRadius * 2,
                            handleRadius * 2);
    }
}

// -----------------------------------------------------------------------------
//  序列化
// -----------------------------------------------------------------------------
Json Wire::ToJson() const
{
    Json j;
    j["id"] = id;

    Json points = Json::array();
    for (const ControlPoint& cp : pts)
    {
        Json e;
        PutPoint(e, "pos", cp.pos);
        e["type"] = static_cast<int>(cp.type);
        points.push_back(e);
    }
    j["points"] = points;

    auto dumpEp = [](const WireEndpoint& e)
    {
        Json je;
        je["comp"] = e.compId;
        je["pin"] = e.pinIndex;
        PutPoint(je, "pos", e.pos);
        return je;
    };
    j["left"] = dumpEp(left);
    j["right"] = dumpEp(right);
    j["signal"] = ToUtf8(LogicSignalName(signal));

    return j;
}

void Wire::FromJson(const Json& j)
{
    id = GetInt(j, "id", 0);

    pts.clear();
    if (Has(j, "points") && j["points"].is_array())
    {
        for (const Json& e : j["points"])
        {
            ControlPoint cp;
            cp.pos = GetPoint(e, "pos", Point(GetInt(e, "x", 0), GetInt(e, "y", 0)));
            cp.type = static_cast<CPType>(GetInt(e, "type", static_cast<int>(CPType::Free)));
            pts.push_back(cp);
        }
    }

    auto readEp = [](const Json& je) -> WireEndpoint
    {
        WireEndpoint e;
        e.compId = GetInt(je, "comp", -1);
        e.pinIndex = GetInt(je, "pin", -1);
        e.pos = GetPoint(je, "pos");
        return e;
    };

    if (Has(j, "left") && j["left"].is_object()) left = readEp(j["left"]);
    if (Has(j, "right") && j["right"].is_object()) right = readEp(j["right"]);

    signal = LogicSignalFromName(GetStr(j, "signal", "Z"), LogicSignal::Z);

    // 老文件里若没有 points，就用端点补一条
    if (pts.empty()) pts = Route(left.pos, right.pos);
}

}  // namespace sf
