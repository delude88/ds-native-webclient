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
#include <wx/panel.h>
#include <wx/menu.h>
#include <wx/frame.h>

///////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
/// Class UILoginFrame
///////////////////////////////////////////////////////////////////////////////
class UILoginFrame : public wxFrame
{
	private:

	protected:
		wxPanel* m_panel1;
		wxStaticBitmap* logo_;
		wxStaticText* email_label_;
		wxTextCtrl* email_entry_;
		wxStaticText* password_label_;
		wxTextCtrl* password_entry_;
		wxStaticText* error_message_;
		wxStaticText* signup_hint_;
		wxHyperlinkCtrl* signup_link_;
		wxButton* login_button_;
		wxMenuBar* m_menubar1;

		// Virtual event handlers, override them in your derived class
		virtual void onLogin( wxCommandEvent& event ) { event.Skip(); }


	public:

		UILoginFrame( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxT("Digital Stage Connector"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 500,305 ), long style = wxCAPTION|wxCLOSE_BOX|wxFRAME_NO_TASKBAR|wxSTAY_ON_TOP|wxTAB_TRAVERSAL );

		~UILoginFrame();

};

