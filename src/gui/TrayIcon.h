//
// Created by Tobias Hegemann on 04.11.21.
//
#pragma once
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include "wx/taskbar.h"
class TrayIcon : wxTaskBarIcon {
#if defined(__WXOSX__) && wxOSX_USE_COCOA
  MyTaskBarIcon(wxTaskBarIconType iconType = wxTBI_DEFAULT_TYPE)
    :   wxTaskBarIcon(iconType)
#else
  MyTaskBarIcon()
#endif
  {}

  wxDECLARE_EVENT_TABLE();
};
