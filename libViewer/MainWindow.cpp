#include <header.h>
#include <ffmpeg_transcoding.h>
#include "MainWindow.h"
#include <LibResource.h>
#include "ViewerParamInit.h"
#include "ViewerParam.h"
#include <PrintEngine.h>
#include <libPicture.h>
#include "ThemeParam.h"
#include <ImageLoadingFormat.h>
#include "MainThemeInit.h"
#include "MainTheme.h"
#include "PanelInfosWnd.h"
#include "SqlFindPhotos.h"
#include <SqlThumbnail.h>
#include "PreviewWnd.h"
#include <BitmapWndViewer.h>
#include <BitmapWnd3d.h>
#include "Toolbar.h"
#include <StatusBarInterface.h>
#include "CentralWindow.h"
#include "FileUtility.h"
#include "CategoryFolderWindow.h"
#include <VideoControl_soft.h>
#include <wx/dir.h>
#include <wx/filename.h>
#include <filesystem>
#include <window_id.h>
#include <SqlFindFolderCatalog.h>
#include <SQLRemoveData.h>
#include <wx/display.h>
#include <SqlInsertFile.h>
#include "StatusText.h"
#include <ThumbnailMessage.h>
#include <SqlThumbnailVideo.h>
#include "WaitingWindow.h"
#include <ShowElement.h>
#include <wx/filedlg.h>
#include <ParamInit.h>
#include <ffplaycore.h>
#include <ConvertUtility.h>
#include "SqlFacePhoto.h"
#include <FiltreEffetCPU.h>
#include <MediaInfo.h>
using namespace Regards::Picture;
using namespace Regards::Control;
using namespace Regards::Viewer;
using namespace std;
using namespace Regards::Sqlite;

bool firstTime = true;

class CThreadMD5
{
public:
	CThreadMD5()
	{
		thread = nullptr;
		mainWindow = nullptr;
	}

	~CThreadMD5()
	{
	}

	wxString filename;
	std::thread* thread;
	wxWindow* mainWindow;
};

class CFolderFiles
{
public:
	vector<wxString> pictureFiles;
	wxString folderName;
};

class CThreadVideoData
{
public:
	CThreadVideoData()
	{
		mainWindow = nullptr;
	}

	~CThreadVideoData();

	CMainWindow* mainWindow;
	wxString video;
};

CThreadVideoData::~CThreadVideoData()
{
}


wxDEFINE_EVENT(wxEVENT_SETSCREEN, wxCommandEvent);

CMainWindow::CMainWindow(wxWindow* parent, wxWindowID id, IStatusBarInterface* statusbar, const bool& openFirstFile)
	: CWindowMain("CMainWindow", parent, id)
{
	fullscreen = false;
	nbProcessMD5 = 0;
	showToolbar = true;
	multithread = true;
	needToReload = false;
	typeAffichage = THUMB_SHOW_ALL;
	updateCriteria = true;
	updateFolder = true;
	refreshFolder = false;
	numElementTraitement = 0;
	start = true;
	criteriaSendMessage = false;
	checkVersion = true;


	CMainTheme* viewerTheme = CMainThemeInit::getInstance();
	viewerParam = CMainParamInit::getInstance();

	if (viewerTheme != nullptr)
	{
		CThemeSplitter theme;
		viewerTheme->GetSplitterTheme(&theme);
		centralWnd = new CCentralWindow(this, CENTRALVIEWERWINDOWID, theme, false);
	}
	this->statusBarViewer = statusbar;

	/*----------------------------------------------------------------------
	 *
	 * Manage Event
	 *
	 ----------------------------------------------------------------------*/

	Connect(wxEVENT_SETSCREEN, wxCommandEventHandler(CMainWindow::SetScreenEvent));
	Connect(wxEVENT_INFOS, wxCommandEventHandler(CMainWindow::OnUpdateInfos));
	Connect(wxEVENT_REFRESHFOLDERLIST, wxCommandEventHandler(CMainWindow::RefreshFolderList));
	Connect(TOOLBAR_UPDATE_ID, wxCommandEventHandler(CMainWindow::OnShowToolbar));
	Connect(wxEVT_IDLE, wxIdleEventHandler(CMainWindow::OnIdle));
	Connect(wxEVENT_UPDATEFOLDER, wxCommandEventHandler(CMainWindow::OnUpdateFolder));
	Connect(wxEVENT_ONPICTURECLICK, wxCommandEventHandler(CMainWindow::OnPictureClick));
	Connect(wxEVT_CRITERIACHANGE, wxCommandEventHandler(CMainWindow::CriteriaChange));
	Connect(wxEVENT_PICTUREVIDEOCLICK, wxCommandEventHandler(CMainWindow::PictureVideoClick));
	Connect(wxEVENT_REFRESHFOLDER, wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler(CMainWindow::InitPictures));
	Connect(wxEVENT_REFRESHPICTURE, wxCommandEventHandler(CMainWindow::OnRefreshPicture));
	Connect(wxEVENT_MD5CHECKING, wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler(CMainWindow::Md5Checking));
	Connect(wxEVENT_SETSTATUSTEXT, wxCommandEventHandler(CMainWindow::OnStatusSetText));
	Connect(wxEVT_EXIT, wxCommandEventHandler(CMainWindow::OnExit));
	Connect(wxEVENT_SETRANGEPROGRESSBAR, wxCommandEventHandler(CMainWindow::OnSetRangeProgressBar));
	Connect(wxEVENT_PRINTPICTURE, wxCommandEventHandler(CMainWindow::PrintPreview));

	Connect(wxEVENT_UPDATESTATUSBARMESSAGE, wxCommandEventHandler(CMainWindow::UpdateStatusBarMessage));

	Connect(wxEVENT_PRINT, wxCommandEventHandler(CMainWindow::OnPrint));
	Connect(wxEVENT_SETVALUEPROGRESSBAR, wxCommandEventHandler(CMainWindow::OnSetValueProgressBar));
	Connect(wxEVENT_ENDVIDEOTHUMBNAIL, wxCommandEventHandler(CMainWindow::OnEndThumbnail));
	Connect(wxEVENT_OPENFILEORFOLDER, wxCommandEventHandler(CMainWindow::OnOpenFileOrFolder));
	Connect(wxEVENT_UPDATETHUMBNAILEXIF, wxCommandEventHandler(CMainWindow::OnUpdateExifThumbnail));


	/*----------------------------------------------------------------------
	 *
	 * Manage Event
	 *
	 ----------------------------------------------------------------------*/

	statusBar = new wxStatusBar(this, wxID_ANY, wxSTB_DEFAULT_STYLE, "wxStatusBar");

	int tabWidth[] = { 100, 300, 300, 300 };
	statusBar->SetFieldsCount(4);
	statusBar->SetStatusWidths(4, tabWidth);

	progressBar = new wxGauge(statusBar, wxID_ANY, 200, wxPoint(1000, 0), wxSize(200, statusBar->GetSize().y),
		wxGA_HORIZONTAL);
	progressBar->SetRange(100);
	progressBar->SetValue(50);
	refreshFolder = true;
	processIdle = true;
	updateFolder = true;
	listProcessWindow.push_back(this);
	CMainParam* config = CMainParamInit::getInstance();
	if (config != nullptr)
		firstFileToShow = localFilename = config->GetLastShowPicture();
}


bool CMainWindow::IsVideo()
{
	if (centralWnd != nullptr)
		return centralWnd->IsVideo();

	return false;
}



void CMainWindow::OnUpdateExifThumbnail(wxCommandEvent& event)
{
	int numPhoto = event.GetInt();
	int rotate = event.GetExtraLong();

	wxString thumbnail = CFileUtility::GetThumbnailPath(to_string(numPhoto));
	CFiltreEffetCPU::LoadAndRotate(thumbnail, rotate);
	wxWindow* window = this->FindWindowById(THUMBNAILVIEWERPICTURE);
	if (window != nullptr)
	{
		wxCommandEvent evt(wxEVENT_REFRESHTHUMBNAIL);
		evt.SetInt(numPhoto);
		window->GetEventHandler()->AddPendingEvent(evt);
	}
}




void CMainWindow::OnPrint(wxCommandEvent& event)
{
	bool showPrintPicture = true;
	if (centralWnd->IsVideo())
	{
		auto video = static_cast<CShowElement*>(this->FindWindowById(SHOWBITMAPVIEWERID));
		if (video != nullptr)
		{
			if (video->IsPause())
			{
				cv::Mat image = video->GetVideoBitmap();
				if (!image.empty())
				{
					CImageLoadingFormat* imageLoading = new CImageLoadingFormat();
					imageLoading->SetPicture(image);
					statusBarViewer->PrintImagePreview(imageLoading);
					showPrintPicture = false;
				}
			}
		}
	}
	if (showPrintPicture)
	{
		CLibPicture libPicture;
		CImageLoadingFormat* image = libPicture.LoadPicture(localFilename);
		if (image != nullptr)
		{
			statusBarViewer->PrintPreview(image);
		}
	}
}


void CMainWindow::SetDataToStatusBar(void* thumbMessage, const wxString& picture)
{
	const auto thumbnailMessage = static_cast<CThumbnailMessage*>(thumbMessage);
	const wxString message = picture + to_string(thumbnailMessage->nbPhoto);

	if (thumbnailMessage != nullptr)
	{
		if (statusBarViewer != nullptr)
		{
			statusBarViewer->SetRangeProgressBar(thumbnailMessage->nbElement);
			statusBarViewer->SetText(2, message);
			statusBarViewer->SetPosProgressBar(thumbnailMessage->thumbnailPos + 1);
		}
		delete thumbnailMessage;
	}
}

//---------------------------------------------------------------
//
//---------------------------------------------------------------
void CMainWindow::UpdateStatusBarMessage(wxCommandEvent& event)
{
	
	const auto thumbnailMessage = static_cast<CThumbnailMessage*>(event.GetClientData());
	if (thumbnailMessage != nullptr)
	{
		const int typeMessage = thumbnailMessage->typeMessage;

		switch (typeMessage)
		{
		case 0:
		{
			const wxString picture = CLibResource::LoadStringFromResource(L"LBLCRITERIANBIMAGE", 1);
			SetDataToStatusBar(event.GetClientData(), picture);
		}
		break;

		case 1:
		{
			if (statusBarViewer != nullptr)
			{
				statusBarViewer->SetRangeProgressBar(thumbnailMessage->nbElement);
				statusBarViewer->SetPosProgressBar(0);
			}
			delete thumbnailMessage;
		}
		break;

		case 2:
		{
			const wxString picture = CLibResource::LoadStringFromResource(L"LBLFOLDERPROCESSING", 1);
			SetDataToStatusBar(event.GetClientData(), picture);
		}
		break;

		case 3:
		{
			
			const wxString picture = CLibResource::LoadStringFromResource(L"LBLPICTURERENDER", 1);
			SetDataToStatusBar(event.GetClientData(), picture);
		}
		break;

		case 4:
		{
			
			const wxString picture = CLibResource::LoadStringFromResource(L"LBLFACEPROCESS", 1);
			SetDataToStatusBar(event.GetClientData(), picture);
		}
		break;

		case 5:
		{
			
			const wxString picture = "Face Recognition progress : ";
			SetDataToStatusBar(event.GetClientData(), picture);
		}
		break;
		default:;
		}
	}
}

//---------------------------------------------------------------
//
//---------------------------------------------------------------
void CMainWindow::PrintPreview(wxCommandEvent& event)
{
	const auto bitmap = static_cast<CImageLoadingFormat*>(event.GetClientData());

	if (bitmap != nullptr)
	{
		statusBarViewer->PrintImagePreview(bitmap);
	}
}

//---------------------------------------------------------------
//
//---------------------------------------------------------------
void CMainWindow::RefreshFolderList(wxCommandEvent& event)
{
	
	updateFolder = true;
	processIdle = true;
}


//---------------------------------------------------------------
//
//---------------------------------------------------------------
wxString CMainWindow::GetFilename()
{
	return localFilename;
}

//---------------------------------------------------------------
//
//---------------------------------------------------------------
void CMainWindow::CriteriaChange(wxCommandEvent& event)
{
	
	//Refresh Criteria
	wxWindow* window = FindWindowById(CRITERIAFOLDERWINDOWID);
	if (window)
	{
		wxCommandEvent evt(wxEVT_COMMAND_TEXT_UPDATED, wxEVENT_UPDATECRITERIA);
		evt.SetExtraLong(1);
		window->GetEventHandler()->AddPendingEvent(evt);
	}

	processIdle = true;
}

//---------------------------------------------------------------
//
//---------------------------------------------------------------
void CMainWindow::OnShowToolbar(wxCommandEvent& event)
{
	
	ShowToolbar();
}

//---------------------------------------------------------------
//
//---------------------------------------------------------------
void CMainWindow::UpdateScreenRatio()
{
	centralWnd->UpdateScreenRatio();
	this->Resize();
}

//---------------------------------------------------------------
//
//---------------------------------------------------------------
void CMainWindow::OnStatusSetText(wxCommandEvent& event)
{
	
	auto statusText = static_cast<CStatusText*>(event.GetClientData());
	if (statusText != nullptr)
	{
		statusBar->SetStatusText(statusText->text, statusText->position);
		delete statusText;
	}
}

//---------------------------------------------------------------
//
//---------------------------------------------------------------
void CMainWindow::OnSetValueProgressBar(wxCommandEvent& event)
{
	

	int position = event.GetInt();
	//cout << "OnSetValueProgressBar Pos : " << position << endl;
	if (progressBar != nullptr)
	{
		if (progressBar->GetRange() > 0)
		{
			if (position >= progressBar->GetRange())
				progressBar->SetValue(progressBar->GetRange() - 1);
			else
				progressBar->SetValue(position);

			progressBar->Refresh();
		}
	}
}

//---------------------------------------------------------------
//
//---------------------------------------------------------------
void CMainWindow::OnSetRangeProgressBar(wxCommandEvent& event)
{
	
	int range = event.GetInt();
	// cout << "OnSetRangeProgressBar Pos : " << range << endl;
	if (progressBar != nullptr)
		progressBar->SetRange(range);
}

//---------------------------------------------------------------
//
//---------------------------------------------------------------
void CMainWindow::SetText(const int& numPos, const wxString& libelle)
{
	
	auto event = new wxCommandEvent(wxEVENT_SETSTATUSTEXT);
	auto statusText = new CStatusText();
	statusText->position = numPos;
	statusText->text = libelle;
	event->SetClientData(statusText);
	wxQueueEvent(this, event);

	//statusBar->SetStatusText(libelle, numPos);
}

//---------------------------------------------------------------
//
//---------------------------------------------------------------
void CMainWindow::SetRangeProgressBar(const int& range)
{
	
	auto event = new wxCommandEvent(wxEVENT_SETRANGEPROGRESSBAR);
	event->SetInt(range);
	wxQueueEvent(this, event);
}

//---------------------------------------------------------------
//
//---------------------------------------------------------------
void CMainWindow::SetPosProgressBar(const int& position)
{
	
	auto event = new wxCommandEvent(wxEVENT_SETVALUEPROGRESSBAR);
	event->SetInt(position);
	wxQueueEvent(this, event);
}

//---------------------------------------------------------------
//
//---------------------------------------------------------------
bool CMainWindow::FindNextValidFile()
{
	bool isFound = false;
	wxString lastFile = centralWnd->ImageFin(false);
	do
	{
		auto p = std::find_if(
			pictures.begin(), pictures.end(),
			[&](const auto& val)
			{
				auto photo = static_cast<CPhotos>(val);
				return photo.GetPath() == localFilename;
			}
		);

		if (p != pictures.end())
			isFound = true;

		if (!isFound)
		{
			if (lastFile == localFilename)
				break;

			localFilename = centralWnd->ImageSuivante(false);
		}
	} while (!isFound);

	return isFound;
}

//---------------------------------------------------------------
//
//---------------------------------------------------------------
bool CMainWindow::FindPreviousValidFile()
{
	bool isFound = false;
	wxString firstFile = centralWnd->ImageDebut(false);
	do
	{
		auto p = std::find_if(
			pictures.begin(), pictures.end(),
			[&](const auto& val)
			{
				auto photo = static_cast<CPhotos>(val);
				return photo.GetPath() == localFilename;
			}
		);

		if (p != pictures.end())
			isFound = true;

		if (!isFound)
		{
			if (firstFile == localFilename)
				break;

			localFilename = centralWnd->ImagePrecedente(false);
		}
	} while (!isFound);

	return isFound;
}

//---------------------------------------------------------------
//
//---------------------------------------------------------------
void CMainWindow::UpdateCriteria()
{
	wxWindow* window = this->FindWindowById(CRITERIAFOLDERWINDOWID);
	if (window)
	{
		wxCommandEvent evt(wxEVT_COMMAND_TEXT_UPDATED, wxEVENT_UPDATECRITERIA);
		evt.SetExtraLong((criteriaSendMessage == true) ? 1 : 0);
		window->GetEventHandler()->AddPendingEvent(evt);
	}
	criteriaSendMessage = false;
	updateCriteria = false;
}

//---------------------------------------------------------------
//
//---------------------------------------------------------------
void CMainWindow::RefreshFolder()
{
	FolderCatalogVector folderList;
	CSqlFindFolderCatalog folderCatalog;
	folderCatalog.GetFolderCatalog(&folderList, NUMCATALOGID);

	bool folderChange = false;

	//Test de la validité des répertoires
	for (CFolderCatalog folderlocal : folderList)
	{
		if (!wxDirExists(folderlocal.GetFolderPath()))
		{
			//Remove Folder
			CSQLRemoveData::DeleteFolder(folderlocal.GetNumFolder());
			folderChange = true;
		}
	}

	//Test de la validité des fichiers
	PhotosVector photoList;
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

	int nbFile = 0;

	for (CFolderCatalog folderlocal : folderList)
	{
		wxString fichier;
		CSqlInsertFile sqlInsertFile;
		nbFile += sqlInsertFile.ImportFileFromFolder(folderlocal.GetFolderPath(), folderlocal.GetNumFolder(),
			fichier);
	}

	if (folderChange || nbFile > 0)
	{
		auto viewerParam = CMainParamInit::getInstance();
		wxString sqlRequest = viewerParam->GetLastSqlRequest();

		CSqlFindPhotos sqlFindPhotos;
		sqlFindPhotos.SearchPhotos(sqlRequest);

		updateFolder = true;
		updateCriteria = true;
	}
	refreshFolder = false;
	numElementTraitement = 0;
}

//---------------------------------------------------------------
//
//---------------------------------------------------------------
void CMainWindow::UpdateFolder()
{
	wxString requestSql = "";
	pictures.clear();
	CSqlFindPhotos sqlFindPhotos;
	if (firstFileToShow != "")
		localFilename = firstFileToShow;
	else
		localFilename = centralWnd->GetFilename();
	auto categoryFolder = static_cast<CCategoryFolderWindow*>(this->FindWindowById(
		CATEGORYFOLDERWINDOWID));
	if (categoryFolder != nullptr)
		requestSql = categoryFolder->GetSqlRequest();

	if (requestSql != "")
	{
		sqlFindPhotos.SearchPhotos(requestSql);
		sqlFindPhotos.SearchPhotosByTypeAffichage(&pictures, typeAffichage, NUMCATALOGID);
	}
	else
		sqlFindPhotos.SearchPhotos(&pictures);

	bool isFound = false;

	if (!isFound && pictures.size() > 0 && localFilename != "")
	{
		isFound = FindNextValidFile();
		if (!isFound)
			isFound = FindPreviousValidFile();
	}

	if (!isFound && !pictures.empty())
		localFilename = pictures[0].GetPath();

	centralWnd->SetListeFile(localFilename);

	updateFolder = false;

	firstFileToShow = "";
	numElementTraitement = 0;
}

//---------------------------------------------------------------
//
//---------------------------------------------------------------
void CMainWindow::PhotoProcess(CPhotos* photo)
{
	int nbProcesseur = 1;

	if (wxFileName::FileExists(photo->GetPath()))
	{
		//Test si thumbnail valide
		CMainParam* config = CMainParamInit::getInstance();
		if (config != nullptr)
		{
			if (config->GetCheckThumbnailValidity() && nbProcessMD5 < nbProcesseur)
			{
				auto path = new CThreadMD5();
				path->filename = photo->GetPath();
				path->mainWindow = this;
				path->thread = new thread(CheckMD5, path);
				nbProcessMD5++;
			}
			else
				numElementTraitement++;
		}
	}
	else
	{
		//Remove file
		CSQLRemoveData::DeletePhoto(photo->GetId());
		updateCriteria = true;
		updateFolder = true;
		numElementTraitement++;
	}

	wxString label = CLibResource::LoadStringFromResource(L"LBLFILECHECKING", 1);
	wxString message = label + to_string(numElementTraitement) + L"/" + to_string(pictures.size());
	if (statusBarViewer != nullptr)
	{
		statusBarViewer->SetText(3, message);
	}
}

//---------------------------------------------------------------
//
//---------------------------------------------------------------
void CMainWindow::ProcessIdle()
{
	
	bool hasDoneOneThings = false;
	//int nbProcesseur = 1;

	if (updateCriteria)
	{
		UpdateCriteria();
		hasDoneOneThings = true;
	}
	else if (refreshFolder)
	{
		RefreshFolder();
		hasDoneOneThings = true;
	}
	else if (updateFolder)
	{
		UpdateFolder();
		hasDoneOneThings = true;
	}
	else if (numElementTraitement < pictures.size())
	{
		PhotoProcess(&pictures[numElementTraitement]);
		hasDoneOneThings = true;
	}

	if (setViewerMode)
	{
		setViewerMode = false;
		wxWindow* central = this->FindWindowById(CENTRALVIEWERWINDOWID);
		if (central != nullptr)
		{
			wxCommandEvent event(wxEVENT_SETMODEVIEWER);
			event.SetInt(4);
			wxPostEvent(central, event);
		}
	}

	if (!hasDoneOneThings)
		processIdle = false;
}

//---------------------------------------------------------------
//
//---------------------------------------------------------------
void CMainWindow::OnIdle(wxIdleEvent& evt)
{
    if(needToRefresh)
    {
        this->Refresh();
        needToRefresh = false;
    }  
    
	StartThread();
}

//---------------------------------------------------------------
//
//---------------------------------------------------------------
void CMainWindow::Md5Checking(wxCommandEvent& event)
{
	
	auto path = static_cast<CThreadMD5*>(event.GetClientData());
	if (path != nullptr)
	{
		if (path->thread != nullptr)
		{
			path->thread->join();
			delete(path->thread);
			path->thread = nullptr;
		}
		delete path;
	}
	nbProcessMD5--;
	numElementTraitement++;
	wxString label = CLibResource::LoadStringFromResource(L"LBLFILECHECKING", 1);
	wxString message = label + to_string(numElementTraitement) + L"/" + to_string(pictures.size());
	if (statusBarViewer != nullptr)
	{
		statusBarViewer->SetText(3, message);
	}
}

//---------------------------------------------------------------
//
//---------------------------------------------------------------
void CMainWindow::CheckMD5(void* param)
{
	
	auto path = static_cast<CThreadMD5*>(param);
	if (path != nullptr)
	{
		CSqlThumbnail sqlThumbnail;
		CSqlThumbnailVideo sqlThumbnailVideo;
		wxFileName file(path->filename);
		wxULongLong sizeFile = file.GetSize();
		wxString md5file = sizeFile.ToString();

		bool result = sqlThumbnail.TestThumbnail(path->filename, md5file);
		if (!result)
		{
			//Remove thumbnail
			sqlThumbnail.DeleteThumbnail(path->filename);
			sqlThumbnailVideo.DeleteThumbnail(path->filename);
		}

		wxCommandEvent evt(wxEVT_COMMAND_TEXT_UPDATED, wxEVENT_MD5CHECKING);
		evt.SetClientData(path);
		path->mainWindow->GetEventHandler()->AddPendingEvent(evt);
	}
}

//---------------------------------------------------------------
//
//---------------------------------------------------------------
CMainWindow::~CMainWindow()
{
	delete(centralWnd);
}

//---------------------------------------------------------------
//
//---------------------------------------------------------------
void CMainWindow::SaveParameter()
{
	if (centralWnd != nullptr)
		centralWnd->SaveParameter();
}

void CMainWindow::Resize()
{
	
	if (!fullscreen)
	{
		wxRect rcAffichageBitmap;
		wxSize sizeStatusBar = statusBar->GetSize();

		rcAffichageBitmap.x = 0;
		rcAffichageBitmap.y = 0;
		rcAffichageBitmap.width = GetWindowWidth();
		rcAffichageBitmap.height = GetWindowHeight() - sizeStatusBar.y;

		centralWnd->SetSize(rcAffichageBitmap.x, rcAffichageBitmap.y, rcAffichageBitmap.width,
			rcAffichageBitmap.height);
		centralWnd->Refresh();
		statusBar->SetSize(rcAffichageBitmap.x, rcAffichageBitmap.y + rcAffichageBitmap.height, rcAffichageBitmap.width,
			sizeStatusBar.y);
		statusBar->Refresh();
	}
	else
	{
		centralWnd->SetSize(0, 0, GetWindowWidth(), GetWindowHeight());
		centralWnd->Refresh();
	}
}

void CMainWindow::PictureVideoClick(wxCommandEvent& event)
{
	
	const long timePosition = event.GetExtraLong();
	if (centralWnd != nullptr)
	{
		centralWnd->SetPosition(timePosition);
	}
}


void CMainWindow::OnPictureClick(wxCommandEvent& event)
{
	
	const int photoId = event.GetExtraLong();
	wxString filename = "";
	for (CPhotos photo : pictures)
	{
		if (photo.GetId() == photoId)
		{
			filename = photo.GetPath();
			break;
		}
	}

	centralWnd->LoadPicture(filename);
}

void CMainWindow::OnUpdateFolder(wxCommandEvent& event)
{
	int typeId = event.GetInt();

	if (typeId == 0)
	{
		const auto newPath = static_cast<wxString*>(event.GetClientData());
		if (newPath != nullptr)
		{
			firstFileToShow = *newPath;
			delete newPath;
		}
	}


	updateCriteria = true;
	updateFolder = true;
	processIdle = true;
	this->Show(true);
}

void CMainWindow::TransitionEnd()
{
	
	centralWnd->TransitionEnd();

	if (!centralWnd->IsDiaporamaStart())
	{
		auto showBitmap = static_cast<CShowElement*>(this->FindWindowById(SHOWBITMAPVIEWERID));
		if (showBitmap != nullptr)
		{
			showBitmap->TransitionEnd();
		}
	}
}

void CMainWindow::OnUpdateInfos(wxCommandEvent& event)
{
	

#if defined(WIN32) && defined(_DEBUG)
	OutputDebugString(L"CMainWindow::OnUpdateInfos");
	OutputDebugString(L"\n");
#endif

	auto pictureInfos = static_cast<CPictureInfosMessage*>(event.GetClientData());
	if (pictureInfos != nullptr)
	{
		wxString filename = pictureInfos->filename;
		if (filename[0] != '\0')
		{
			statusBarViewer->SetText(1, filename);
		}

		statusBarViewer->SetText(0, pictureInfos->infos);

		wxString label = CLibResource::LoadStringFromResource(L"LBLUPDATEINFOS", 1);
		wxString infos = label + CFileUtility::GetFileName(filename);
		statusBarViewer->SetWindowTitle(infos);

		delete pictureInfos;
	}
}

bool CMainWindow::GetProcessEnd()
{
	
	if (nbProcessMD5 > 0)
		return false;

	return true;
}

void CMainWindow::OnEndThumbnail(wxCommandEvent& event)
{
	localFilename = centralWnd->GetFilename();
	auto thumbName = static_cast<wxString*>(event.GetClientData());
	if (*thumbName == localFilename)
		centralWnd->OnEndThumbnail();
	delete thumbName;
}

void CMainWindow::OnExit(wxCommandEvent& event)
{
	
	statusBarViewer->Exit();
}

void CMainWindow::OnOpenFileOrFolder(wxCommandEvent& event)
{
	auto file = static_cast<wxString*>(event.GetClientData());
	if (file != nullptr)
	{
		int type = event.GetInt();
		if (type == 1)
			OpenFile(*file);
		else
			OpenFolder(*file);

		wxWindow* window = this->FindWindowById(CRITERIAFOLDERWINDOWID);
		if (window != nullptr)
		{
			wxCommandEvent evt(wxEVENT_SELCHANGED);
			evt.SetExtraLong(1);
			evt.SetInt(1);
			window->GetEventHandler()->AddPendingEvent(evt);
		}

		delete file;
	}
}

void CMainWindow::OpenFile(const wxString& fileToOpen)
{
	FolderCatalogVector folderList;
	CSqlFindFolderCatalog folderCatalog;
	folderCatalog.GetFolderCatalog(&folderList, NUMCATALOGID);
	bool find = false;
	wxFileName filename(fileToOpen);
	wxString folder = filename.GetPath();

	for (CFolderCatalog folderlocal : folderList)
	{
		if (folder == folderlocal.GetFolderPath())
		{
			find = true;
			break;
		}
	}

	if (!find)
	{
		OpenFolder(folder);
	}

	updateCriteria = true;
	updateFolder = true;
	processIdle = true;
	setViewerMode = true;

	centralWnd->LoadPicture(fileToOpen);
}

////////////////////////////////////////////////////////////////////////////////////////////////
///Gestion des événements du menu
////////////////////////////////////////////////////////////////////////////////////////////////
bool CMainWindow::OpenFolder(const wxString& path)
{
	
	if (wxDirExists(path))
	{
		if (viewerParam != nullptr)
			viewerParam->SetCatalogCriteria("");

		wxWindow* window = this->FindWindowById(CRITERIAFOLDERWINDOWID);
		if (window != nullptr)
		{
			auto newFolder = new wxString(path);
			wxCommandEvent evt(wxEVENT_SETFOLDER);
			evt.SetClientData(newFolder);
			window->GetEventHandler()->AddPendingEvent(evt);
		}
	}

	return true;
}

bool CMainWindow::IsFullscreen()
{
	
	return fullscreen;
}

void CMainWindow::InitPictures(wxCommandEvent& event)
{
	
	printf("InitPictures \n");
	refreshFolder = true;
	processIdle = true;
}

void CMainWindow::OnRefreshPicture(wxCommandEvent& event)
{
	localFilename = centralWnd->GetFilename();
	centralWnd->LoadPicture(localFilename, true);
}


void CMainWindow::ShowToolbar()
{
	
	showToolbar = !showToolbar;
	if (centralWnd != nullptr)
	{
		if (!showToolbar)
			centralWnd->HideToolbar();
		else
			centralWnd->ShowToolbar();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////
///Affichage en mode plein écran
////////////////////////////////////////////////////////////////////////////////////////////////
bool CMainWindow::SetFullscreen()
{
	
	const bool work = centralWnd->IsCompatibleFullscreen();
	if (work)
		statusBarViewer->SetFullscreen();
	return work;
}

void CMainWindow::SetScreenEvent(wxCommandEvent& event)
{
	
	this->Resize();
}

bool CMainWindow::SetFullscreenMode()
{
	bool is_work = false;
	
	if (!fullscreen)
	{
		if (centralWnd->FullscreenMode())
		{
			is_work = true;
			fullscreen = true;
			statusBar->Show(false);
			wxCommandEvent event(wxEVENT_SETSCREEN);
			wxPostEvent(this, event);
		}
	}
	return is_work;
}

bool CMainWindow::SetScreen()
{
	bool isWork = false;
	
	if (fullscreen)
	{
		if (centralWnd->ScreenMode())
		{
			statusBarViewer->SetScreen();
			fullscreen = false;
			statusBar->Show(true);
			isWork = true;
			wxCommandEvent event(wxEVENT_SETSCREEN);
			wxPostEvent(this, event);
		}
	}
	return isWork;
}