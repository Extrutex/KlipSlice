#ifndef slic3r_GUI_ReleaseNote_hpp_
#define slic3r_GUI_ReleaseNote_hpp_

#include <wx/wx.h>
#include <wx/intl.h>
#include <wx/collpane.h>
#include <wx/dataview.h>
#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
#include <wx/dataview.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/string.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/hyperlink.h>
#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/popupwin.h>
#include <wx/spinctrl.h>
#include <wx/artprov.h>
#include <wx/wrapsizer.h>
#include <wx/event.h>
#include <wx/hyperlink.h>
#include <wx/richtext/richtextctrl.h>

#include "AmsMappingPopup.hpp"
#include "CommonDialogs.hpp"
#include "GUI_Utils.hpp"
#include "wxExtensions.hpp"
#include "DeviceManager.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/CheckBox.hpp"
#include "Widgets/ComboBox.hpp"
#include "Widgets/ScrolledWindow.hpp"
#include "Widgets/HyperLink.hpp"
#include <wx/hashmap.h>
#include <wx/webview.h>

#include "Jobs/Worker.hpp"

namespace Slic3r { namespace GUI {

wxDECLARE_EVENT(EVT_SECONDARY_CHECK_RETRY, wxCommandEvent);
wxDECLARE_EVENT(EVT_SECONDARY_CHECK_DONE, wxCommandEvent);
wxDECLARE_EVENT(EVT_SECONDARY_CHECK_RESUME, wxCommandEvent);

class SecondaryCheckDialog : public DPIFrame
{
private:
    wxWindow* event_parent { nullptr };
public:
    enum VisibleButtons { // ORCA VisibleButtons instead ButtonStyle 
        ONLY_CONFIRM        = 0,
        CONFIRM_AND_CANCEL  = 1,
        CONFIRM_AND_DONE    = 2,
        CONFIRM_AND_RETRY   = 3,
        CONFIRM_AND_RESUME  = 4,
        DONE_AND_RETRY      = 5,
        MAX_STYLE_NUM       = 6
    };
    SecondaryCheckDialog(
        wxWindow* parent,
        wxWindowID      id = wxID_ANY,
        const wxString& title = wxEmptyString,
        enum VisibleButtons btn_style = CONFIRM_AND_CANCEL, // ORCA VisibleButtons instead ButtonStyle 
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long            style = wxCLOSE_BOX | wxCAPTION,
        bool not_show_again_check = false
    );
    void update_text(wxString text);
    void on_show();
    void on_hide();
    void update_btn_label(wxString ok_btn_text, wxString cancel_btn_text);
    void update_title_style(wxString title, SecondaryCheckDialog::VisibleButtons style, wxWindow* parent = nullptr); // ORCA VisibleButtons instead ButtonStyle 
    void post_event(wxCommandEvent&& event);
    void rescale();
    ~SecondaryCheckDialog();
    void on_dpi_changed(const wxRect& suggested_rect);
    void msw_rescale();


    StateColor btn_bg_green;
    StateColor btn_bg_white;
    Label* m_staticText_release_note {nullptr};
    wxBoxSizer* m_sizer_main;
    wxScrolledWindow *m_vebview_release_note {nullptr};
    Button* m_button_ok { nullptr };
    Button* m_button_retry { nullptr };
    Button* m_button_cancel { nullptr };
    Button* m_button_fn { nullptr };
    Button* m_button_resume { nullptr };
    wxCheckBox* m_show_again_checkbox;
    VisibleButtons m_button_style; // ORCA VisibleButtons instead ButtonStyle 
    bool not_show_again = false;
    std::string show_again_config_text = "";
};

class SendFailedConfirm : public DPIDialog
{
public:
    SendFailedConfirm(wxWindow *parent = nullptr);
    ~SendFailedConfirm(){};

    //void on_ok(wxMouseEvent &evt);
    void on_dpi_changed(const wxRect &suggested_rect) override;
};

wxDECLARE_EVENT(EVT_CHECKBOX_CHANGE, wxCommandEvent);


}} // namespace Slic3r::GUI

#endif
