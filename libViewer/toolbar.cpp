#include <header.h>
#include "Toolbar.h"
#include <ToolbarButton.h>
#include <LibResource.h>
#include "ViewerFrame.h"
#include <window_id.h>
#include <RegardsConfigParam.h>
#include <ParamInit.h>
#include "CheckVersion.h"
#include <wx/mimetype.h>
using namespace Regards::Window;
using namespace Regards::Viewer;
using namespace Regards::Internet;

//
#define IDM_WINDOWSEARCH 152
#define IDM_THUMBNAILFACE 153
#define IDM_VIEWERMODE 154
#define IDM_EXPLORERMODE 155
#define IDM_SHOWINFOS 156
#define IDM_QUITTER 157
#define IDM_PRINT 158
#define IDM_SCANNER 159
#define IDM_PICTUREMODE 160
#define IDM_EDIT 161
#define IDM_EXPORT 162
#define IDM_NEWVERSION 163
#define IDM_EXPORT_DIAPORAMA 164

CToolbar::CToolbar(wxWindow* parent, wxWindowID id, const CThemeToolbar & theme, const bool& vertical)
	: CToolbarWindow(parent,id,theme, vertical)
{
	wxString lblOpenFolder = CLibResource::LoadStringFromResource(L"LBLSELECTFILE",1);
	wxString lblInfos = CLibResource::LoadStringFromResource(L"LBLINFOS", 1);
	wxString lblQuit = CLibResource::LoadStringFromResource(L"LBLQUIT", 1);
	wxString lblPrint = CLibResource::LoadStringFromResource(L"LBLPRINT", 1);

	wxString lblViewerMode = CLibResource::LoadStringFromResource(L"LBLVIEWERMODE", 1);
	wxString lblPictureMode = CLibResource::LoadStringFromResource(L"LBLPICTUREMODE", 1);
	wxString lblEditor = CLibResource::LoadStringFromResource(L"LBLEDITORMODE", 1);
	wxString lblNewVersion = CLibResource::LoadStringFromResource(L"LBLUPDATE", 1);

  
	CToolbarButton * viewer = new CToolbarButton(themeToolbar.button);
	viewer->SetButtonResourceId(L"IDB_THUMBNAILPNG");
	viewer->SetLibelle(lblViewerMode);
	viewer->SetCommandId(IDM_VIEWERMODE);
	navElement.push_back(viewer);

	CToolbarButton * picture = new CToolbarButton(themeToolbar.button);
	picture->SetButtonResourceId(L"IDB_PHOTOVIEWERPNG");
	picture->SetLibelle(lblPictureMode);
	picture->SetCommandId(IDM_PICTUREMODE);
	navElement.push_back(picture);

	if (NewVersionAvailable())
	{
		CToolbarButton* imageNewVersion = new CToolbarButton(themeToolbar.button);
		imageNewVersion->SetButtonResourceId(L"IDB_REFRESH");
		imageNewVersion->SetLibelle(lblNewVersion);
		imageNewVersion->SetCommandId(IDM_NEWVERSION);
		navElement.push_back(imageNewVersion);
	}

	CToolbarButton * imageFirst = new CToolbarButton(themeToolbar.button);
	imageFirst->SetButtonResourceId(L"IDB_EXIT");
	imageFirst->SetLibelle(lblQuit);
	imageFirst->SetCommandId(IDM_QUITTER);
	navElement.push_back(imageFirst);
}

CToolbar::~CToolbar()
{
}

bool CToolbar::NewVersionAvailable()
{
	wxString localVersion = CLibResource::LoadStringFromResource("REGARDSVERSION", 1);
	wxString serverURL = CLibResource::LoadStringFromResource("ADRESSEWEBVERSION", 1);
	CCheckVersion _checkVersion(serverURL);
	wxString serverVersion = _checkVersion.GetLastVersion();
	serverVersion = serverVersion.SubString(0, serverVersion.length() - 2);

	long localValueVersion;
	long localServerVersion;

	localVersion.Replace(".", "");
	serverVersion.Replace(".", "");

	if (!localVersion.ToLong(&localValueVersion)) { /* error! */ }
	if (!serverVersion.ToLong(&localServerVersion)) { /* error! */ }


	if (serverVersion != "error" && serverVersion != "")
	{
		if (localValueVersion < localServerVersion)
		{
			return true;
		}
	}
	return false;

}
void CToolbar::EventManager(const int &id)
{
	switch (id)
	{
                         
    case IDM_VIEWERMODE:
		{
			wxWindow* central = this->FindWindowById(CENTRALVIEWERWINDOWID);
			wxCommandEvent event(wxEVENT_SETMODEVIEWER);
			event.SetInt(1);
			wxPostEvent(central, event);
		}
	    break;
                
	case IDM_PICTUREMODE:
	{
		wxWindow* central = this->FindWindowById(CENTRALVIEWERWINDOWID);
		wxCommandEvent event(wxEVENT_SETMODEVIEWER);
		event.SetInt(4);
		wxPostEvent(central, event);
	}
	break;

	case IDM_QUITTER:
		{
			wxWindow* central = this->FindWindowById(MAINVIEWERWINDOWID);
			wxCommandEvent* event = new wxCommandEvent(wxEVT_EXIT);
			wxQueueEvent(central, event);
		}
		break;
		
	case IDM_NEWVERSION:
		{
			wxString siteweb = CLibResource::LoadStringFromResource("SITEWEB", 1);
			wxMimeTypesManager manager;
			wxFileType* filetype = manager.GetFileTypeFromExtension("html");
			wxString command = filetype->GetOpenCommand(siteweb);
			wxExecute(command);
		}
		break;
	}
}