#pragma once

// =============================================================================
//  SiliFlow · core/Command.h
//  撤销栈。
//
//  采用**快照式**命令而不是细粒度反向操作：骨架阶段可靠优先，且单张图纸的
//  JSON 体积很小（通常 < 200 KB），快照成本可以接受。
//  事务边界由 CanvasEventHandler 决定：一次完整手势 = 一个事务。
// =============================================================================

#include "Circuit.h"
#include "Json.h"

#include <deque>
#include <memory>

namespace sf
{

class Library;

// -----------------------------------------------------------------------------
//  命令基类
// -----------------------------------------------------------------------------
class Command
{
public:
    virtual ~Command() = default;

    virtual wxString Name() const = 0;
    virtual void Undo(CircuitSheet& sheet) = 0;
    virtual void Redo(CircuitSheet& sheet) = 0;
};

// -----------------------------------------------------------------------------
//  图纸级 JSON 前后快照
// -----------------------------------------------------------------------------
class SnapshotCommand : public Command
{
public:
    SnapshotCommand(wxString name, Json before, Json after, const Library* library)
        : m_name(std::move(name)), m_before(std::move(before)), m_after(std::move(after)), m_library(library)
    {
    }

    wxString Name() const override { return m_name; }
    void Undo(CircuitSheet& sheet) override;
    void Redo(CircuitSheet& sheet) override;

private:
    wxString m_name;
    Json m_before, m_after;
    const Library* m_library = nullptr;
};

// -----------------------------------------------------------------------------
//  撤销栈
// -----------------------------------------------------------------------------
class UndoStack
{
public:
    static constexpr size_t kDefaultLimit = 100;

    void Push(std::unique_ptr<Command> cmd);
    bool CanUndo() const { return !m_undo.empty(); }
    bool CanRedo() const { return !m_redo.empty(); }

    wxString UndoName() const { return m_undo.empty() ? wxString() : m_undo.back()->Name(); }
    wxString RedoName() const { return m_redo.empty() ? wxString() : m_redo.back()->Name(); }

    bool Undo(CircuitSheet& sheet);
    bool Redo(CircuitSheet& sheet);
    void Clear();

    void SetLimit(size_t limit);
    size_t Limit() const { return m_limit; }
    size_t UndoCount() const { return m_undo.size(); }
    size_t RedoCount() const { return m_redo.size(); }

private:
    std::deque<std::unique_ptr<Command>> m_undo;
    std::deque<std::unique_ptr<Command>> m_redo;
    size_t m_limit = kDefaultLimit;
};

// -----------------------------------------------------------------------------
//  事务记录器：把「取 before → 干活 → 提交」这段套路封起来。
//  用法：
//      ScopeTransaction tx(sheet, undoStack, library, "移动元件");
//      ... 修改 sheet ...
//      tx.Commit();          // 未调 Commit 则析构时自动放弃，不入栈
// -----------------------------------------------------------------------------
class ScopeTransaction
{
public:
    ScopeTransaction(CircuitSheet& sheet, UndoStack* stack, const Library* library, wxString name);
    ~ScopeTransaction();

    ScopeTransaction(const ScopeTransaction&) = delete;
    ScopeTransaction& operator=(const ScopeTransaction&) = delete;

    void Commit();
    void Abort() { m_active = false; }

private:
    CircuitSheet& m_sheet;
    UndoStack* m_stack;
    const Library* m_library;
    wxString m_name;
    Json m_before;
    bool m_active = true;
};

}  // namespace sf
