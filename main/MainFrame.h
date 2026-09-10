#pragma once

#include <wx/frame.h>

class MainFrame : public wxFrame
{
public:
    MainFrame(const wxString& title);
    ~MainFrame() override = default;

private:
    void OnHello(wxCommandEvent& event);
    void OnExit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
};
