///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 3.10.1-0-g8feb16b)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "UI.h"

///////////////////////////////////////////////////////////////////////////

UILoginDialog::UILoginDialog( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );

	wxBoxSizer* container_sizer_;
	container_sizer_ = new wxBoxSizer( wxVERTICAL );


	container_sizer_->Add( 0, 0, 1, wxEXPAND, 5 );

  logo_ = new wxStaticBitmap( this, wxID_ANY, wxBITMAP_PNG("logo-full.png"), wxDefaultPosition, wxDefaultSize, 0 );
	container_sizer_->Add( logo_, 0, wxALL|wxEXPAND, 5 );


	container_sizer_->Add( 0, 0, 1, wxEXPAND, 5 );

	wxFlexGridSizer* credentials_sizer_;
	credentials_sizer_ = new wxFlexGridSizer( 2, 2, 0, 0 );
	credentials_sizer_->AddGrowableCol( 1 );
	credentials_sizer_->SetFlexibleDirection( wxBOTH );
	credentials_sizer_->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

	email_label_ = new wxStaticText( this, wxID_ANY, wxT("E-Mail Adresse:"), wxDefaultPosition, wxDefaultSize, 0 );
	email_label_->Wrap( -1 );
	credentials_sizer_->Add( email_label_, 0, wxALL, 5 );

	email_entry_ = new wxTextCtrl( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	credentials_sizer_->Add( email_entry_, 0, wxALL|wxEXPAND, 5 );

	password_label_ = new wxStaticText( this, wxID_ANY, wxT("Passwort"), wxDefaultPosition, wxDefaultSize, 0 );
	password_label_->Wrap( -1 );
	credentials_sizer_->Add( password_label_, 0, wxALL, 5 );

	password_entry_ = new wxTextCtrl( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	credentials_sizer_->Add( password_entry_, 0, wxALL|wxEXPAND, 5 );


	container_sizer_->Add( credentials_sizer_, 1, wxEXPAND, 5 );

	wxBoxSizer* action_sizer_;
	action_sizer_ = new wxBoxSizer( wxVERTICAL );

	error_message_ = new wxStaticText( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER_HORIZONTAL );
	error_message_->Wrap( -1 );
	error_message_->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxEmptyString ) );
	error_message_->SetForegroundColour( wxColour( 251, 2, 7 ) );

	action_sizer_->Add( error_message_, 0, wxALL|wxEXPAND, 5 );

	wxBoxSizer* bSizer4;
	bSizer4 = new wxBoxSizer( wxHORIZONTAL );

	signup_hint_ = new wxStaticText( this, wxID_ANY, wxT("Noch keinen Account?"), wxDefaultPosition, wxDefaultSize, 0 );
	signup_hint_->Wrap( -1 );
	bSizer4->Add( signup_hint_, 0, wxALL, 5 );

	signup_link_ = new wxHyperlinkCtrl( this, wxID_ANY, wxT("Registriere Dich hier"), wxT("https://dev.dstage.org/account/signup"), wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE );
	signup_link_->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, true, wxEmptyString ) );
	signup_link_->SetForegroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNTEXT ) );

	bSizer4->Add( signup_link_, 0, wxALL, 5 );


	action_sizer_->Add( bSizer4, 1, wxEXPAND, 5 );

	login_button_ = new wxButton( this, wxID_ANY, wxT("Anmelden"), wxDefaultPosition, wxDefaultSize, 0 );

	login_button_->SetDefault();
	action_sizer_->Add( login_button_, 0, wxALIGN_CENTER|wxALL, 5 );


	container_sizer_->Add( action_sizer_, 1, wxEXPAND, 5 );


	container_sizer_->Add( 0, 0, 1, wxEXPAND, 5 );


	this->SetSizer( container_sizer_ );
	this->Layout();

	this->Centre( wxBOTH );

	// Connect Events
	login_button_->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( UILoginDialog::onLogin ), NULL, this );
}

UILoginDialog::~UILoginDialog()
{
	// Disconnect Events
	login_button_->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( UILoginDialog::onLogin ), NULL, this );

}
