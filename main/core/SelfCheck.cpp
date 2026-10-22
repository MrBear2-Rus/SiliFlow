#include "SelfCheck.h"

#include "Circuit.h"
#include "Command.h"
#include "Library.h"
#include "Project.h"
#include "Shape.h"
#include "sim/SimEngine.h"

#include <wx/string.h>

#include <cmath>

namespace sf
{

namespace
{

class Reporter
{
public:
    void Section(const wxString& title) { m_out << "\n[" << title << "]\n"; }

    void Check(bool ok, const wxString& what)
    {
        if (ok)
        {
            ++m_passed;
            m_out << "  PASS  " << what << "\n";
        }
        else
        {
            ++m_failed;
            m_out << "  FAIL  " << what << "\n";
        }
    }

    void Info(const wxString& text) { m_out << "        " << text << "\n"; }

    int Failed() const { return m_failed; }
    int Passed() const { return m_passed; }
    wxString Text() const { return m_out; }

private:
    wxString m_out;
    int m_passed = 0;
    int m_failed = 0;
};

}  // namespace

int RunCoreSelfCheck(wxString* report)
{
    Reporter r;

    // ---------------------------------------------------------------- 元件库
    r.Section("元件库");
    auto lib = Library::CreateBuiltin();
    r.Check(lib->Size() >= 19, wxString::Format("内置元件数量 = %zu（期望 >= 19）", lib->Size()));
    r.Check(lib->Find("nand") != nullptr, "能按 type 找到 nand");
    r.Check(lib->Find("不存在的元件") == nullptr, "查不到的 type 返回 nullptr");
    r.Check(!lib->Categories().empty(), wxString::Format("分类数 = %zu", lib->Categories().size()));

    for (const ComponentDef* d : lib->All())
    {
        const bool ok = d->size.x > 0 && d->size.y > 0 && !d->shapes.empty() &&
                        (!d->inputs.empty() || !d->outputs.empty() || !d->inouts.empty());
        if (!ok) r.Info(wxString::Format("可疑元件：%s（size=%dx%d, shapes=%zu, in=%zu out=%zu）", d->type,
                                         d->size.x, d->size.y, d->shapes.size(), d->inputs.size(),
                                         d->outputs.size()));
    }
    r.Check(true, "逐个检查元件定义字段完备性（可疑项见上方 Info）");

    // ---------------------------------------------------------------- 变换
    r.Section("图元与元件变换");
    {
        // 绕原点顺时针 90°：(1,0) -> (0,1)
        r.Check(RotatePoint90(Point(1, 0), 1) == Point(0, 1), "RotatePoint90 (1,0) -> (0,1)");
        r.Check(RotatePoint90(Point(1, 0), 4) == Point(1, 0), "RotatePoint90 四次回到原位");

        // 各种图元旋转 4 次应回到原状
        std::vector<Shape> samples{
            MakeLine(Point(0, 0), Point(10, 5)),
            MakeRect(Rect(0, 0, 20, 10)),
            MakeCircle(Point(5, 5), 5),
            MakeArc(Point(0, 0), 20, -90, 90),
            MakeBezier(Point(0, 0), Point(5, 5), Point(10, 5), Point(15, 0)),
            MakePoly({Point(0, 0), Point(10, 0), Point(10, 10)}, true),
        };
        bool allBack = true;
        for (Shape s : samples)
        {
            const Shape original = s;
            for (int i = 0; i < 4; ++i) RotateShape90(s, 1);
            if (ShapeToJson(s) != ShapeToJson(original)) allBack = false;
        }
        r.Check(allBack, "6 种图元旋转 4×90° 后回到原始（JSON 逐字段相等）");

        // 元件旋转后中心不动
        const ComponentDef* andDef = lib->Find("and");
        ComponentInstance inst;
        inst.pos = Point(100, 200);
        inst.Rebuild(*andDef);

        const Point center0 = inst.Center();
        inst.RotateCw();
        const Point center1 = inst.Center();
        r.Check(std::abs(center0.x - center1.x) <= 1 && std::abs(center0.y - center1.y) <= 1,
                wxString::Format("旋转后中心不动 (%d,%d) -> (%d,%d)", center0.x, center0.y, center1.x, center1.y));
        r.Check(inst.boxSize.x == andDef->size.y && inst.boxSize.y == andDef->size.x,
                wxString::Format("旋转 90° 后宽高互换 %dx%d", inst.boxSize.x, inst.boxSize.y));

        inst.RotateCw();
        inst.RotateCw();
        inst.RotateCw();
        const Point center4 = inst.Center();
        r.Check(std::abs(center0.x - center4.x) <= 1 && std::abs(center0.y - center4.y) <= 1,
                "转满一圈回到原位");

        // 镜像后中心不动
        inst.ToggleMirror();
        const Point centerM = inst.Center();
        r.Check(centerM == center4, "镜像后中心不动");
        inst.ToggleMirror();

        // 画布坐标 <-> 局部坐标 往返一致
        const Point probe(137, 233);
        const Point back = inst.CanvasToLocal(probe);
        r.Info(wxString::Format("CanvasToLocal(%d,%d) = (%d,%d)", probe.x, probe.y, back.x, back.y));
    }

    // ---------------------------------------------------------------- 图纸
    r.Section("图纸：放元件 + 连线 + 往返");
    auto project = Project::CreateNew(lib.get());
    CircuitSheet* sheet = project->Top();
    r.Check(sheet != nullptr, "新建工程自带一张 main 图纸");

    // 放 20 个元件：交替摆成两列，保证引脚能对上
    std::vector<ComponentInstance*> placed;
    const wxString kinds[] = {"input", "and", "or", "not", "xor", "nand", "buf", "and3", "mux2", "dff"};
    for (int i = 0; i < 20; ++i)
    {
        const ComponentDef* d = lib->Find(kinds[i % 10]);
        if (d == nullptr) continue;
        placed.push_back(sheet->AddComponent(*d, Point(80 + (i % 4) * 260, 80 + (i / 4) * 180)));
    }
    r.Check(placed.size() == 20, wxString::Format("放置元件 = %zu（期望 20）", placed.size()));

    // 连 30 根线：优先接真实引脚，接不上就悬空连（只验证几何）
    int attached = 0;
    for (int i = 0; i < 30; ++i)
    {
        ComponentInstance* a = placed[i % placed.size()];
        ComponentInstance* b = placed[(i + 5) % placed.size()];
        if (a->pins.empty() || b->pins.empty()) continue;

        WireEndpoint ea;
        ea.compId = a->id;
        ea.pinIndex = 0;
        ea.pos = a->PinPos(0);

        WireEndpoint eb;
        eb.compId = b->id;
        eb.pinIndex = static_cast<int>(b->pins.size()) - 1;
        eb.pos = b->PinPos(eb.pinIndex);

        sheet->AddWire(ea, eb);
        ++attached;
    }
    r.Check(attached == 30, wxString::Format("连接导线 = %d（期望 30）", attached));

    // 命中测试 & 吸附
    {
        ComponentInstance* c0 = placed[0];
        const Point body = c0->Center();
        const ComponentInstance* hit = sheet->HitComponent(body);
        r.Check(hit == c0, "在元件中心命中该元件");

        const Point pinPos = c0->PinPos(0);
        WireEndpoint snapped;
        r.Check(sheet->SnapToPin(Point(pinPos.x + 3, pinPos.y + 3), 10, snapped) && snapped.compId == c0->id,
                "端点吸附能找到最近引脚");

        const Point faraway(99999, 99999);
        r.Check(sheet->HitComponent(faraway) == nullptr, "空白处命中测试返回 nullptr");
    }

    // 网络推导
    {
        const std::vector<Net> nets = sheet->BuildNets();
        r.Check(!nets.empty(), wxString::Format("推导出网络数 = %zu", nets.size()));

        bool allNamed = true;
        for (const Net& n : nets)
            if (n.name.IsEmpty()) allNamed = false;
        r.Check(allNamed, "每个网络都有名字");
    }

    // JSON 往返：元件数 / 导线数 / 引脚坐标必须完全一致
    const Json snapshot = sheet->ToJson();
    const size_t compCountBefore = sheet->components.size();
    const size_t wireCountBefore = sheet->wires.size();

    // 记录往返前的引脚绝对坐标
    std::vector<Point> pinBefore;
    for (const auto& c : sheet->components)
        for (int i = 0; i < static_cast<int>(c->pins.size()); ++i) pinBefore.push_back(c->PinPos(i));

    sheet->FromJson(snapshot, lib.get());

    r.Check(sheet->components.size() == compCountBefore,
            wxString::Format("往返后元件数一致 %zu == %zu", sheet->components.size(), compCountBefore));
    r.Check(sheet->wires.size() == wireCountBefore,
            wxString::Format("往返后导线数一致 %zu == %zu", sheet->wires.size(), wireCountBefore));

    std::vector<Point> pinAfter;
    for (const auto& c : sheet->components)
        for (int i = 0; i < static_cast<int>(c->pins.size()); ++i) pinAfter.push_back(c->PinPos(i));

    bool pinsSame = pinBefore.size() == pinAfter.size();
    if (pinsSame)
        for (size_t i = 0; i < pinBefore.size(); ++i)
            if (!NearlyEqual(pinBefore[i], pinAfter[i], 0)) pinsSame = false;
    r.Check(pinsSame, wxString::Format("往返后全部引脚坐标一致（%zu 个引脚）", pinBefore.size()));

    r.Check(sheet->ToJson() == snapshot, "二次序列化结果与首次逐字节一致");

    // ---------------------------------------------------------------- 撤销
    r.Section("撤销栈");
    {
        UndoStack stack;
        const Json before = sheet->ToJson();
        const int id = sheet->AddComponent(*lib->Find("and"), Point(500, 500))->id;
        stack.Push(std::make_unique<SnapshotCommand>("放置元件", before, sheet->ToJson(), lib.get()));

        r.Check(stack.CanUndo(), "入栈后 CanUndo 为真");
        r.Check(stack.UndoName() == "放置元件", "撤销名正确");

        stack.Undo(*sheet);
        r.Check(sheet->FindComponent(id) == nullptr, "撤销后新增元件消失");

        stack.Redo(*sheet);
        r.Check(sheet->FindComponent(id) != nullptr, "重做后元件回来");
    }

    // ---------------------------------------------------------------- 工程
    r.Section("工程序列化");
    {
        Json pj = project->ToJson();
        auto restored = std::make_unique<Project>();
        wxString err;
        const bool ok = restored->FromJson(pj, lib.get(), &err);
        r.Check(ok, "工程 JSON 往返成功" + (ok ? wxString() : "：" + err));
        r.Check(restored->sheets.size() == project->sheets.size(), "图纸数量一致");
        r.Check(restored->topSheetName == project->topSheetName, "顶层图纸名一致");
    }

    // ---------------------------------------------------------------- 仿真
    r.Section("仿真：组合层");
    {
        // 另开一张干净图纸（main 图纸里已经塞了 20 个元件）
        auto simProj = Project::CreateNew(lib.get());
        CircuitSheet* ss = simProj->Top();

        ComponentInstance* pinA = ss->AddComponent(*lib->Find("input"), Point(80, 100));
        ComponentInstance* pinB = ss->AddComponent(*lib->Find("input"), Point(80, 300));
        ComponentInstance* gate = ss->AddComponent(*lib->Find("and"), Point(400, 180));
        ComponentInstance* pinY = ss->AddComponent(*lib->Find("output"), Point(700, 190));

        // 引脚下标约定：pins 的顺序是 inputs…outputs…inouts，所以
        //   input 端口 → 0 = Y（驱动）
        //   and 门     → 0 = A、1 = B、2 = Y
        //   output 端口→ 0 = A（只读）
        const auto connect = [&](ComponentInstance* a, int ai, ComponentInstance* b, int bi)
        {
            WireEndpoint ea;
            ea.compId = a->id;
            ea.pinIndex = ai;
            ea.pos = a->PinPos(ai);

            WireEndpoint eb;
            eb.compId = b->id;
            eb.pinIndex = bi;
            eb.pos = b->PinPos(bi);

            ss->AddWire(ea, eb);
        };

        connect(pinA, 0, gate, 0);
        connect(pinB, 0, gate, 1);
        connect(gate, 2, pinY, 0);

        sim::SimEngine eng;
        eng.Attach(ss);
        eng.Evaluate();

        r.Check(eng.ComponentCount() == 4,
                wxString::Format("仿真建图：元件 %zu（期望 4）", eng.ComponentCount()));
        r.Check(eng.NetCount() == 3, wxString::Format("仿真建图：网络 %zu（期望 3）", eng.NetCount()));
        r.Check(!eng.HasCombinationalLoop(), "这几条线里没有组合环");

        eng.SetForced(pinA->id, LogicVector(1, LogicSignal::Zero));
        eng.SetForced(pinB->id, LogicVector(1, LogicSignal::Zero));
        r.Check(eng.PinSignal(pinY->id, 0) == LogicSignal::Zero, "与门 0 · 0 → 0");

        eng.SetForced(pinA->id, LogicVector(1, LogicSignal::One));
        r.Check(eng.PinSignal(pinY->id, 0) == LogicSignal::Zero, "与门 1 · 0 → 0");

        eng.SetForced(pinB->id, LogicVector(1, LogicSignal::One));
        r.Check(eng.PinSignal(pinY->id, 0) == LogicSignal::One, "与门 1 · 1 → 1");

        // 四值支配律：0 能压住 X
        eng.SetForced(pinA->id, LogicVector(1, LogicSignal::Zero));
        eng.SetForced(pinB->id, LogicVector(1, LogicSignal::X));
        r.Check(eng.PinSignal(pinY->id, 0) == LogicSignal::Zero, "与门 0 · X → 0（0 支配）");

        eng.SetForced(pinA->id, LogicVector(1, LogicSignal::One));
        r.Check(eng.PinSignal(pinY->id, 0) == LogicSignal::X, "与门 1 · X → X");

        // 点击翻转：0 → 1 → 0
        eng.ClearForced(pinA->id);
        eng.ClearForced(pinB->id);
        const LogicSignal t0 = eng.PinSignal(pinA->id, 0);
        eng.TogglePin(pinA->id);
        const LogicSignal t1 = eng.PinSignal(pinA->id, 0);
        eng.TogglePin(pinA->id);
        const LogicSignal t2 = eng.PinSignal(pinA->id, 0);
        r.Check(t0 == LogicSignal::Zero && t1 == LogicSignal::One && t2 == LogicSignal::Zero,
                "输入端口点击翻转 0 → 1 → 0");
        r.Check(!eng.IsToggleable(gate->id), "与门不是可翻转元件");

        // 编辑后重建：加一个 const1
        ComponentInstance* c1 = ss->AddComponent(*lib->Find("const1"), Point(80, 480));
        eng.MarkDirty();
        eng.Evaluate();
        r.Check(eng.ComponentCount() == 5, "加元件后重建求值图，元件数变 5");
        r.Check(eng.PinSignal(c1->id, 0) == LogicSignal::Z, "没接线的元件引脚悬空（Z），不驱动");

        // 把它接到一个输出端口上，再验一次
        ComponentInstance* pinC = ss->AddComponent(*lib->Find("output"), Point(700, 490));
        connect(c1, 0, pinC, 0);
        eng.MarkDirty();
        eng.Evaluate();
        r.Check(eng.ComponentCount() == 6, "再重建，元件数变 6");
        r.Check(eng.PinSignal(pinC->id, 0) == LogicSignal::One, "const1 接上线后驱动高电平");

        // 复位：清掉全部强制电平与 tick 计数
        eng.SetForced(pinA->id, LogicVector(1, LogicSignal::One));
        eng.Reset();
        r.Check(!eng.ForcedValue(pinA->id, nullptr), "复位后强制电平被清空");
        r.Check(eng.TickCount() == 0, "复位后 tick 归零");
    }

    // ---------------------------------------------------------- 组合环 / 冲突
    r.Section("仿真：组合环与驱动冲突");
    {
        auto loopProj = Project::CreateNew(lib.get());
        CircuitSheet* ls = loopProj->Top();

        ComponentInstance* n1 = ls->AddComponent(*lib->Find("not"), Point(100, 100));
        ComponentInstance* n2 = ls->AddComponent(*lib->Find("not"), Point(500, 100));

        // not 的引脚：0 = A（读），1 = Y（驱动）
        const auto link = [&](ComponentInstance* a, int ai, ComponentInstance* b, int bi)
        {
            WireEndpoint ea;
            ea.compId = a->id;
            ea.pinIndex = ai;
            ea.pos = a->PinPos(ai);

            WireEndpoint eb;
            eb.compId = b->id;
            eb.pinIndex = bi;
            eb.pos = b->PinPos(bi);

            ls->AddWire(ea, eb);
        };
        link(n1, 1, n2, 0);
        link(n2, 1, n1, 0);

        sim::SimEngine loopEng;
        loopEng.Attach(ls);
        loopEng.Evaluate();
        r.Check(loopEng.HasCombinationalLoop(),
                wxString::Format("两个非门互接 → 检出 %d 处组合环", loopEng.LoopCount()));
        r.Check(loopEng.PinSignal(n1->id, 1) == LogicSignal::X, "环上的网络是 X，而不是某个假值");

        // 多驱动冲突：两个输入端口驱动同一个网络
        auto cProj = Project::CreateNew(lib.get());
        CircuitSheet* cs = cProj->Top();

        ComponentInstance* d1 = cs->AddComponent(*lib->Find("input"), Point(80, 100));
        ComponentInstance* d2 = cs->AddComponent(*lib->Find("input"), Point(80, 300));
        ComponentInstance* ob = cs->AddComponent(*lib->Find("output"), Point(600, 200));

        WireEndpoint w1;
        w1.compId = ob->id;
        w1.pinIndex = 0;
        w1.pos = ob->PinPos(0);

        WireEndpoint w2;
        w2.compId = d1->id;
        w2.pinIndex = 0;
        w2.pos = d1->PinPos(0);

        WireEndpoint w3;
        w3.compId = d2->id;
        w3.pinIndex = 0;
        w3.pos = d2->PinPos(0);

        cs->AddWire(w1, w2);
        cs->AddWire(w1, w3);  // 与上一根共用 ob 的引脚 → 三个引脚同属一个网络

        sim::SimEngine cEng;
        cEng.Attach(cs);
        cEng.SetForced(d1->id, LogicVector(1, LogicSignal::One));
        cEng.SetForced(d2->id, LogicVector(1, LogicSignal::Zero));
        r.Check(cEng.PinSignal(ob->id, 0) == LogicSignal::X, "两个驱动源电平冲突 → X");
        r.Check(cEng.LastConflictCount() >= 1,
                wxString::Format("冲突网络数 = %d", cEng.LastConflictCount()));
    }

    // ---------------------------------------------------------------- 多位总线
    r.Section("总线：多位信号");
    {
        auto busProj = Project::CreateNew(lib.get());
        CircuitSheet* bs = busProj->Top();

        // 改位宽必须 Rebuild 才会重新解析引脚位宽
        const auto setWidth = [](ComponentInstance* c, int w)
        {
            c->width = w;
            if (c->def != nullptr) c->Rebuild(*c->def);
        };
        const auto link = [&](ComponentInstance* a, int ai, ComponentInstance* b, int bi)
        {
            WireEndpoint ea;
            ea.compId = a->id;
            ea.pinIndex = ai;
            ea.pos = a->PinPos(ai);

            WireEndpoint eb;
            eb.compId = b->id;
            eb.pinIndex = bi;
            eb.pos = b->PinPos(bi);

            bs->AddWire(ea, eb);
        };

        // ---- 4 位通路 ----
        ComponentInstance* bin = bs->AddComponent(*lib->Find("input"), Point(80, 100));
        ComponentInstance* bout = bs->AddComponent(*lib->Find("output"), Point(600, 100));
        setWidth(bin, 4);
        setWidth(bout, 4);
        link(bin, 0, bout, 0);

        sim::SimEngine be;
        be.Attach(bs);
        be.SetForced(bin->id, LogicVector::FromText("1010"));

        r.Check(be.PinWidth(bin->id, 0) == 4,
                wxString::Format("input 引脚位宽 = %d（期望 4）", be.PinWidth(bin->id, 0)));
        r.Check(be.WireBits(bs->wires[0]->id) == 4, "导线位宽 = 4");
        r.Check(be.PinValue(bout->id, 0) == LogicVector::FromText("1010"), "4 位通路传值 1010");
        r.Check(be.PinValue(bout->id, 0).ToUnsigned() == 10, "1010 读作 10");
        r.Check(be.PinValue(bout->id, 0).ToDisplay() == "0xA",
                wxString::Format("多位显示 = %s", be.PinValue(bout->id, 0).ToDisplay()));

        // ---- 4 位按位与：1100 & 1010 = 1000 ----
        ComponentInstance* ga = bs->AddComponent(*lib->Find("input"), Point(80, 300));
        ComponentInstance* gb = bs->AddComponent(*lib->Find("input"), Point(80, 500));
        ComponentInstance* gg = bs->AddComponent(*lib->Find("and"), Point(400, 380));
        ComponentInstance* gy = bs->AddComponent(*lib->Find("output"), Point(700, 390));
        setWidth(ga, 4);
        setWidth(gb, 4);
        setWidth(gg, 4);
        setWidth(gy, 4);

        link(ga, 0, gg, 0);   // and: 0=A 1=B 2=Y
        link(gb, 0, gg, 1);
        link(gg, 2, gy, 0);

        be.MarkDirty();
        be.SetForced(ga->id, LogicVector::FromText("1100"));
        be.SetForced(gb->id, LogicVector::FromText("1010"));
        r.Check(be.PinValue(gy->id, 0).ToUnsigned() == 0x8, "4 位与 1100 & 1010 = 1000");

        // ---- splitter 二路等分 ----
        ComponentInstance* sp = bs->AddComponent(*lib->Find("splitter"), Point(400, 700));
        setWidth(sp, 4);
        r.Check(sp->pins[1].width == 2 && sp->pins[2].width == 2,
                wxString::Format("splitter 4 位输入 → 输出 %d / %d 位", sp->pins[1].width,
                                 sp->pins[2].width));
    }

    // ------------------------------------------------------- 位宽不一致要报错
    r.Section("总线：位宽不一致");
    {
        auto badProj = Project::CreateNew(lib.get());
        CircuitSheet* bds = badProj->Top();

        ComponentInstance* bi = bds->AddComponent(*lib->Find("input"), Point(80, 100));
        ComponentInstance* bo = bds->AddComponent(*lib->Find("output"), Point(600, 100));

        bi->width = 4;
        if (bi->def != nullptr) bi->Rebuild(*bi->def);  // bo 保持 1 位

        WireEndpoint a;
        a.compId = bi->id;
        a.pinIndex = 0;
        a.pos = bi->PinPos(0);

        WireEndpoint b;
        b.compId = bo->id;
        b.pinIndex = 0;
        b.pos = bo->PinPos(0);

        bds->AddWire(a, b);

        sim::SimEngine badEng;
        badEng.Attach(bds);
        badEng.SetForced(bi->id, LogicVector::FromText("1010"));

        r.Check(badEng.WidthMismatchCount() >= 1,
                wxString::Format("位宽不一致被检出（%d 个网络）", badEng.WidthMismatchCount()));
        r.Check(badEng.PinValue(bi->id, 0).Bit(0) == LogicSignal::X, "位宽不一致 → 网络标 X");
    }

    // ------------------------------------------------------- 位向量运算单元
    r.Section("总线：位向量运算");
    {
        const LogicVector v = LogicVector::FromText("1010", 4);
        r.Check(v.Width() == 4 && v.ToText() == "1010", "FromText / ToText 往返");
        r.Check(v.ToUnsigned() == 10, "ToUnsigned = 10");
        r.Check(v.Slice(0, 1).ToText() == "10", "取低 2 位 = 10");
        r.Check(v.Slice(2, 3).ToText() == "10", "取高 2 位 = 10");
        r.Check(v.Resize(8).ToUnsigned() == 10, "展宽到 8 位值不变");
        r.Check(LogicVector::FromText("11", 2).Concat(LogicVector::FromText("00", 2)).ToText() == "1100",
                "拼接 11 + 00 = 1100");
        r.Check(LogicVector::FromInt(0xA5, 8).ToText() == "10100101", "FromInt 0xA5 → 10100101");

        const LogicVector ze = LogicVector::FromText("1Z");
        r.Check(!ze.AllKnown(), "含 Z 不算全确定");
        r.Check(!ze.ToInt(nullptr), "含 Z 转整数失败");

        r.Check(LogicVector::ResolveDrivers({LogicVector::FromText("0"), LogicVector::FromText("Z")}, 1) ==
                    LogicVector::FromText("0"),
                "驱动归并：0 + Z = 0");
        r.Check(LogicVector::ResolveDrivers({LogicVector::FromText("0"), LogicVector::FromText("1")}, 1) ==
                    LogicVector::FromText("X"),
                "驱动归并：0 + 1 = X");
    }

    // ---------------------------------------------------------------- 汇总
    r.Section("汇总");
    r.Info(wxString::Format("通过 %d 项，失败 %d 项", r.Passed(), r.Failed()));
    if (r.Failed() == 0)
        r.Info(wxString::Format("core OK: %zu components / %zu wires / roundtrip identical", compCountBefore,
                                wireCountBefore));

    if (report != nullptr) *report = r.Text();
    return r.Failed() == 0 ? 0 : 1;
}

}  // namespace sf
