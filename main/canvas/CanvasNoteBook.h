#pragma once

// =============================================================================
//  SiliFlow · canvas/CanvasNoteBook.h
//  图纸标签页容器：每张 CircuitSheet 对应一个 CanvasPanel。
//  只做「页 ↔ 图纸」的映射与标签页右键菜单，不含任何编辑逻辑。
// =============================================================================

#include "core/Services.h"

#include <wx/aui/auibook.h>
#include <wx/string.h>

#include <utility>
#include <vector>

class wxAuiNotebookEvent;

namespace sf
{

class CanvasPanel;

class CanvasNoteBook : public wxAuiNotebook
{
public:
    CanvasNoteBook(wxWindow* parent, Services* services);

    /// 依据 project->sheets 重建全部标签页
    void RebuildFromProject();

    CanvasPanel* ActiveCanvas() const;
    wxString ActiveSheetName() const;

    CanvasPanel* CanvasFor(const wxString& sheetName) const;

    /// 切换（若不存在则先建页）。返回是否成功。
    bool SetActiveSheet(const wxString& sheetName);

    /// 新建一张图纸并切过去
    void CreateNewSheet();

    /// 删除指定图纸（至少保留一张）
    void CloseSheetByName(const wxString& name);

private:
    void OnPageChanged(wxAuiNotebookEvent& event);
    void OnPageClose(wxAuiNotebookEvent& event);
    void OnTabRightUp(wxAuiNotebookEvent& event);

    Services* m_services = nullptr;
    std::vector<std::pair<wxString, CanvasPanel*>> m_pages;

    // 右键菜单里当前作用的页面
    int m_contextPage = -1;
};

}  // namespace sf
