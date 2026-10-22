#pragma once

// =============================================================================
//  SiliFlow · panels/WavePanel.h
//  波形面板（框架版）：
//    左侧信号树 + 右侧波形区 + 顶部工具条的布局已经就位，
//    但波形解析与绘制属于 M5 里程碑，当前显示空状态。
//
//  之所以先把这个面板搭出来，是因为「四视图联动」是 SiliFlow 的骨架特征，
//  面板先存在、接口先定死，后面填实现即可。
// =============================================================================

#include "core/Services.h"

#include <wx/panel.h>
#include <wx/splitter.h>
#include <wx/string.h>

#include <vector>

class wxStaticText;
class wxTreeCtrl;
class wxPaintEvent;

namespace sf
{

namespace ui
{
class PanelHeader;
}  // namespace ui

/// 波形绘制区（当前只画空状态与时间轴标尺）
class WaveCanvas : public wxPanel
{
public:
    explicit WaveCanvas(wxWindow* parent, wxWindowID id = wxID_ANY);

    void SetPlaceholder(const wxString& text);
    void SetSignalCount(int count);

private:
    void OnPaint(wxPaintEvent& event);

    /// 左侧信号名列宽 / 顶部时间轴高 / 行高（都按 DPI 换算）
    int NameColumnWidth() const;
    int TimeAxisHeight() const;
    int RowHeight() const;

    wxString m_placeholder = "尚未加载波形文件";
    int m_signalCount = 0;
};

/// 波形面板容器
class WavePanel : public wxPanel
{
public:
    WavePanel(wxWindow* parent, Services* services, wxWindowID id = wxID_ANY);

    /// 刷新信号树与波形区（当前为清空 + 占位提示）
    void Reset();

    WaveCanvas* Canvas() const { return m_canvas; }

private:
    void ResetWaveform();

    Services* m_services = nullptr;
    wxSplitterWindow* m_splitter = nullptr;
    wxTreeCtrl* m_signalTree = nullptr;
    WaveCanvas* m_canvas = nullptr;
    ui::PanelHeader* m_head = nullptr;
};

}  // namespace sf
