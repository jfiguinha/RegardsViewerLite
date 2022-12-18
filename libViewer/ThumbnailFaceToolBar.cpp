#include <header.h>
#ifndef __NOFACE_DETECTION__
#include "ThumbnailFaceToolBar.h"
#include <ToolbarSlide.h>
#include <LibResource.h>
#include "ListFace.h"
#include <window_id.h>
using namespace Regards::Viewer;

#define WM_REFRESHTHUMBNAIL 1023
#define WM_EXPORT 1024
#define WM_CALENDAR 1025
#define WM_GEOLOCALISE 1026

CThumbnailFaceToolBar::CThumbnailFaceToolBar(wxWindow* parent, wxWindowID id, const CThemeToolbar & theme, const bool& vertical)
	: CToolbarWindow(parent, id, theme, vertical)
{
	themeToolbar = theme;
    wxString refresh = CLibResource::LoadStringFromResource(L"LBLREFRESHTHUMBNAIL",1);//L"History";
    wxString zoomon = CLibResource::LoadStringFromResource(L"LBLZOOMON",1);
    wxString zoomoff = CLibResource::LoadStringFromResource(L"LBLZOOMOFF",1);
    wxString copy_label = CLibResource::LoadStringFromResource(L"LBLCOPY",1);

	CToolbarButton * copy = new CToolbarButton(themeToolbar.button);
	copy->SetButtonResourceId(L"IDB_MULTIPLESELECT");
	copy->SetCommandId(WM_COPY);
	copy->SetLibelleTooltip(copy_label);
	navElement.push_back(copy);

	CToolbarButton * moins = new CToolbarButton(themeToolbar.button);
	moins->SetButtonResourceId(L"IDB_ZOOMMOINS");
	moins->SetCommandId(WM_ZOOMOUT);
    moins->SetLibelleTooltip(zoomoff);
	navElement.push_back(moins);

	slide = new CToolbarSlide(themeToolbar.slider, this);
	navElement.push_back(slide);

	CToolbarButton * plus = new CToolbarButton(themeToolbar.button);
	plus->SetButtonResourceId(L"IDB_ZOOMPLUS");
	plus->SetCommandId(WM_ZOOMON);
    plus->SetLibelleTooltip(zoomon);
	navElement.push_back(plus);

	CToolbarButton* refreshButton = new CToolbarButton(themeToolbar.button);
	refreshButton->SetButtonResourceId(L"IDB_FOLDER_REFRESH");
	refreshButton->SetCommandId(WM_REFRESH);
	refreshButton->SetLibelleTooltip(refresh);
	navElement.push_back(refreshButton);

}

CThumbnailFaceToolBar::~CThumbnailFaceToolBar()
{
}

bool CThumbnailFaceToolBar::ZoomOn()
{
	if (slide != nullptr)
	{
		int dwPos = slide->GetPosition();
		dwPos++;
		if (dwPos >= slide->GetNbValue())
		{
			return false;
		}
		SetTrackBarPosition(dwPos);
		needToRefresh = true;
		
	}
    return true;
}

bool CThumbnailFaceToolBar::ZoomOff()
{
	if (slide != nullptr)
	{
		int dwPos = slide->GetPosition();
		dwPos--;
		if (dwPos < 0)
		{
			return false;
		}
		SetTrackBarPosition(dwPos);
		needToRefresh = true;
		
	}
    return true;
}

void CThumbnailFaceToolBar::SetTabValue(vector<int> value)
{
	if (slide != nullptr)
		slide->SetTabValue(value);
}

void CThumbnailFaceToolBar::SetTrackBarPosition(const int &iPos)
{
	int positionTrackBar = iPos;
	if (slide != nullptr)
	{
		slide->SetPosition(positionTrackBar);
		needToRefresh = true;
	}
}

void CThumbnailFaceToolBar::SlidePosChange(const int &position, const wxString &key)
{
	CListFace * listFace = (CListFace *)this->FindWindowById(LISTFACEID);
	if (listFace != nullptr)
	{
		wxCommandEvent evt(wxEVENT_THUMBNAILZOOMPOSITION);
		evt.SetExtraLong(position);
		listFace->GetEventHandler()->AddPendingEvent(evt);
	}
}

void CThumbnailFaceToolBar::ZoomPos(const int &position)
{
	CListFace * listFace = (CListFace *)this->FindWindowById(LISTFACEID);
	if (listFace != nullptr)
	{
		wxCommandEvent evt(wxEVENT_THUMBNAILZOOMPOSITION);
		evt.SetExtraLong(position);
		listFace->GetEventHandler()->AddPendingEvent(evt);
	}
}

void CThumbnailFaceToolBar::EventManager(const int &id)
{
	CListFace * listFace = (CListFace *)this->FindWindowById(LISTFACEID);
	if (listFace != nullptr)
	{
		switch (id)
		{
		case WM_COPY:
			{
				if (listFace != nullptr)
				{
					wxCommandEvent evt(wxEVENT_THUMBNAILMOVE);
					listFace->GetEventHandler()->AddPendingEvent(evt);
				}
			}
			break;
               
		case WM_ZOOMON:
			{
				if (ZoomOn() && listFace != nullptr)
				{
					wxCommandEvent evt(wxEVENT_THUMBNAILZOOMON);
						listFace->GetEventHandler()->AddPendingEvent(evt);
				}
			}

			break;

		case WM_ZOOMOUT:
			{
				if (ZoomOff() && listFace != nullptr)
				{
					wxCommandEvent evt(wxEVENT_THUMBNAILZOOMOFF);
					listFace->GetEventHandler()->AddPendingEvent(evt);
				}
			}

			break;
		case WM_REFRESH:
			if (listFace != nullptr)
			{
				wxCommandEvent evt(wxEVENT_THUMBNAILREFRESHFACE);
				listFace->GetEventHandler()->AddPendingEvent(evt);
			}
			break;
		}
	}
}

#endif
