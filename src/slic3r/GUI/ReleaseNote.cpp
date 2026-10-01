#include "ReleaseNote.hpp"
#include "I18N.hpp"

#include "libslic3r/Utils.hpp"
#include "libslic3r/Thread.hpp"
#include "GUI.hpp"
#include "GUI_App.hpp"
#include "GUI_Utils.hpp"
#include "GUI_Preview.hpp"
#include "MainFrame.hpp"
#include "format.hpp"
#include "Widgets/ProgressDialog.hpp"
#include "Widgets/RoundedRectangle.hpp"
#include "Widgets/StaticBox.hpp"
#include "Widgets/WebView.hpp"
#include "Jobs/BoostThreadWorker.hpp"
#include "Jobs/PlaterWorker.hpp"

#include <wx/regex.h>
#include <wx/progdlg.h>
#include <wx/clipbrd.h>
#include <wx/dcgraph.h>
#include <miniz.h>
#include <algorithm>
#include "Plater.hpp"
#include "BitmapCache.hpp"
#include "slic3r/GUI/GUI_App.hpp"

#include "DeviceCore/DevManager.h"
#include "DeviceCore/DevStorage.h"
#include "md4c/src/md4c-html.h"

namespace Slic3r { namespace GUI {

wxDEFINE_EVENT(EVT_SECONDARY_CHECK_DONE, wxCommandEvent);
wxDEFINE_EVENT(EVT_SECONDARY_CHECK_RESUME, wxCommandEvent);
wxDEFINE_EVENT(EVT_CHECKBOX_CHANGE, wxCommandEvent);
wxDEFINE_EVENT(EVT_SECONDARY_CHECK_RETRY, wxCommandEvent);

UpdatePluginDialog::UpdatePluginDialog(wxWindow* parent /*= nullptr*/)
    : DPIDialog(static_cast<wxWindow*>(wxGetApp().mainframe), wxID_ANY, _L("Network plug-in update"), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxCLOSE_BOX)
{
    SetBackgroundColour(*wxWHITE);
    wxBoxSizer* m_sizer_main = new wxBoxSizer(wxVERTICAL);
    auto        m_line_top = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 1));
    m_line_top->SetBackgroundColour(wxColour(166, 169, 170));
    m_sizer_main->Add(m_line_top, 0, wxEXPAND, 0);
    m_sizer_main->Add(0, 0, 0, wxTOP, FromDIP(30));

    wxBoxSizer* m_sizer_body = new wxBoxSizer(wxHORIZONTAL);



    auto sm = create_scaled_bitmap("OrcaSlicer", nullptr, 55);
    auto brand = new wxStaticBitmap(this, wxID_ANY, sm, wxDefaultPosition, wxSize(FromDIP(55), FromDIP(55)));

    wxBoxSizer* m_sizer_right = new wxBoxSizer(wxVERTICAL);

    m_text_up_info = new Label(this, Label::Head_13, wxEmptyString, LB_AUTO_WRAP);
    m_text_up_info->SetMaxSize(wxSize(FromDIP(260), -1));
    m_text_up_info->SetForegroundColour(wxColour(0x26, 0x2E, 0x30));


    operation_tips = new ::Label(this, Label::Body_12, _L("Click OK to update the Network plug-in now. If a file is in use, the update will be applied the next time Orca Slicer launches."), LB_AUTO_WRAP);
    operation_tips->SetMinSize(wxSize(FromDIP(260), -1));
    operation_tips->SetMaxSize(wxSize(FromDIP(260), -1));

    m_vebview_release_note = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL);
    m_vebview_release_note->SetScrollRate(5, FromDIP(20));
    m_vebview_release_note->SetBackgroundColour(wxColour(0xF8, 0xF8, 0xF8));
    m_vebview_release_note->SetMinSize(wxSize(FromDIP(260), FromDIP(150)));
    m_vebview_release_note->SetMaxSize(wxSize(FromDIP(260), FromDIP(150)));

    auto sizer_button = new wxBoxSizer(wxHORIZONTAL);

    auto m_button_ok = new Button(this, _L("OK"));
    m_button_ok->SetStyle(ButtonStyle::Confirm, ButtonType::Choice);

    m_button_ok->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& e) {
        EndModal(wxID_OK);
        });

    auto m_button_cancel = new Button(this, _L("Cancel"));
    m_button_cancel->SetStyle(ButtonStyle::Regular, ButtonType::Choice);

    m_button_cancel->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& e) {
        EndModal(wxID_NO);
        });

    sizer_button->AddStretchSpacer();
    sizer_button->Add(m_button_ok, 0, wxALL, FromDIP(5));
    sizer_button->Add(m_button_cancel, 0, wxALL, FromDIP(5));

    m_sizer_right->Add(m_text_up_info, 0, wxEXPAND, 0);
    m_sizer_right->Add(0, 0, 0, wxTOP, FromDIP(5));
    m_sizer_right->Add(m_vebview_release_note, 0, wxEXPAND | wxRIGHT, FromDIP(20));
    m_sizer_right->Add(0, 0, 0, wxTOP, FromDIP(5));
    m_sizer_right->Add(operation_tips, 1, wxEXPAND | wxRIGHT, FromDIP(20));
    m_sizer_right->Add(0, 0, 0, wxTOP, FromDIP(5));
    m_sizer_right->Add(sizer_button, 0, wxEXPAND | wxRIGHT, FromDIP(20));

    m_sizer_body->Add(0, 0, 0, wxLEFT, FromDIP(24));
    m_sizer_body->Add(brand, 0, wxALL, 0);
    m_sizer_body->Add(0, 0, 0, wxRIGHT, FromDIP(20));
    m_sizer_body->Add(m_sizer_right, 1, wxBOTTOM | wxEXPAND, FromDIP(18));
    m_sizer_main->Add(m_sizer_body, 0, wxEXPAND, 0);

    SetSizer(m_sizer_main);
    Layout();
    m_sizer_main->Fit(this);

    Centre(wxBOTH);
    wxGetApp().UpdateDlgDarkUI(this);
}

UpdatePluginDialog::~UpdatePluginDialog() {}


void UpdatePluginDialog::on_dpi_changed(const wxRect& suggested_rect)
{
}

void UpdatePluginDialog::update_info(std::string json_path)
{
    std::string version_str, description_str;
    wxString version;
    wxString description;

    // Parse defensively: a missing or malformed changelog must never leave the
    // dialog blank, so fall back to a generic prompt instead of returning early.
    try {
        boost::nowide::ifstream ifs(json_path);
        json j;
        ifs >> j;

        if (j.contains("version"))
            version_str = j["version"];
        if (j.contains("description"))
            description_str = j["description"];
    }
    catch (std::exception& err) {
        BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << ": parse " << json_path << " failed, reason = " << err.what();
    }

    version = from_u8(version_str);
    description = from_u8(description_str);

    if (version.IsEmpty())
        m_text_up_info->SetLabel(_L("A new Network plug-in is available. Do you want to install it?"));
    else
        m_text_up_info->SetLabel(wxString::Format(_L("A new Network plug-in (%s) is available. Do you want to install it?"), version));
    m_text_up_info->SetMinSize(wxSize(FromDIP(260), -1));
    m_text_up_info->SetMaxSize(wxSize(FromDIP(260), -1));

    if (description.IsEmpty()) {
        m_vebview_release_note->Hide();
    } else {
        wxBoxSizer* sizer_text_release_note = new wxBoxSizer(wxVERTICAL);
        auto        m_text_label            = new ::Label(m_vebview_release_note, Label::Body_13, description, LB_AUTO_WRAP);
        m_text_label->SetMinSize(wxSize(FromDIP(235), -1));
        m_text_label->SetMaxSize(wxSize(FromDIP(235), -1));

        sizer_text_release_note->Add(m_text_label, 0, wxALL, 5);
        m_vebview_release_note->SetSizer(sizer_text_release_note);
        m_vebview_release_note->Layout();
        m_vebview_release_note->Fit();
    }
    wxGetApp().UpdateDlgDarkUI(this);
    Layout();
    Fit();
}

SecondaryCheckDialog::SecondaryCheckDialog(wxWindow* parent, wxWindowID id, const wxString& title, enum VisibleButtons btn_style, const wxPoint& pos, const wxSize& size, long style, bool not_show_again_check) // ORCA VisibleButtons instead ButtonStyle 
    :DPIFrame(parent, id, title, pos, size, style)
{
    m_button_style = btn_style;

    SetBackgroundColour(*wxWHITE);
    m_sizer_main = new wxBoxSizer(wxVERTICAL);
    auto        m_line_top = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(400), 1));
    m_line_top->SetBackgroundColour(wxColour(166, 169, 170));
    m_sizer_main->Add(m_line_top, 0, wxEXPAND, 0);
    m_sizer_main->Add(0, 0, 0, wxTOP, FromDIP(5));

    wxBoxSizer* m_sizer_right = new wxBoxSizer(wxVERTICAL);

    m_sizer_right->Add(0, 0, 1, wxTOP, FromDIP(15));

    m_vebview_release_note = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL);
    m_vebview_release_note->SetScrollRate(0, FromDIP(20));
    m_vebview_release_note->SetBackgroundColour(*wxWHITE);
    m_vebview_release_note->SetMinSize(wxSize(FromDIP(400), FromDIP(380)));
    m_sizer_right->Add(m_vebview_release_note, 0, wxEXPAND | wxRIGHT | wxLEFT, FromDIP(15));


    auto bottom_sizer = new wxBoxSizer(wxVERTICAL);
    auto sizer_button = new wxBoxSizer(wxHORIZONTAL);

    if (not_show_again_check) {
        auto checkbox_sizer = new wxBoxSizer(wxHORIZONTAL);
        m_show_again_checkbox = new wxCheckBox(this, wxID_ANY, _L("Don't show again"), wxDefaultPosition, wxDefaultSize, 0);
        m_show_again_checkbox->Bind(wxEVT_COMMAND_CHECKBOX_CLICKED, [this](wxCommandEvent& e) {
            not_show_again = !not_show_again;
            m_show_again_checkbox->SetValue(not_show_again);
        });
        checkbox_sizer->Add(FromDIP(15), 0, 0, 0);
        checkbox_sizer->Add(m_show_again_checkbox, 0, wxALL, FromDIP(5));
        bottom_sizer->Add(checkbox_sizer, 0, wxBOTTOM | wxEXPAND, 0);
    }
    m_button_ok = new Button(this, _L("Confirm"));
    m_button_ok->SetStyle(ButtonStyle::Confirm, ButtonType::Choice);

    m_button_ok->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [this](wxCommandEvent& e) {
        wxCommandEvent evt(EVT_SECONDARY_CHECK_CONFIRM, GetId());
        e.SetEventObject(this);
        GetEventHandler()->ProcessEvent(evt);
        this->on_hide();
    });

    m_button_retry = new Button(this, _L("Retry"));
    m_button_retry->SetStyle(ButtonStyle::Regular, ButtonType::Choice);

    m_button_retry->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [this](wxCommandEvent& e) {
        wxCommandEvent evt(EVT_SECONDARY_CHECK_RETRY, GetId());
        e.SetEventObject(this);
        GetEventHandler()->ProcessEvent(evt);
        this->on_hide();
    });

    m_button_cancel = new Button(this, _L("Cancel"));
    m_button_cancel->SetStyle(ButtonStyle::Regular, ButtonType::Choice);

    m_button_cancel->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [this](wxCommandEvent& e) {
            wxCommandEvent evt(EVT_SECONDARY_CHECK_CANCEL);
            e.SetEventObject(this);
            GetEventHandler()->ProcessEvent(evt);
            this->on_hide();
        });

    m_button_fn = new Button(this, _L("Done"));
    m_button_fn->SetStyle(ButtonStyle::Regular, ButtonType::Choice);

    m_button_fn->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [this](wxCommandEvent& e) {
            post_event(wxCommandEvent(EVT_SECONDARY_CHECK_DONE));
            e.Skip();
        });

    m_button_resume = new Button(this, _L("Resume"));
    m_button_resume->SetStyle(ButtonStyle::Regular, ButtonType::Choice);

    m_button_resume->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [this](wxCommandEvent& e) {
        post_event(wxCommandEvent(EVT_SECONDARY_CHECK_RESUME));
        e.Skip();
        });
    m_button_resume->Hide();

    if (btn_style == CONFIRM_AND_CANCEL) {
        m_button_cancel->Show();
        m_button_fn->Hide();
        m_button_retry->Hide();
    } else if (btn_style == CONFIRM_AND_DONE) {
        m_button_cancel->Hide();
        m_button_fn->Show();
        m_button_retry->Hide();
    } else if (btn_style == CONFIRM_AND_RETRY) {
        m_button_retry->Show();
        m_button_cancel->Hide();
        m_button_fn->Hide();
    } else if (style == DONE_AND_RETRY) {
        m_button_retry->Show();
        m_button_fn->Show();
        m_button_cancel->Hide();
    }
    else {
        m_button_retry->Hide();
        m_button_cancel->Hide();
        m_button_fn->Hide();
    }

    sizer_button->AddStretchSpacer();
    sizer_button->Add(m_button_resume, 0, wxALL, FromDIP(5));
    sizer_button->Add(m_button_retry, 0, wxALL, FromDIP(5));
    sizer_button->Add(m_button_fn, 0, wxALL, FromDIP(5));
    sizer_button->Add(m_button_ok, 0, wxALL, FromDIP(5));
    sizer_button->Add(m_button_cancel, 0, wxALL, FromDIP(5));
    sizer_button->Add(FromDIP(5),0, 0, 0);
    bottom_sizer->Add(sizer_button, 0, wxEXPAND | wxRIGHT | wxLEFT, 0);


    m_sizer_right->Add(bottom_sizer, 0, wxEXPAND | wxRIGHT | wxLEFT, FromDIP(15));
    m_sizer_right->Add(0, 0, 0, wxTOP,FromDIP(10));

    m_sizer_main->Add(m_sizer_right, 0, wxBOTTOM | wxEXPAND, FromDIP(5));

    Bind(wxEVT_CLOSE_WINDOW, [this](auto& e) {this->on_hide();});
    // Fix for #9874: Remove RequestUserAttention on deactivation to prevent focus stealing
    // This was causing random window activation when multiple instances were running
    // Bind(wxEVT_ACTIVATE, [this](auto& e) { if (!e.GetActive()) this->RequestUserAttention(wxUSER_ATTENTION_ERROR); });

    SetSizer(m_sizer_main);
    Layout();
    m_sizer_main->Fit(this);

    CenterOnParent();
    wxGetApp().UpdateFrameDarkUI(this);
}

void SecondaryCheckDialog::post_event(wxCommandEvent&& event)
{
    if (event_parent) {
        event.SetString("");
        event.SetEventObject(event_parent);
        wxPostEvent(event_parent, event);
        event.Skip();
    }
}

void SecondaryCheckDialog::update_text(wxString text)
{
    wxBoxSizer* sizer_text_release_note = new wxBoxSizer(wxVERTICAL);

    if (!m_staticText_release_note) {
        m_staticText_release_note = new Label(m_vebview_release_note, text, LB_AUTO_WRAP);
        wxBoxSizer* top_blank_sizer = new wxBoxSizer(wxVERTICAL);
        wxBoxSizer* bottom_blank_sizer = new wxBoxSizer(wxVERTICAL);
        top_blank_sizer->Add(FromDIP(5), 0, wxALIGN_CENTER | wxALL, FromDIP(5));
        bottom_blank_sizer->Add(FromDIP(5), 0, wxALIGN_CENTER | wxALL, FromDIP(5));

        sizer_text_release_note->Add(top_blank_sizer, 0, wxALIGN_CENTER | wxALL, FromDIP(5));
        sizer_text_release_note->Add(m_staticText_release_note, 0, wxALIGN_CENTER, FromDIP(5));
        sizer_text_release_note->Add(bottom_blank_sizer, 0, wxALIGN_CENTER | wxALL, FromDIP(5));
        m_vebview_release_note->SetSizer(sizer_text_release_note);
    }
    m_staticText_release_note->SetMaxSize(wxSize(FromDIP(330), -1));
    m_staticText_release_note->SetMinSize(wxSize(FromDIP(330), -1));
    m_staticText_release_note->SetLabelText(text);
    m_vebview_release_note->Layout();

    auto text_size = m_staticText_release_note->GetBestSize();
    if (text_size.y < FromDIP(360))
        m_vebview_release_note->SetMinSize(wxSize(FromDIP(360), text_size.y + FromDIP(25)));
    else {
        m_vebview_release_note->SetMinSize(wxSize(FromDIP(360), FromDIP(360)));
    }

    Layout();
    Fit();
}

void SecondaryCheckDialog::on_show()
{
#ifdef __APPLE__
    if (wxGetApp().mainframe && wxGetApp().mainframe->get_mac_full_screen()) {
        SetWindowStyleFlag(GetWindowStyleFlag() | wxSTAY_ON_TOP);
    }
#endif
    wxGetApp().UpdateFrameDarkUI(this);
    // recover button color
    wxMouseEvent evt_ok(wxEVT_LEFT_UP);
    m_button_ok->GetEventHandler()->ProcessEvent(evt_ok);
    wxMouseEvent evt_cancel(wxEVT_LEFT_UP);
    m_button_cancel->GetEventHandler()->ProcessEvent(evt_cancel);

    this->Show();
    this->Raise();
}

void SecondaryCheckDialog::on_hide()
{
    if (m_show_again_checkbox != nullptr && not_show_again && show_again_config_text != "")
        wxGetApp().app_config->set(show_again_config_text, "1");

    this->Hide();
    if (wxGetApp().mainframe != nullptr) {
        wxGetApp().mainframe->Show();
        wxGetApp().mainframe->Raise();
    }
}

void SecondaryCheckDialog::update_title_style(wxString title, SecondaryCheckDialog::VisibleButtons style, wxWindow* parent) // ORCA VisibleButtons instead ButtonStyle 
{
    if (m_button_style == style && title == GetTitle()) return;

    SetTitle(title);

    event_parent = parent;

    if (style == CONFIRM_AND_CANCEL) {
        m_button_cancel->Show();
        m_button_fn->Hide();
        m_button_retry->Hide();
        m_button_resume->Hide();
    }
    else if (style == CONFIRM_AND_DONE) {
        m_button_cancel->Hide();
        m_button_fn->Show();
        m_button_retry->Hide();
        m_button_resume->Hide();
    }
    else if (style == CONFIRM_AND_RETRY) {
        m_button_retry->Show();
        m_button_cancel->Hide();
        m_button_fn->Hide();
        m_button_resume->Hide();
    }
    else if (style == DONE_AND_RETRY) {
        m_button_retry->Show();
        m_button_fn->Show();
        m_button_cancel->Hide();
        m_button_resume->Hide();
    }
    else if(style == CONFIRM_AND_RESUME)
    {
        m_button_retry->Hide();
        m_button_fn->Hide();
        m_button_cancel->Hide();
        m_button_resume->Show();
    }
    else {
        m_button_retry->Hide();
        m_button_cancel->Hide();
        m_button_fn->Hide();
        m_button_resume->Hide();

    }


    Layout();
}

void SecondaryCheckDialog::update_btn_label(wxString ok_btn_text, wxString cancel_btn_text)
{
    m_button_ok->SetLabel(ok_btn_text);
    m_button_cancel->SetLabel(cancel_btn_text);
    rescale();
}

SecondaryCheckDialog::~SecondaryCheckDialog()
{

}

void SecondaryCheckDialog::on_dpi_changed(const wxRect& suggested_rect)
{
    rescale();
}

void SecondaryCheckDialog::msw_rescale() {
    wxGetApp().UpdateFrameDarkUI(this);
    Refresh();
}

void SecondaryCheckDialog::rescale()
{
    m_button_ok->Rescale();
    m_button_cancel->Rescale();
}

 SendFailedConfirm::SendFailedConfirm(wxWindow *parent /*= nullptr*/):
     DPIDialog(static_cast<wxWindow *>(wxGetApp().mainframe),
                  wxID_ANY,
                  _L("sending failed"),
                  wxDefaultPosition,
                  wxDefaultSize,
                  wxCAPTION | wxCLOSE_BOX)
 {
     SetMinSize(wxSize(FromDIP(560), -1));
     SetMaxSize(wxSize(FromDIP(560), -1));

     SetBackgroundColour(*wxWHITE);
     auto m_sizer_main    = new wxBoxSizer(wxVERTICAL);
     auto m_line_top = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(400), 1));
     m_line_top->SetBackgroundColour(wxColour(166, 169, 170));


     auto tip = new Label(this, _L("Failed to send. Click Retry to attempt sending again. If retrying does not work, please check the reason."));
     tip->Wrap(FromDIP(480));
     tip->SetMinSize(wxSize(FromDIP(480), -1));
     tip->SetMaxSize(wxSize(FromDIP(480), -1));

     wxBoxSizer *button_sizer = new wxBoxSizer(wxHORIZONTAL);


     auto m_button_retry = new Button(this, _L("Retry"));
     m_button_retry->SetStyle(ButtonStyle::Confirm, ButtonType::Choice);
     m_button_retry->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) {
         EndModal(wxYES);
     });

     auto m_button_input = new Button(this, _L("reconnect"));
     m_button_input->SetStyle(ButtonStyle::Regular, ButtonType::Choice);
     m_button_input->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) {
         EndModal(wxAPPLY);
     });

     button_sizer->Add(0, 0, 1, wxEXPAND, 5);
     button_sizer->Add(m_button_retry, 0, wxALL, 5);
     button_sizer->Add(m_button_input, 0, wxALL, 5);

     m_sizer_main->Add(m_line_top, 0, wxEXPAND, 0);
     m_sizer_main->Add(0, 0, 0, wxTOP, FromDIP(20));
     m_sizer_main->Add(tip, 0, wxEXPAND|wxLEFT|wxRIGHT, FromDIP(25));
     m_sizer_main->Add(0, 0, 0, wxTOP, FromDIP(25));
     m_sizer_main->Add(button_sizer, 0, wxEXPAND|wxRIGHT, FromDIP(25));
     m_sizer_main->Add(0, 0, 0, wxTOP, FromDIP(20));

     SetSizer(m_sizer_main);
     Layout();
     Fit();

     CentreOnParent();
     wxGetApp().UpdateDlgDarkUI(this);
 }

void SendFailedConfirm::on_dpi_changed(const wxRect &suggested_rect) {}

 }} // namespace Slic3r::GUI
