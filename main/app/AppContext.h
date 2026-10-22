#pragma once

// =============================================================================
//  SiliFlow · app/AppContext.h
//  应用上下文（L4）：持有元件库、工程、撤销栈，并实现 core::Services 的回调面。
//
//  L3 各模块只认 core::Services*，本类继承它，所以可以直接把 this 传下去，
//  同时又不让 L3 反向依赖 L4。
// =============================================================================

#include "core/Command.h"
#include "core/Library.h"
#include "core/Project.h"
#include "core/Services.h"
#include "core/sim/SimEngine.h"

#include <wx/string.h>

#include <memory>

namespace sf
{

class AppContext : public Services
{
public:
    AppContext();
    ~AppContext() override;

    /// 建内置元件库 + 新建空工程。成功返回 true。
    bool Initialize();

    // ---- 工程生命周期 ----
    void NewProject();
    bool OpenProject(const wxString& path, wxString* err = nullptr);
    bool SaveProjectTo(const wxString& path, wxString* err = nullptr);
    bool SaveProject(wxString* err = nullptr);  ///< 无路径时返回 false（调用方应改用另存为）

    Library* Lib() const { return m_library.get(); }
    Project* Proj() const { return m_project.get(); }
    UndoStack& Undo() { return m_undoStack; }
    sim::SimEngine& Sim() { return m_sim; }
    const sim::SimEngine& Sim() const { return m_sim; }

    bool HasProjectPath() const { return m_project != nullptr && !m_project->path.IsEmpty(); }

private:
    std::unique_ptr<Library> m_library;
    std::unique_ptr<Project> m_project;
    UndoStack m_undoStack;
    sim::SimEngine m_sim;

    /// 重新把 Services 里的裸指针指向当前工程 / 库
    void Rebind();
};

}  // namespace sf
