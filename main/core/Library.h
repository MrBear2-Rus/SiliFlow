#pragma once

// =============================================================================
//  SiliFlow · core/Library.h
//  元件库：按分类 / 关键词检索元件模板，支持从 JSON 载入与导出。
//  CreateBuiltin() 完全由代码构造，零资源依赖 —— 程序不带任何数据文件也能跑。
// =============================================================================

#include "Component.h"
#include "Json.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace sf
{

class Library
{
public:
    Library();

    // ---- 查询 ----
    const ComponentDef* Find(const wxString& type) const;
    std::vector<wxString> Categories() const;
    std::vector<const ComponentDef*> ByCategory(const wxString& category) const;
    std::vector<const ComponentDef*> Search(const wxString& keyword) const;
    std::vector<const ComponentDef*> All() const;

    size_t Size() const { return m_defs.size(); }

    // ---- 增删 ----
    /// type 冲突时覆盖旧定义。返回是否为覆盖。
    bool Add(std::unique_ptr<ComponentDef> def);
    bool Remove(const wxString& type);
    void Clear() { m_defs.clear(); m_order.clear(); }

    // ---- 存取 ----
    bool LoadFromFile(const wxString& path, bool merge, wxString* err = nullptr);
    bool SaveToFile(const wxString& path, wxString* err = nullptr) const;
    bool LoadFromString(const std::string& utf8, bool merge, wxString* err = nullptr);

    Json ToJson() const;

    /// 内置元件库（19 个 type / 24 个条目，含 3 入变体）。
    static std::unique_ptr<Library> CreateBuiltin();

private:
    std::map<std::string, std::unique_ptr<ComponentDef>> m_defs;
    std::vector<wxString> m_order;  ///< 保持插入顺序，供分类面板稳定显示
};

}  // namespace sf
