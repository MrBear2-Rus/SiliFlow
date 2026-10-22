#pragma once

// =============================================================================
//  SiliFlow · panels/ConsolePanel.h
//  消息面板：导出/导入日志、操作提示、错误与性能计数。
// =============================================================================

#include <wx/panel.h>
#include <wx/string.h>

#include <vector>

class wxTextCtrl;

namespace sf
{

class ConsolePanel : public wxPanel
{
public:
    explicit ConsolePanel(wxWindow* parent, wxWindowID id = wxID_ANY);

    /// 追加一行（自动加时间戳）。level 只用于着色。
    void Append(const wxString& message);

    void Clear();

    /// 限制最大行数，避免长时间运行内存膨胀
    void SetMaxLines(size_t maxLines) { m_maxLines = maxLines; }
    size_t MaxLines() const { return m_maxLines; }

private:
    void TrimIfNeeded();

    wxTextCtrl* m_text = nullptr;
    size_t m_maxLines = 2000;
};

}  // namespace sf
