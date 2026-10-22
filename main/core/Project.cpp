#include "Project.h"

#include "Library.h"

#include <wx/file.h>
#include <wx/filefn.h>
#include <wx/filename.h>

#include <algorithm>

namespace sf
{

namespace
{

/// 读取整个文件为字符串。失败返回 false。
bool ReadWholeFile(const wxString& path, std::string& out, wxString* err)
{
    if (!wxFileExists(path))
    {
        if (err != nullptr) *err = "文件不存在：" + path;
        return false;
    }

    wxFile file;
    if (!file.Open(path, wxFile::read))
    {
        if (err != nullptr) *err = "无法打开文件：" + path;
        return false;
    }

    const wxFileOffset len = file.Length();
    out.clear();
    out.resize(static_cast<size_t>(std::max<wxFileOffset>(0, len)));
    if (len > 0) file.Read(&out[0], static_cast<size_t>(len));
    file.Close();
    return true;
}

bool WriteWholeFile(const wxString& path, const std::string& text, wxString* err)
{
    wxFile file;
    if (!file.Create(path, true) || !file.Open(path, wxFile::write))
    {
        if (err != nullptr) *err = "无法写入文件：" + path;
        return false;
    }

    const bool ok = file.Write(text.c_str(), text.size()) == text.size();
    file.Close();

    if (!ok && err != nullptr) *err = "写入不完整：" + path;
    return ok;
}

}  // namespace

// -----------------------------------------------------------------------------
//  图纸管理
// -----------------------------------------------------------------------------
CircuitSheet* Project::Top()
{
    if (CircuitSheet* s = Sheet(topSheetName)) return s;
    return sheets.empty() ? nullptr : sheets.front().get();
}

const CircuitSheet* Project::Top() const
{
    if (const CircuitSheet* s = Sheet(topSheetName)) return s;
    return sheets.empty() ? nullptr : sheets.front().get();
}

CircuitSheet* Project::Sheet(const wxString& name) const
{
    for (const auto& s : sheets)
        if (s->name == name) return s.get();
    return nullptr;
}

CircuitSheet* Project::Active()
{
    if (CircuitSheet* s = Sheet(activeSheetName)) return s;
    return Top();
}

const CircuitSheet* Project::Active() const
{
    if (const CircuitSheet* s = Sheet(activeSheetName)) return s;
    return Top();
}

wxString Project::UniqueSheetName(const wxString& base) const
{
    if (Sheet(base) == nullptr) return base;

    for (int i = 2; i < 10000; ++i)
    {
        const wxString candidate = wxString::Format("%s%d", base, i);
        if (Sheet(candidate) == nullptr) return candidate;
    }
    return base + "_x";
}

CircuitSheet* Project::AddSheet(const wxString& name)
{
    auto sheet = std::make_unique<CircuitSheet>();
    sheet->name = UniqueSheetName(name.IsEmpty() ? wxString("sheet") : name);

    CircuitSheet* raw = sheet.get();
    sheets.push_back(std::move(sheet));
    MarkDirty();
    return raw;
}

bool Project::RemoveSheet(const wxString& name)
{
    auto it = std::find_if(sheets.begin(), sheets.end(),
                           [&name](const std::unique_ptr<CircuitSheet>& s) { return s->name == name; });
    if (it == sheets.end()) return false;

    sheets.erase(it);
    if (topSheetName == name && !sheets.empty()) topSheetName = sheets.front()->name;
    if (activeSheetName == name && !sheets.empty()) activeSheetName = sheets.front()->name;

    MarkDirty();
    return true;
}

wxString Project::DisplayName() const
{
    wxString base = name;
    if (!path.IsEmpty()) base = wxFileName(path).GetFullName();

    return wxString::Format("%s%s", base, dirty ? " *" : "");
}

// -----------------------------------------------------------------------------
//  生命周期
// -----------------------------------------------------------------------------
std::unique_ptr<Project> Project::CreateNew(Library* library)
{
    auto p = std::make_unique<Project>();
    p->library = library;
    p->name = "未命名";
    p->topSheetName = "main";

    CircuitSheet* sheet = p->AddSheet("main");
    p->activeSheetName = sheet->name;
    p->MarkClean();
    return p;
}

Json Project::ToJson() const
{
    Json j;
    j["format"] = "siliFlow-project";
    j["version"] = 2;  // v2 起元件带位宽（width）
    j["name"] = ToUtf8(name);
    j["top"] = ToUtf8(topSheetName);
    if (!linkedVerilogPath.IsEmpty()) j["linkedVerilog"] = ToUtf8(linkedVerilogPath);
    j["lastVerilogSync"] = static_cast<long long>(lastVerilogSync.GetValue());

    Json arr = Json::array();
    for (const auto& s : sheets) arr.push_back(s->ToJson());
    j["sheets"] = arr;

    return j;
}

bool Project::FromJson(const Json& j, Library* lib, wxString* err)
{
    if (!j.is_object())
    {
        if (err != nullptr) *err = "工程文件根节点不是对象";
        return false;
    }

    const wxString format = GetStr(j, "format", "siliFlow-project");
    if (format != "siliFlow-project")
    {
        if (err != nullptr) *err = "不是 SiliFlow 工程文件（format = " + format + "）";
        return false;
    }

    // ---- 格式版本迁移 ----
    // v1 的元件没有 width 字段、导线没有 strokeWidth 字段。这两项的缺省值
    // 正好就是 1 位 / 2px，所以 v1 读进来天然等价 —— 这里是显式的说明，
    // 不是空实现：以后要是改成非 1 的缺省值，就必须在这里补真正的迁移逻辑。
    const int version = GetInt(j, "version", 1);
    (void)version;

    library = lib;
    sheets.clear();

    name = GetStr(j, "name", "未命名");
    topSheetName = GetStr(j, "top", "main");
    linkedVerilogPath = GetStr(j, "linkedVerilog");
    lastVerilogSync = wxLongLong(static_cast<wxLongLong_t>(GetDouble(j, "lastVerilogSync", 0.0)));

    if (Has(j, "sheets") && j["sheets"].is_array())
    {
        for (const Json& e : j["sheets"])
        {
            auto sheet = std::make_unique<CircuitSheet>();
            sheet->FromJson(e, lib);
            sheets.push_back(std::move(sheet));
        }
    }

    if (sheets.empty())
    {
        if (err != nullptr) *err = "工程里没有任何图纸";
        return false;
    }

    activeSheetName = topSheetName;
    MarkClean();
    return true;
}

bool Project::Save(const wxString& savePath, wxString* err)
{
    const std::string text = ToJson().dump(2);
    if (!WriteWholeFile(savePath, text, err)) return false;

    path = savePath;
    if (name.IsEmpty() || name == "未命名") name = wxFileName(savePath).GetName();
    MarkClean();
    return true;
}

bool Project::Load(const wxString& loadPath, Library* lib, wxString* err)
{
    std::string text;
    if (!ReadWholeFile(loadPath, text, err)) return false;

    Json j;
    try
    {
        j = Json::parse(text);
    }
    catch (const std::exception& e)
    {
        if (err != nullptr) *err = wxString("JSON 解析失败：") + e.what();
        return false;
    }

    if (!FromJson(j, lib, err)) return false;

    path = loadPath;
    if (name.IsEmpty() || name == "未命名") name = wxFileName(loadPath).GetName();
    MarkClean();
    return true;
}

}  // namespace sf
