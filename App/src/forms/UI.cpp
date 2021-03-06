///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 3.10.1-0-g8feb16b)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "UI.h"

///////////////////////////////////////////////////////////////////////////

UILoginFrame::UILoginFrame( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxFrame( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );

	wxBoxSizer* container_sizer_;
	container_sizer_ = new wxBoxSizer( wxVERTICAL );

	m_panel1 = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bSizer41;
	bSizer41 = new wxBoxSizer( wxVERTICAL );


	bSizer41->Add( 0, 0, 1, wxEXPAND, 5 );

	logo_ = new wxStaticBitmap( m_panel1, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	bSizer41->Add( logo_, 0, wxALL|wxEXPAND, 5 );

	wxBoxSizer* action_sizer_;
	action_sizer_ = new wxBoxSizer( wxVERTICAL );


	action_sizer_->Add( 0, 0, 1, wxEXPAND, 5 );

	wxFlexGridSizer* credentials_sizer_;
	credentials_sizer_ = new wxFlexGridSizer( 2, 2, 0, 0 );
	credentials_sizer_->AddGrowableCol( 1 );
	credentials_sizer_->SetFlexibleDirection( wxBOTH );
	credentials_sizer_->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

	email_label_ = new wxStaticText( m_panel1, wxID_ANY, wxT("E-Mail Adresse:"), wxDefaultPosition, wxDefaultSize, 0 );
	email_label_->Wrap( -1 );
	credentials_sizer_->Add( email_label_, 0, wxALL, 5 );

	email_entry_ = new wxTextCtrl( m_panel1, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	credentials_sizer_->Add( email_entry_, 0, wxALL|wxEXPAND, 5 );

	password_label_ = new wxStaticText( m_panel1, wxID_ANY, wxT("Passwort"), wxDefaultPosition, wxDefaultSize, 0 );
	password_label_->Wrap( -1 );
	credentials_sizer_->Add( password_label_, 0, wxALL, 5 );

	password_entry_ = new wxTextCtrl( m_panel1, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD );
	credentials_sizer_->Add( password_entry_, 0, wxALL|wxEXPAND, 5 );


	action_sizer_->Add( credentials_sizer_, 1, wxEXPAND, 5 );

	error_message_ = new wxStaticText( m_panel1, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER_HORIZONTAL );
	error_message_->Wrap( -1 );
	error_message_->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	error_message_->SetForegroundColour( wxColour( 251, 2, 7 ) );

	action_sizer_->Add( error_message_, 0, wxALL|wxEXPAND, 5 );

	login_button_ = new wxButton( m_panel1, wxID_ANY, wxT("Anmelden"), wxDefaultPosition, wxDefaultSize, 0 );

	login_button_->SetDefault();
	action_sizer_->Add( login_button_, 0, wxALIGN_CENTER|wxALL, 5 );


	action_sizer_->Add( 0, 0, 1, wxEXPAND, 5 );

	wxBoxSizer* bSizer4;
	bSizer4 = new wxBoxSizer( wxHORIZONTAL );

	signup_hint_ = new wxStaticText( m_panel1, wxID_ANY, wxT("Noch keinen Account?"), wxDefaultPosition, wxDefaultSize, 0 );
	signup_hint_->Wrap( -1 );
	bSizer4->Add( signup_hint_, 0, wxALL, 5 );

	signup_link_ = new wxHyperlinkCtrl( m_panel1, wxID_ANY, wxT("Registriere Dich hier"), wxT("https://dev.dstage.org/account/signup"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );

	signup_link_->SetHoverColour( wxColour( 87, 121, 217 ) );
	signup_link_->SetNormalColour( wxColour( 242, 5, 68 ) );
	signup_link_->SetVisitedColour( wxColour( 242, 5, 68 ) );
	signup_link_->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, true, wxEmptyString ) );
	signup_link_->SetForegroundColour( wxColour( 235, 0, 53 ) );

	bSizer4->Add( signup_link_, 0, wxALL, 5 );


	action_sizer_->Add( bSizer4, 1, wxALIGN_CENTER|wxALL, 5 );


	bSizer41->Add( action_sizer_, 1, wxEXPAND, 5 );


	m_panel1->SetSizer( bSizer41 );
	m_panel1->Layout();
	bSizer41->Fit( m_panel1 );
	container_sizer_->Add( m_panel1, 1, wxEXPAND | wxALL, 5 );


	this->SetSizer( container_sizer_ );
	this->Layout();
	m_menubar1 = new wxMenuBar( 0 );
	this->SetMenuBar( m_menubar1 );


	this->Centre( wxBOTH );

	// Connect Events
	login_button_->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( UILoginFrame::onLogin ), NULL, this );
}

UILoginFrame::~UILoginFrame()
{
	// Disconnect Events
	login_button_->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( UILoginFrame::onLogin ), NULL, this );

}
