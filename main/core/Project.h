#pragma once

// =============================================================================
//  SiliFlow · core/Project.h
//  工程：多张图纸 + 元件库引用 + 与 Verilog 对侧文件的联动状态。
//  落盘格式 .sfproj —— 纯文本 JSON，便于 Git diff 与人工查看。
// =============================================================================

#include "Circuit.h"
#include "Json.h"

#include <memory>
#include <vector>

namespace sf
{

class Library;

class Project
{
public:
    wxString name = "未命名";
    wxString path;          ///< .sfproj 全路径（未保存过则为空）
    Library* library = nullptr;  ///< 非持有

    std::vector<std::unique_ptr<CircuitSheet>> sheets;
    wxString topSheetName = "main";
    bool dirty = false;

    wxString linkedVerilogPath;        ///< 双向同步的对侧 .v 文件
    wxLongLong lastVerilogSync = 0;    ///< 上次同步时间戳（仲裁用）

    // ---- 图纸管理 ----
    CircuitSheet* Top();
    const CircuitSheet* Top() const;
    CircuitSheet* Sheet(const wxString& name) const;
    CircuitSheet* AddSheet(const wxString& name);
    bool RemoveSheet(const wxString& name);
    wxString UniqueSheetName(const wxString& base) const;

    /// 当前活动图纸名（由 UI 层维护，供保存/导出使用）。
    wxString activeSheetName = "main";
    CircuitSheet* Active();
    const CircuitSheet* Active() const;

    // ---- 生命周期 ----
    static std::unique_ptr<Project> CreateNew(Library* library);

    bool Save(const wxString& path, wxString* err = nullptr);
    bool Load(const wxString& path, Library* library, wxString* err = nullptr);

    Json ToJson() const;
    bool FromJson(const Json& j, Library* library, wxString* err = nullptr);

    void MarkDirty() { dirty = true; }
    void MarkClean() { dirty = false; }

    /// 写进标题栏的字符串："demo.sfproj * — SiliFlow"
    wxString DisplayName() const;
};

}  // namespace sf
