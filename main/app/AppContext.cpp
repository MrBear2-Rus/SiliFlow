#include "AppContext.h"

namespace sf
{

AppContext::AppContext() = default;

AppContext::~AppContext() = default;

bool AppContext::Initialize()
{
    m_library = Library::CreateBuiltin();
    if (m_library == nullptr) return false;

    m_project = Project::CreateNew(m_library.get());
    if (m_project == nullptr) return false;

    Rebind();
    m_undoStack.Clear();
    return true;
}

void AppContext::Rebind()
{
    library = m_library.get();
    project = m_project.get();
    undo = &m_undoStack;
    sim = &m_sim;
}

void AppContext::NewProject()
{
    m_undoStack.Clear();
    m_project = Project::CreateNew(m_library.get());
    Rebind();

    Log("已新建工程。");
    NotifyActiveSheetChanged();
    NotifySheetChanged();
    NotifySelectionChanged();
    NotifyUndoStateChanged();
}

bool AppContext::OpenProject(const wxString& path, wxString* err)
{
    if (m_project != nullptr && m_project->dirty)
    {
        // 覆盖提示由调用方（MainFrame）负责，这里只做「保存后打开」的兜底
        Log("注意：当前工程有未保存的改动。");
    }

    auto loaded = std::make_unique<Project>();
    if (!loaded->Load(path, m_library.get(), err)) return false;

    m_project = std::move(loaded);
    m_undoStack.Clear();
    Rebind();

    Log(wxString::Format("已打开工程：%s（%zu 张图纸）", path, m_project->sheets.size()));
    NotifyActiveSheetChanged();
    NotifySheetChanged();
    NotifySelectionChanged();
    NotifyUndoStateChanged();
    return true;
}

bool AppContext::SaveProjectTo(const wxString& path, wxString* err)
{
    if (m_project == nullptr) return false;
    if (!m_project->Save(path, err)) return false;

    Log(wxString::Format("已保存：%s", path));
    NotifySheetChanged();
    return true;
}

bool AppContext::SaveProject(wxString* err)
{
    if (m_project == nullptr) return false;

    if (m_project->path.IsEmpty())
    {
        if (err != nullptr) *err = "尚未指定保存路径";
        return false;
    }
    return SaveProjectTo(m_project->path, err);
}

}  // namespace sf
