///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 3.10.1-0-g8feb16b)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#pragma once

#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/statbmp.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/string.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/sizer.h>
#include <wx/hyperlink.h>
#include <wx/button.h>
#include <wx/dialog.h>

///////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
/// Class UILoginDialog
///////////////////////////////////////////////////////////////////////////////
class UILoginDialog : public wxDialog
{
	private:

	protected:
		wxStaticBitmap* logo_;
		wxStaticText* email_label_;
		wxTextCtrl* email_entry_;
		wxStaticText* password_label_;
		wxTextCtrl* password_entry_;
		wxStaticText* error_message_;
		wxStaticText* signup_hint_;
		wxHyperlinkCtrl* signup_link_;
		wxButton* login_button_;

		// Virtual event handlers, override them in your derived class
		virtual void onLogin( wxCommandEvent& event ) { event.Skip(); }


	public:

		UILoginDialog( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxT("Anmeldung"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 428,280 ), long style = wxDEFAULT_DIALOG_STYLE );

		~UILoginDialog();

};

