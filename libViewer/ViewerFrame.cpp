#include <header.h>
#include "ViewerFrame.h"
#include "MainWindow.h"
#include <BitmapPrintout.h>
#include "ViewerParamInit.h"
#include "SQLRemoveData.h"
#include <PrintEngine.h>
#include <LibResource.h>
//#include <OpenCLDialog.h>
#include <wx/filename.h>
#include <ConfigRegards.h>
#include "MainThemeInit.h"
#include "ViewerParam.h"
#include "MainTheme.h"
#include <RegardsConfigParam.h>
#include <ParamInit.h>
#include <FileUtility.h>
#include <SqlFindPhotos.h>
#include <SqlFindFolderCatalog.h>
#include <libPicture.h>
#include <SavePicture.h>
#include <ImageLoadingFormat.h>
#include "WaitingWindow.h"
#include <wx/stdpaths.h>
#include <SqlThumbnail.h>
#include <SqlFacePhoto.h>
#include <wx/stdpaths.h>
#include <wx/busyinfo.h>
using namespace std;
using namespace Regards::Print;
using namespace Regards::Control;
using namespace Regards::Viewer;
using namespace Regards::Sqlite;
using namespace Regards::Picture;

constexpr auto TIMER_LOADPICTURE = 2;
constexpr auto TIMER_EVENTFILEFS = 3;
constexpr auto TIMER_LOADPICTUREEND = 4;
#if !wxUSE_PRINTING_ARCHITECTURE
#error "You must set wxUSE_PRINTING_ARCHITECTURE to 1 in setup.h, and recompile the library."
#endif


#include <ctype.h>
#include "wx/metafile.h"
#include "wx/print.h"
#include "wx/printdlg.h"

#if wxUSE_POSTSCRIPT
#include "wx/generic/printps.h"
#include "wx/generic/prntdlgg.h"
#endif

#if wxUSE_GRAPHICS_CONTEXT
#include "wx/graphics.h"
#endif

#ifdef __WXMAC__
#include "wx/osx/printdlg.h"
#endif


#ifndef wxHAS_IMAGES_IN_RESOURCES
#ifdef __WXGTK__
#include "../Resource/sample.xpm"
#else
#include "../../Resource/sample.xpm"
#endif
#endif


bool CViewerFrame::viewerMode = true;

using namespace Regards::Viewer;

bool CViewerFrame::GetViewerMode()
{
	return viewerMode;
}


void CViewerFrame::SetViewerMode(const bool& mode)
{
	viewerMode = mode;
}


CViewerFrame::CViewerFrame(const wxString& title, const wxPoint& pos, const wxSize& size, IMainInterface* mainInterface,
                           const wxString& fileToOpen)
	: wxFrame(nullptr, FRAMEVIEWER_ID, title, pos, size, wxMAXIMIZE | wxDEFAULT_FRAME_STYLE), title_(title), pos_(pos),
	  size_(size), main_interface_(mainInterface), file_to_open_(fileToOpen)
{

	mainWindow = nullptr;
	fullscreen = false;
	onExit = false;
	mainWindowWaiting = nullptr;
	SetIcon(wxICON(sample));
	frameScanner = nullptr;
	viewerParam = new CMainParam();
	CMainParamInit::Initialize(viewerParam);
	Maximize();
	viewerTheme = new CMainTheme();
	CMainThemeInit::Initialize(viewerTheme);

	this->mainInterface = mainInterface;
	this->mainInterface->parent = this;

	FolderCatalogVector folderList;
	CSqlFindFolderCatalog folderCatalog;
	folderCatalog.GetFolderCatalog(&folderList, NUMCATALOGID);


	m_watcher = new wxFileSystemWatcher();
	m_watcher->SetOwner(this);
	Connect(wxEVT_FSWATCHER, wxFileSystemWatcherEventHandler(CViewerFrame::OnFileSystemModified));

	CheckDatabase(folderList);

	exitTimer = new wxTimer(this, wxTIMER_EXIT);
	Connect(wxTIMER_EXIT, wxEVT_TIMER, wxTimerEventHandler(CViewerFrame::CheckAllProcessEnd), nullptr, this);


	bool openFirstFile = true;
	//SetIcon(wxIcon(wxT("regards.xpm")));
	if (fileToOpen != "")
		openFirstFile = false;


	wxString dirpath = "";
	if (fileToOpen == "")
	{
		if (folderList.size() == 0)
		{
			dirpath = wxStandardPaths::Get().GetUserDir(wxStandardPaths::Dir_Pictures);
		}
	}


	mainWindow = new CMainWindow(this, MAINVIEWERWINDOWID, this, openFirstFile);

	//mainWindow->Show(true);
	//mainWindowWaiting->Show(false);
	//preview = nullptr;
	m_previewModality = wxPreviewFrame_AppModal;
	loadPictureTimer = new wxTimer(this, TIMER_LOADPICTURE);
	eventFileSysTimer = new wxTimer(this, TIMER_EVENTFILEFS);
	endLoadPictureTimer = new wxTimer(this, TIMER_LOADPICTUREEND);
	auto menuFile = new wxMenu;

	wxString labelDecreaseIconSize = CLibResource::LoadStringFromResource(L"labelDecreaseIconSize", 1);
	//L"Decrease Icon Size";
	wxString labelDecreaseIconSize_link = CLibResource::LoadStringFromResource(L"labelDecreaseIconSize_link", 1);
	//L"&Decrease Icon Size";
	wxString labelEnlargeIconSize = CLibResource::LoadStringFromResource(L"labelEnlargeIconSize", 1);
	//L"Enlarge Icon Size";
	wxString labelEnlargeIconSize_link = CLibResource::LoadStringFromResource(L"labelEnlargeIconSize_link", 1);
	//L"&Enlarge Icon Size";
	wxString labelConfiguration = CLibResource::LoadStringFromResource(L"labelConfiguration", 1); //L"Configuration";
	wxString labelConfiguration_link = CLibResource::LoadStringFromResource(L"labelConfiguration_link", 1);
	//L"&Configuration";
	wxString labelEraseDataBase = CLibResource::LoadStringFromResource(L"labelEraseDataBase", 1); //L"Erase Database";
	wxString labelEraseDataBase_link = CLibResource::LoadStringFromResource(L"labelEraseDataBase_link", 1);
	//L"&Erase Database";
	wxString labelThumbnailRight = CLibResource::LoadStringFromResource(L"labelThumbnailRight", 1); //L"Right Position";
	wxString labelThumbnailRight_link = CLibResource::LoadStringFromResource(L"labelThumbnailRight_link", 1);
	//L"&Right Position";
	wxString labelThumbnailBottom = CLibResource::LoadStringFromResource(L"labelThumbnailBottom", 1);
	//L"Bottom Position";
	wxString labelThumbnailBottom_link = CLibResource::LoadStringFromResource(L"labelThumbnailBottom_link", 1);
	//L"&Bottom Position";
	wxString labelPageSetup = CLibResource::LoadStringFromResource(L"labelPageSetup", 1); //L"Page setup";
	wxString labelPageSetup_link = CLibResource::LoadStringFromResource(L"labelPageSetup_link", 1); //L"Page Set&up...";
	wxString labelPageMargins = CLibResource::LoadStringFromResource(L"labelPageMargins", 1); // L"Page margins";
	wxString labelPageMargins_link = CLibResource::LoadStringFromResource(L"labelPageMargins_link", 1);
	//L"Page margins...";
	wxString labelFile = CLibResource::LoadStringFromResource(L"labelFile", 1); //L"&File";
	wxString labelParameter = CLibResource::LoadStringFromResource(L"labelParameter", 1); //L"&Parameter";
	wxString labelSizeIcon = CLibResource::LoadStringFromResource(L"labelSizeIcon", 1); //L"&Icon Size";
	wxString labelThumbnail = CLibResource::LoadStringFromResource(L"labelThumbnail", 1); //L"&Thumbnail";
	wxString labelHelp = CLibResource::LoadStringFromResource(L"labelHelp", 1); //L"&Help";

	auto menuSizeIcon = new wxMenu;
	
	menuSizeIcon->Append(ID_SIZEICONLESS, labelDecreaseIconSize_link, labelDecreaseIconSize);
	menuSizeIcon->Append(ID_SIZEICONMORE, labelEnlargeIconSize_link, labelEnlargeIconSize);

	auto menuWindowIcon = new wxMenu;
	wxString labelWindow = CLibResource::LoadStringFromResource(L"labelWindow", 1);
	wxString labelViewerMode = CLibResource::LoadStringFromResource(L"labelViewerMode", 1);
	wxString labelViewerMode_link = CLibResource::LoadStringFromResource(L"labelViewerMode_link", 1);
	wxString labelPictureMode = CLibResource::LoadStringFromResource(L"labelPictureMode", 1);
	wxString labelPictureMode_link = CLibResource::LoadStringFromResource(L"labelPictureMode_link", 1);
	menuWindowIcon->Append(ID_VIEWERMODE, labelViewerMode_link, labelViewerMode);
	menuWindowIcon->Append(ID_PICTUREMODE, labelPictureMode_link, labelPictureMode);

	menuFile->Append(ID_Configuration, labelConfiguration_link, labelConfiguration);
	menuFile->AppendSeparator();

#ifdef WIN32
	menuFile->Append(ID_ASSOCIATE, "&Associate", "Associate");
#endif

	menuFile->AppendSeparator();
	menuFile->Append(WXPRINT_PAGE_SETUP, labelPageSetup_link, labelPageSetup);
#ifdef __WXMAC__
	menuFile->Append(WXPRINT_PAGE_MARGINS, labelPageMargins_link, labelPageMargins);
#endif
	menuFile->Append(wxID_PRINT, wxT("&Print..."), wxT("Print"));
	menuFile->AppendSeparator();

	menuFile->Append(wxID_EXIT);
	auto menuHelp = new wxMenu;
	menuHelp->Append(wxID_ABOUT);
	menuHelp->Append(wxID_HELP);
	auto menuBar = new wxMenuBar;
	menuBar->Append(menuFile, labelFile);
	menuBar->Append(menuSizeIcon, labelSizeIcon);
	menuBar->Append(menuWindowIcon, labelWindow);
	menuBar->Append(menuHelp, labelHelp);
	wxFrameBase::SetMenuBar(menuBar);

	wxWindow::SetLabel(wxT("RegardsViewer Lite"));
	//Connect(wxEVT_SIZE, wxSizeEventHandler(CViewerFrame::OnSize));
	Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(CViewerFrame::OnClose));
	Connect(wxEVENT_PICTUREENDLOADING, wxCommandEventHandler(CViewerFrame::OnPictureEndLoading));
	Connect(wxID_PRINT, wxEVT_MENU, wxCommandEventHandler(CViewerFrame::OnPrint));

#ifdef WIN32
	Connect(ID_ASSOCIATE, wxEVT_MENU, wxCommandEventHandler(CViewerFrame::OnAssociate));
#endif
	//Connect(ID_SCANNER, wxEVT_MENU, wxCommandEventHandler(CViewerFrame::OnScanner));
	mainWindow->Bind(wxEVT_CHAR_HOOK, &CViewerFrame::OnKeyDown, this);
	mainWindow->Bind(wxEVT_KEY_UP, &CViewerFrame::OnKeyUp, this);

	if (fileToOpen != "")
	{
		auto file = new wxString(fileToOpen);
		wxCommandEvent evt(wxEVENT_OPENFILEORFOLDER);
		evt.SetInt(1);
		evt.SetClientData(file);
		mainWindow->GetEventHandler()->AddPendingEvent(evt);
	}
	else
	{
		if (folderList.size() == 0)
		{
			auto file = new wxString(dirpath);
			wxCommandEvent evt(wxEVENT_OPENFILEORFOLDER);
			evt.SetInt(2);
			evt.SetClientData(file);
			mainWindow->GetEventHandler()->AddPendingEvent(evt);
		}
	}

	mainInterface->HideAbout();
	
	Connect(TIMER_LOADPICTUREEND, wxEVT_TIMER, wxTimerEventHandler(CViewerFrame::OnTimerEndLoadPicture), nullptr, this);
	Connect(TIMER_LOADPICTURE, wxEVT_TIMER, wxTimerEventHandler(CViewerFrame::OnTimerLoadPicture), nullptr, this);
	Connect(TIMER_EVENTFILEFS, wxEVT_TIMER, wxTimerEventHandler(CViewerFrame::OnTimereventFileSysTimer), nullptr, this);
}

bool CViewerFrame::CheckDatabase(FolderCatalogVector& folderList)
{
	wxBusyInfo wait("Please wait, Database is checking ...");
	
	bool folderChange = false;

	//Test de la validit?? des r??pertoires
	for (CFolderCatalog folderlocal : folderList)
	{
		if (!wxDirExists(folderlocal.GetFolderPath()))
		{
			//Remove Folder
			CSQLRemoveData::DeleteFolder(folderlocal.GetNumFolder());
			folderChange = true;
		}
		else
		{
			CViewerFrame::AddFSEntry(folderlocal.GetFolderPath());
		}
	}




	//Test de la validit?? des fichiers
	PhotosVector photoList;
	CSqlThumbnail sqlThumbnail;
	CSqlFindPhotos findphotos;
	findphotos.GetAllPhotos(&photoList);
	for (CPhotos photo : photoList)
	{
		if (!wxFileExists(photo.GetPath()))
		{
			//Remove Folder
			CSQLRemoveData::DeletePhoto(photo.GetId());
			folderChange = true;
		}
	}

	//Thumbnail Photo Verification
	
	vector<int> listPhoto = sqlThumbnail.GetAllPhotoThumbnail();
	for (int numPhoto : listPhoto)
	{
		wxString thumbnail = CFileUtility::GetThumbnailPath(to_string(numPhoto));
		if (!wxFileExists(thumbnail))
		{
			sqlThumbnail.EraseThumbnail(numPhoto);
		}
	}

	CSqlFacePhoto sqlFacePhoto;
	vector<int> listFacePhoto = sqlFacePhoto.GetAllThumbnailFace();
	for (int numPhoto : listFacePhoto)
	{
		wxString thumbnail = CFileUtility::GetFaceThumbnailPath(numPhoto);
		if (!wxFileExists(thumbnail))
		{
			sqlFacePhoto.EraseFace(numPhoto);
		}
	}
	
	if (folderChange)
	{
		auto viewerParam = CMainParamInit::getInstance();
		wxString sqlRequest = viewerParam->GetLastSqlRequest();

		CSqlFindPhotos sqlFindPhotos;
		sqlFindPhotos.SearchPhotos(sqlRequest);
	}

	return folderChange;
}


#ifdef WIN32
void CViewerFrame::OnAssociate(wxCommandEvent& event)
{
	wxString path = CFileUtility::GetProgramFolderPath() + "\\associate.exe";
	ShellExecute(this->GetHWND(), L"runas", path, nullptr, nullptr, SW_SHOWNORMAL);
	//wxExecute(path, wxEXEC_SYNC);
	/*
	Association associate(this);
	associate.ShowModal();
	if (associate.IsOk())
		associate.AssociateExtension();
	*/
}
#endif
void CViewerFrame::OnPrint(wxCommandEvent& event)
{
	const wxString filename = mainWindow->GetFilename();
	if (filename != "")
	{
		CLibPicture libPicture;
		CImageLoadingFormat* image = libPicture.LoadPicture(filename);
		if (image != nullptr)
			PrintPreview(image);
	}
}

bool CViewerFrame::RemoveFSEntry(const wxString& dirPath)
{
	if (m_watcher == nullptr)
		return false;

	if (wxDirExists(dirPath) == false)
		return false;

	const wxFileName dirname(dirPath, "");
	return m_watcher->Remove(dirname);
}

bool CViewerFrame::AddFSEntry(const wxString& dirPath)
{
	if (m_watcher == nullptr)
		return false;

	if (wxDirExists(dirPath) == false)
		return false;

	const wxFileName dirname(dirPath, "");
	return m_watcher->AddTree(
		dirname, wxFSW_EVENT_CREATE | wxFSW_EVENT_DELETE | wxFSW_EVENT_RENAME | wxFSW_EVENT_MODIFY);
}

void CViewerFrame::OnClose(wxCloseEvent& event)
{
	Exit();
}

void CViewerFrame::OnTimereventFileSysTimer(wxTimerEvent& event)
{
	printf("OnFileSystemModified \n");
	const wxCommandEvent evt(wxEVT_COMMAND_TEXT_UPDATED, wxEVENT_REFRESHFOLDER);
	mainWindow->GetEventHandler()->AddPendingEvent(evt);
	eventFileSysTimer->Stop();
}


void CViewerFrame::OnHelp(wxCommandEvent& event)
{
	wxString helpFile = CFileUtility::GetResourcesFolderPath();
	helpFile.Append("//NoticeRegardsViewer.pdf");
	wxLaunchDefaultApplication(helpFile);
}

void CViewerFrame::CheckAllProcessEnd(wxTimerEvent& event)
{
	nbTime++;

	if (nbTime < 50)
	{
		for (CMasterWindow* window : CMasterWindow::listMainWindow)
		{
			if (window != nullptr)
			{
				if (!window->GetProcessEnd())
				{
					const wxString message = window->GetWaitingMessage();
					mainWindowWaiting->SetTexte(message);
					exitTimer->Start(1000, wxTIMER_ONE_SHOT);
					return;
				}
			}
		}
	}

	onExit = true;
	Exit();
}

void CViewerFrame::Exit()
{
	if (!onExit)
	{
		nbTime = 0;
		CWindowMain::SetEndProgram();
		eventFileSysTimer->Stop();
		loadPictureTimer->Stop();
		mainWindowWaiting = new CWaitingWindow(this, wxID_ANY);
		mainWindow->Show(false);
		mainWindowWaiting->Show(true);
		mainWindowWaiting->SetSize(0, 0, mainWindow->GetWindowWidth(), mainWindow->GetWindowHeight());
		mainWindowWaiting->Refresh();

		exitTimer->Start(1000, wxTIMER_ONE_SHOT);
	}
	else
	{
		CMainThemeInit::SaveTheme();
		mainInterface->Close();
		onExit = true;
	}
}

void CViewerFrame::OnTimerLoadPicture(wxTimerEvent& event)
{
	wxWindow* mainWindow = this->FindWindowById(CENTRALVIEWERWINDOWID);
	if (mainWindow != nullptr)
	{
		wxCommandEvent evt(eventToLoop);
		mainWindow->GetEventHandler()->AddPendingEvent(evt);
	}

	if (endLoadPictureTimer->IsRunning())
		endLoadPictureTimer->Stop();

	endLoadPictureTimer->Start(1000, true);
	
	//if (repeatEvent)
	//	loadPictureTimer->Start(200, true);
	//else
	loadPictureTimer->Stop();
}

void CViewerFrame::OnTimerEndLoadPicture(wxTimerEvent& event)
{
	pictureEndLoading = true;
}

void CViewerFrame::OnPictureEndLoading(wxCommandEvent& event)
{
	pictureEndLoading = true;
}

void CViewerFrame::OnKeyUp(wxKeyEvent& event)
{
	if(loadPictureTimer->IsRunning())
		loadPictureTimer->Stop();
}

void CViewerFrame::OnKeyDown(wxKeyEvent& event)
{
	if (event.m_keyCode == WXK_ESCAPE && fullscreen)
	{
		mainWindow->SetScreen();
	}
	else
	{
		switch (event.GetKeyCode())
		{
		case WXK_ESCAPE:
			{
				mainWindow->SetScreen();
			}
			break;

		case WXK_SPACE:
		case WXK_PAGEUP:
			{
				repeatEvent = true;
				eventToLoop = wxEVENT_PICTURENEXT;
				if(pictureEndLoading)
					loadPictureTimer->Start(200, true);
				pictureEndLoading = false;
			}
			break;

		case WXK_PAGEDOWN:
			{
				repeatEvent = true;
				eventToLoop = wxEVENT_PICTUREPREVIOUS;
				if (pictureEndLoading)
					loadPictureTimer->Start(200, true);
				pictureEndLoading = false;
			}
			break;


		case WXK_END:
			{
				repeatEvent = false;
				eventToLoop = wxEVENT_PICTURELAST;
				loadPictureTimer->Start(0, true);
			}
			break;

		case WXK_HOME:
			{
				repeatEvent = false;
				eventToLoop = wxEVENT_PICTUREFIRST;
				loadPictureTimer->Start(0, true);

			}
			break;

		case WXK_F3:
		{
			ViewerMode();
		}
		break;

		case WXK_F4:
		{
			PictureMode();
		}
		break;

		case WXK_F5:
			{
				if (!fullscreen)
				{
					if (mainWindow->SetFullscreen())
						fullscreen = true;
				}
			}
			break;
		default: ;
		}
	}
	event.Skip();
}


void CViewerFrame::ViewerMode()
{
	wxWindow* central = this->FindWindowById(CENTRALVIEWERWINDOWID);
	wxCommandEvent event(wxEVENT_SETMODEVIEWER);
	event.SetInt(1);
	wxPostEvent(central, event);
}

void CViewerFrame::PictureMode()
{
	wxWindow* central = this->FindWindowById(CENTRALVIEWERWINDOWID);
	wxCommandEvent event(wxEVENT_SETMODEVIEWER);
	event.SetInt(4);
	wxPostEvent(central, event);
}

void CViewerFrame::OnViewerMode(wxCommandEvent& event)
{
	ViewerMode();
}

void CViewerFrame::OnPictureMode(wxCommandEvent& event)
{
	PictureMode();
}


void CViewerFrame::SetFullscreen()
{
	if (mainWindow->SetFullscreenMode())
	{
		fullscreen = true;
		this->ShowFullScreen(true);
	}
}

void CViewerFrame::SetScreen()
{
	fullscreen = false;
	this->ShowFullScreen(false);
}

void CViewerFrame::SetWindowTitle(const wxString& libelle)
{
	SetLabel(libelle);
}

CViewerFrame::~CViewerFrame()
{
	if (mainWindow != nullptr)
		mainWindow->SaveParameter();

	if (exitTimer->IsRunning())
		exitTimer->Stop();

	delete exitTimer;

	if (loadPictureTimer->IsRunning())
		loadPictureTimer->Stop();

	delete(loadPictureTimer);

	if (eventFileSysTimer->IsRunning())
		eventFileSysTimer->Stop();

	delete(eventFileSysTimer);

	if (endLoadPictureTimer->IsRunning())
		endLoadPictureTimer->Stop();
	delete(endLoadPictureTimer);

	if (mainWindow != nullptr)
		delete(mainWindow);

	if (mainWindowWaiting != nullptr)
		delete(mainWindowWaiting);

	if (viewerTheme != nullptr)
		delete(viewerTheme);

	viewerParam->SaveFile();


	if (viewerParam != nullptr)
		delete(viewerParam);


	if (!onExit)
		CViewerFrame::Exit();
}

void CViewerFrame::SetText(const int& numPos, const wxString& libelle)
{
	if (mainWindow != nullptr)
		mainWindow->SetText(numPos, libelle);
}

void CViewerFrame::SetRangeProgressBar(const int& range)
{
	if (mainWindow != nullptr)
		mainWindow->SetRangeProgressBar(range);
}

void CViewerFrame::SetPosProgressBar(const int& position)
{
	if (mainWindow != nullptr)
		mainWindow->SetPosProgressBar(position);
}

void CViewerFrame::OnConfiguration(wxCommandEvent& event)
{
	auto regardsParam = CParamInit::getInstance();
	int pictureSize = regardsParam->GetFaceDetectionPictureSize();
	ConfigRegards configFile(this);
	configFile.ShowModal();
	if (configFile.IsOk())
	{
		const int newPictureSize = regardsParam->GetFaceDetectionPictureSize();
		if (pictureSize != newPictureSize)
		{
			//Suppression de toutes les Faces
			CSQLRemoveData::DeleteFaceDatabase();

			if (mainWindow != nullptr)
			{
				wxCommandEvent evt(wxEVENT_REFRESHFOLDERLIST);
				mainWindow->GetEventHandler()->AddPendingEvent(evt);
			}

			wxWindow* window = this->FindWindowById(CRITERIAFOLDERWINDOWID);
			if (window)
			{
				wxCommandEvent evt(wxEVT_COMMAND_TEXT_UPDATED, wxEVENT_UPDATECRITERIA);
				evt.SetExtraLong(0);
				window->GetEventHandler()->AddPendingEvent(evt);
			}
		}
	}
}


void CViewerFrame::OnFileSystemModified(wxFileSystemWatcherEvent& event)
{
	if(eventFileSysTimer != nullptr)
	{
		eventFileSysTimer->Stop();
		if (mainWindow != nullptr)
		{
			if (!mainWindow->IsVideo())
			{
				eventFileSysTimer->Start(1000);
			}
		}
	}

}

void CViewerFrame::OnIconSizeLess(wxCommandEvent& event)
{
	CRegardsConfigParam* config = CParamInit::getInstance();
	if (config != nullptr)
	{
		float ratio = config->GetIconSizeRatio();
		if (ratio > 1.0)
			ratio = ratio - 0.25;
		config->SetIconSizeRatio(ratio);

		mainWindow->UpdateScreenRatio();
		//Refresh();
	}
}

void CViewerFrame::OnIconSizeMore(wxCommandEvent& event)
{
	CRegardsConfigParam* config = CParamInit::getInstance();
	if (config != nullptr)
	{
		float ratio = config->GetIconSizeRatio();
		if (ratio < 2.0)
			ratio = ratio + 0.25;
		config->SetIconSizeRatio(ratio);

		mainWindow->UpdateScreenRatio();
		//Refresh();
	}
}

void CViewerFrame::OnExit(wxCommandEvent& event)
{
	//onExit = true;
	//mainInterface->Close();
	Exit();
}

void CViewerFrame::OnAbout(wxCommandEvent& event)
{
	mainInterface->ShowAbout();
}

void CViewerFrame::OnHello(wxCommandEvent& event)
{
	wxLogMessage("Hello world from wxWidgets!");
}

void CViewerFrame::PrintImagePreview(CImageLoadingFormat * imageToPrint)
{
	// Pass two printout objects: for preview, and possible printing.
	wxPrintData* g_printData = CPrintEngine::GetPrintData();
	wxPrintDialogData printDialogData(*g_printData);
	const auto bitmapPreview = new CImageLoadingFormat();
	*bitmapPreview = *imageToPrint;
	const auto preview = new wxPrintPreview(new CBitmapPrintout(imageToPrint), new CBitmapPrintout(bitmapPreview),
	                                        &printDialogData);
	if (!preview->IsOk())
	{
		delete preview;
		wxLogError(wxT("There was a problem previewing.\nPerhaps your current printer is not set correctly?"));
		return;
	}

	const wxString picture_print_label = CLibResource::LoadStringFromResource(L"PicturePrintPreview", 1);
	auto frame =
		new wxPreviewFrame(preview, this, picture_print_label, wxPoint(100, 100), wxSize(600, 650));
	frame->Centre(wxBOTH);
	frame->InitializeWithModality(m_previewModality);
	frame->Show();

	//delete imageToPrint;
}


void CViewerFrame::PrintPreview(CImageLoadingFormat* imageToPrint)
{
	// Pass two printout objects: for preview, and possible printing.
	wxPrintData* g_printData = CPrintEngine::GetPrintData();
	wxPrintDialogData print_dialog_data(*g_printData);
	cv::Mat bitmap_preview = imageToPrint->GetOpenCVPicture();


	const auto preview = new wxPrintPreview(new CBitmapPrintout(imageToPrint), new CBitmapPrintout(bitmap_preview),
	                                        &print_dialog_data);
	if (!preview->IsOk())
	{
		delete preview;
		wxLogError(wxT("There was a problem previewing.\nPerhaps your current printer is not set correctly?"));
		return;
	}

	const wxString picture_print_label = CLibResource::LoadStringFromResource(L"PicturePrintPreview", 1);
	auto frame =
		new wxPreviewFrame(preview, this, picture_print_label, wxPoint(100, 100), wxSize(600, 650));
	frame->Centre(wxBOTH);
	frame->InitializeWithModality(m_previewModality);
	frame->Show();

	//delete imageToPrint;
}

void CViewerFrame::OnEraseDatabase(wxCommandEvent& event)
{
	const wxString erasedatabase = CLibResource::LoadStringFromResource(L"EraseDatabase", 1);
	const wxString informations = CLibResource::LoadStringFromResource(L"labelInformations", 1);
	if (wxMessageBox(erasedatabase, informations, wxYES_NO | wxICON_WARNING) == wxYES)
	{
		CSQLRemoveData::DeleteCatalog(1);
		if (mainWindow != nullptr)
		{
			wxCommandEvent evt(wxEVT_COMMAND_TEXT_UPDATED, wxEVENT_REFRESHFOLDER);
			mainWindow->GetEventHandler()->AddPendingEvent(evt);
		}
	}
}

void CViewerFrame::OnPageSetup(wxCommandEvent& WXUNUSED(event))
{
	wxPrintData* g_printData = CPrintEngine::GetPrintData();
	wxPageSetupDialogData* g_pageSetupData = CPrintEngine::GetPageSetupDialogData();
	(*g_pageSetupData) = *g_printData;

	wxPageSetupDialog pageSetupDialog(this, g_pageSetupData);
	pageSetupDialog.ShowModal();

	(*g_printData) = pageSetupDialog.GetPageSetupDialogData().GetPrintData();
	(*g_pageSetupData) = pageSetupDialog.GetPageSetupDialogData();
}

#ifdef __WXMAC__
void CViewerFrame::OnPageMargins(wxCommandEvent& WXUNUSED(event))
{
    wxPrintData * g_printData = CPrintEngine::GetPrintData();
    wxPageSetupDialogData * g_pageSetupData = CPrintEngine::GetPageSetupDialogData();
    (*g_pageSetupData) = *g_printData;

	wxMacPageMarginsDialog pageMarginsDialog(this, g_pageSetupData);
	pageMarginsDialog.ShowModal();

	(*g_printData) = pageMarginsDialog.GetPageSetupDialogData().GetPrintData();
	(*g_pageSetupData) = pageMarginsDialog.GetPageSetupDialogData();
}
#endif

wxBEGIN_EVENT_TABLE(CViewerFrame, wxFrame)
	EVT_MENU(ID_Hello, CViewerFrame::OnHello)
	EVT_MENU(wxID_HELP, CViewerFrame::OnHelp)
	EVT_MENU(ID_Configuration, CViewerFrame::OnConfiguration)
	EVT_MENU(ID_SIZEICONLESS, CViewerFrame::OnIconSizeLess)
	EVT_MENU(ID_SIZEICONMORE, CViewerFrame::OnIconSizeMore)
	EVT_MENU(ID_VIEWERMODE, CViewerFrame::OnViewerMode)
	EVT_MENU(ID_PICTUREMODE, CViewerFrame::OnPictureMode)
	EVT_MENU(ID_ERASEDATABASE, CViewerFrame::OnEraseDatabase)
	//EVT_MENU(ID_INTERPOLATIONFILTER, CViewerFrame::OnInterpolationFilter)
	EVT_MENU(wxID_ABOUT, CViewerFrame::OnAbout)
	EVT_MENU(WXPRINT_PAGE_SETUP, CViewerFrame::OnPageSetup)
	EVT_MENU(wxID_EXIT, CViewerFrame::OnExit)
#ifdef __WXMAC__
	EVT_MENU(WXPRINT_PAGE_MARGINS, CViewerFrame::OnPageMargins)
#endif
wxEND_EVENT_TABLE()
