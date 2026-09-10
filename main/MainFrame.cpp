#include "MainFrame.h"

#include <wx/aboutdlg.h>
#include <wx/button.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>

namespace
{
enum
{
    ID_Hello = 1,
};
}

MainFrame::MainFrame(const wxString& title)
    : wxFrame(nullptr, wxID_ANY, title)
{
    wxMenu* menuFile = new wxMenu;
    menuFile->Append(ID_Hello, "&Hello...\tCtrl-H",
        "Help string shown in status bar for this menu item");
    menuFile->AppendSeparator();
    menuFile->Append(wxID_EXIT);

    wxMenu* menuHelp = new wxMenu;
    menuHelp->Append(wxID_ABOUT);

    wxMenuBar* menuBar = new wxMenuBar;
    menuBar->Append(menuFile, "&File");
    menuBar->Append(menuHelp, "&Help");

    SetMenuBar(menuBar);
    CreateStatusBar();
    SetStatusText("Welcome to SiliFlow!");

    wxPanel* panel = new wxPanel(this, wxID_ANY);
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    wxButton* btn = new wxButton(panel, wxID_ANY, "Press me");
    sizer->AddStretchSpacer();
    sizer->Add(btn, 0, wxALIGN_CENTER | wxALL, 12);
    sizer->AddStretchSpacer();
    panel->SetSizer(sizer);

    btn->Bind(wxEVT_BUTTON, &MainFrame::OnHello, this);

    Bind(wxEVT_MENU, &MainFrame::OnHello, this, ID_Hello);
    Bind(wxEVT_MENU, &MainFrame::OnExit, this, wxID_EXIT);
    Bind(wxEVT_MENU, &MainFrame::OnAbout, this, wxID_ABOUT);
}

void MainFrame::OnExit(wxCommandEvent& WXUNUSED(event))
{
    Close(true);
}

void MainFrame::OnAbout(wxCommandEvent& WXUNUSED(event))
{
    wxAboutDialogInfo info;
    info.SetName("SiliFlow");
    info.SetVersion("0.1.0");
    info.SetDescription(_("SiliFlow wxWidgets skeleton\n"
        "wxWidgets framework is configured to match SigFlow_FPGA.\n"
        "Build with VS2026 (Visual Studio Version 18), Toolset v145."));
    info.SetCopyright("(C) 2026");
    wxAboutBox(info);
}

void MainFrame::OnHello(wxCommandEvent& WXUNUSED(event))
{
    wxLogMessage("Hello world from SiliFlow");
}
