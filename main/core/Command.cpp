#include "Command.h"

namespace sf
{

// -----------------------------------------------------------------------------
//  SnapshotCommand
// -----------------------------------------------------------------------------
void SnapshotCommand::Undo(CircuitSheet& sheet)
{
    sheet.FromJson(m_before, m_library);
}

void SnapshotCommand::Redo(CircuitSheet& sheet)
{
    sheet.FromJson(m_after, m_library);
}

// -----------------------------------------------------------------------------
//  UndoStack
// -----------------------------------------------------------------------------
void UndoStack::Push(std::unique_ptr<Command> cmd)
{
    if (cmd == nullptr) return;

    m_undo.push_back(std::move(cmd));

    // 新操作让「重做」分支失效
    m_redo.clear();

    while (m_undo.size() > m_limit) m_undo.pop_front();
}

bool UndoStack::Undo(CircuitSheet& sheet)
{
    if (m_undo.empty()) return false;

    std::unique_ptr<Command> cmd = std::move(m_undo.back());
    m_undo.pop_back();
    cmd->Undo(sheet);
    m_redo.push_back(std::move(cmd));
    return true;
}

bool UndoStack::Redo(CircuitSheet& sheet)
{
    if (m_redo.empty()) return false;

    std::unique_ptr<Command> cmd = std::move(m_redo.back());
    m_redo.pop_back();
    cmd->Redo(sheet);
    m_undo.push_back(std::move(cmd));
    return true;
}

void UndoStack::Clear()
{
    m_undo.clear();
    m_redo.clear();
}

void UndoStack::SetLimit(size_t limit)
{
    m_limit = (limit == 0) ? 1 : limit;
    while (m_undo.size() > m_limit) m_undo.pop_front();
}

// -----------------------------------------------------------------------------
//  ScopeTransaction
// -----------------------------------------------------------------------------
ScopeTransaction::ScopeTransaction(CircuitSheet& sheet, UndoStack* stack, const Library* library, wxString name)
    : m_sheet(sheet), m_stack(stack), m_library(library), m_name(std::move(name))
{
    // 栈为空时不必真去快照，省一次序列化
    if (m_stack != nullptr) m_before = m_sheet.ToJson();
}

ScopeTransaction::~ScopeTransaction()
{
    // 没有显式 Commit 的事务视为放弃
}

void ScopeTransaction::Commit()
{
    if (!m_active) return;
    m_active = false;

    if (m_stack == nullptr) return;

    Json after = m_sheet.ToJson();
    if (after == m_before) return;  // 无实质变化不入栈，避免撤销栈被空操作塞满

    m_stack->Push(std::make_unique<SnapshotCommand>(m_name, std::move(m_before), std::move(after), m_library));
}

}  // namespace sf
