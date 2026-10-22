// =============================================================================
//  SiliFlow · cMain.cpp
//  程序入口。
//
//  命令行：
//    （无参数）        正常启动 GUI
//    -s / --selfcheck  只跑 core 数据层自检，结果打到控制台 + selfcheck.log 后退出
// =============================================================================

#include <wx/wx.h>

#include <wx/cmdline.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>

#include <cstdio>

#include "app/MainFrame.h"
#include "core/SelfCheck.h"
#include "core/TextConv.h"

namespace
{

/// 本程序是 GUI 子系统（/SUBSYSTEM:WINDOWS），从终端重定向 stdout 不一定拿得到结果，
/// 所以自检同时落一份 UTF-8 文本文件（放在可执行文件旁边），方便脚本或 CI 读取。
void WriteSelfCheckLog(const wxString& report, int exitCode)
{
    const wxFileName exe(wxStandardPaths::Get().GetExecutablePath());
    const wxString path = exe.GetPath() + wxFILE_SEP_PATH + "selfcheck.log";

    std::FILE* fp = _wfopen(path.wc_str(), L"wb");
    if (fp == nullptr)
        return;

    std::fputs("========== SiliFlow core self-check ==========\n", fp);
    std::fputs(report.utf8_str(), fp);
    std::fprintf(fp, "exit code = %d\n", exitCode);
    std::fclose(fp);
}

}  // namespace

class SiliFlowApp : public wxApp
{
public:
    // -------------------------------------------------------------------------
    //  命令行
    //  wxApp::OnInit() 内部会自己解析命令行，未声明的选项会让它直接返回 false，
    //  所以 --selfcheck 必须在这里登记成 wx 认识的开关。
    // -------------------------------------------------------------------------
    void OnInitCmdLine(wxCmdLineParser& parser) override
    {
        wxApp::OnInitCmdLine(parser);  // 保留 wx 内建的 -h / --help
        parser.AddSwitch("s", "selfcheck", "只跑 core 数据层自检，打印结果后退出");
    }

    bool OnCmdLineParsed(wxCmdLineParser& parser) override
    {
        if (!wxApp::OnCmdLineParsed(parser))
            return false;

        m_selfCheck = parser.Found("selfcheck");
        return true;
    }

    bool OnInit() override
    {
        // 必须排在所有中文字面量之前：把 wxString 的窄字符转换切成 UTF-8。
        sf::UseUtf8NarrowStrings();

        if (!wxApp::OnInit())
            return false;

        // 自检不需要窗口。真正执行放在 OnRun，这样退出码能带回给调用方。
        if (m_selfCheck)
            return true;

        sf::MainFrame* frame = new sf::MainFrame("SiliFlow");
        frame->Centre(wxBOTH);
        frame->Show(true);
        SetTopWindow(frame);
        return true;
    }

    int OnRun() override
    {
        if (!m_selfCheck)
            return wxApp::OnRun();

        wxString report;
        const int code = sf::RunCoreSelfCheck(&report);

        std::printf("========== SiliFlow core self-check ==========\n");
        std::printf("%s\n", static_cast<const char*>(report.utf8_str()));
        std::printf("exit code = %d\n", code);
        std::fflush(stdout);

        WriteSelfCheckLog(report, code);

        return code;
    }

private:
    bool m_selfCheck = false;
};

wxIMPLEMENT_APP(SiliFlowApp);
