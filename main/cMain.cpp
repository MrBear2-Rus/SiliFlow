#include <wx/wx.h>

#include "MainFrame.h"

class SiliFlowApp : public wxApp
{
public:
    bool OnInit() override
    {
        if (!wxApp::OnInit())
            return false;

        MainFrame* frame = new MainFrame("SiliFlow");
        frame->Centre(wxBOTH);
        frame->Show(true);
        SetTopWindow(frame);
        return true;
    }
};

wxIMPLEMENT_APP(SiliFlowApp);
