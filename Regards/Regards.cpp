/////////////////////////////////////////////////////////////////////////////
// Name:        minimal.cpp
// Purpose:     Minimal wxWidgets sample
// Author:      Julian Smart
// Modified by:
// Created:     04/01/98
// RCS-ID:      $Id$
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "header.h"
#include <Regards.h>
#include <wx/window.h>
#include "SqlInit.h"
#include <libPicture.h>
#include <ConvertUtility.h>
#include <LoadingResource.h>
#include <LibResource.h>
#include <FileUtility.h>
#include <ParamInit.h>
#include <FilterWindowParam.h>
#include <FilterData.h>
//#include <PictureData.h>

#ifdef TEST
#include <ffmpeg_application.h>
#endif

bool isOpenCLInitialized = false;

using namespace cv;
using namespace Regards::Picture;


void MyApp::OnInitCmdLine(wxCmdLineParser& parser)
{
	parser.SetDesc(g_cmdLineDesc);
	// must refuse '/' as parameter starter or cannot use "/path" style paths
	parser.SetSwitchChars(wxT("-"));
}

bool MyApp::OnCmdLineParsed(wxCmdLineParser& parser)
{
	// to get at your unnamed parameters use
	wxArrayString files;
	for (auto i = 0; i < parser.GetParamCount(); i++)
	{
		//printf("Files to show : %s \n", CConvertUtility::ConvertToUTF8(parser.GetParam(i)));
		files.Add(parser.GetParam(i));
		break;
	}

	// and other command line parameters
	if (files.Count() > 0)
		fileToOpen = files[0];
	// then do what you need with them.

	return true;
}

// ----------------------------------------------------------------------------
// the application class
// ----------------------------------------------------------------------------
#ifdef __APPLE__
void MyApp::MacOpenFile(const wxString &fileName)
{
    wxString message = "Mac Open Files : " + fileName;
    fileToOpen = fileName;
	//wxMessageBox(message);
	//wxMessageBox(fileName);
	//frameViewer->OpenFile(fileName);
}
#endif


int MyApp::Close()
{
  
	CSqlInit::KillSqlEngine();
	CPrintEngine::Kill();

	CLibResource::KillSqlEngine();

	if (regardsParam != nullptr)
		regardsParam->SaveFile();

	if (frameStart != nullptr)
		frameStart->Destroy();


	sqlite3_shutdown();
#ifdef USECURL
	curl_global_cleanup();
#endif
	this->Exit();

	CWindowMain::listMainWindow.clear();

	CLibPicture::Uninitx265Decoder();

	CLibPicture::UninitFreeImage();


#ifdef __WXMSW__


	CoUninitialize();

#endif

#if not defined(WIN32) && defined(LIBBPG)
    CLibPicture::UnloadBpgDll();
#endif

#ifdef FFMPEG
	avformat_network_deinit();
	//av_lockmgr_register(nullptr);
#endif

#if defined(__WXMSW__) && defined(_DEBUG)
	_CrtDumpMemoryLeaks();
#endif

	return 0;
}


// 'Main program' equivalent: the program execution "starts" here
bool MyApp::OnInit()
{
	std::set_terminate(onTerminate);

	// call the base class initialization method, currently it only parses a
	// few common command-line options but it could be do more in the future
	if (!wxApp::OnInit())
		return false;


#ifdef __APPLE__
	wxSystemOptions::SetOption(wxOSX_FILEDIALOG_ALWAYS_SHOW_TYPES, 1);
#endif
	 
	int retCode = sqlite3_config(SQLITE_CONFIG_SERIALIZED);
	if (retCode == SQLITE_OK)
	{
		printf("SQLite in serialized mode \n");
	}

#ifdef WIN32
	LCID lcid = GetThreadLocale();
	wchar_t name[LOCALE_NAME_MAX_LENGTH];
	if (LCIDToLocaleName(lcid, name, LOCALE_NAME_MAX_LENGTH, 0) == 0)
		printf("%s", GetLastError());
	std::wcout << L"Locale name = " << name << std::endl;

	char buffer[64];
	int ret;
	ret = wcstombs(buffer, name, sizeof(buffer));
	setlocale(LC_ALL, buffer);
	
#elif __APPLE__
    std::locale::global(std::locale(""));

#else

	//std::locale loc;
	//string locName = loc.name();
	//setlocale(LC_ALL, locName.c_str());

#endif

#ifdef __APPLE__
	setlocale(LC_NUMERIC, "en_US.UTF-8");
#elif defined(WIN32)
    std::setlocale(LC_NUMERIC, "en_US.UTF-8");
#endif

	sqlite3_initialize();

	wxInitAllImageHandlers();

	CLibPicture::InitFreeImage();

	wxSocketBase::Initialize();

	CPrintEngine::Initialize();
	wxString resourcePath = CFileUtility::GetResourcesFolderPath();
	wxString documentPath = CFileUtility::GetDocumentFolderPath();
	CLibPicture::Initx265Decoder();

#ifdef GLUT
#ifndef __APPLE__
	int argc = 1;
	char* argv[1] = { wxString((wxTheApp->argv)[0]).char_str() };
	glutInit(&argc, argv);
#endif
#endif

#if not defined(WIN32) && defined(LIBBPG)
    printf("LoadBpgDll\n");
    CLibPicture::LoadBpgDll();
#endif

#ifndef NDEBUG
    ::wxMessageBox("toto");
#endif

	//task_scheduler_init init;
	//int n = tbb::task_scheduler_init::default_num_threads();
	bool configFileExist = CParamInit::IsConfigFileExist();

	//Chargement des param??tres de l'application
	regardsParam = new CRegardsConfigParam();
	CParamInit::Initialize(regardsParam);


	bool dataInMemory = regardsParam->GetDatabaseInMemory();

	CLibResource::InitializeSQLServerDatabase(resourcePath);
	CSqlInit::InitializeSQLServerDatabase(documentPath, dataInMemory);

	CFilterWindowParam::InitFilterOpenCLCompatible();

	//Chargement des ressources
	wxXmlResource::Get()->InitAllHandlers();

	CFiltreData::CreateFilterList();

	if (!configFileExist)
	{
		if (!cv::ocl::haveOpenCL())
			regardsParam->SetIsOpenCLSupport(false);
		else
			regardsParam->SetIsOpenCLSupport(true);		
	}


	if (regardsParam->GetIsOpenCLSupport() && !regardsParam->GetIsOpenCLOpenGLInteropSupport())
	{
		if (!cv::ocl::haveOpenCL())
		{
			cout << "OpenCL is not avaiable..." << endl;
		}
		else
		{

			cv::ocl::Context context;
			if (!context.create(cv::ocl::Device::TYPE_GPU))
				isOpenCLInitialized = false;
			else
				isOpenCLInitialized = true;

			if (!isOpenCLInitialized)
			{
				if (!context.create(cv::ocl::Device::TYPE_CPU))
					isOpenCLInitialized = false;
				else
					isOpenCLInitialized = true;
			}

			cout << context.ndevices() << " GPU devices are detected." << endl; //This bit provides an overview of the OpenCL devices you have in your computer
			for (int i = 0; i < context.ndevices(); i++)
			{
				cv::ocl::Device device = context.device(i);
#if defined(WIN32)
				char message[255];
				sprintf(message, "name: % s \n", device.name().c_str());
				OutputDebugStringA(message);
				sprintf(message,"OpenCL_C_Version: % s \n", device.OpenCL_C_Version().c_str());
				OutputDebugStringA(message);
#else
				
				cout << "name:              " << device.name() << endl;
				cout << "available:         " << device.available() << endl;
				cout << "imageSupport:      " << device.imageSupport() << endl;
				cout << "OpenCL_C_Version:  " << device.OpenCL_C_Version() << endl;
				cout << endl;
#endif


			}

			if (isOpenCLInitialized)
			{
				cv::ocl::Device(context.device(1));
			}
		}

		if (!isOpenCLInitialized)
		{
			regardsParam->SetIsOpenCLSupport(false);
		}
	}


#ifdef WIN32
	wxString numIdLang = "\\" + to_string(regardsParam->GetNumLanguage()) + "\\msw";
#else
#ifdef __APPLE__
	wxString numIdLang = "/" + to_string(regardsParam->GetNumLanguage()) + "/osx";
#else
    wxString numIdLang = "/" + to_string(regardsParam->GetNumLanguage()) + "/linux";
#endif
#endif
	wxXmlResource::Get()->LoadAllFiles(resourcePath + numIdLang);


	frameStart = new MyFrameIntro("Welcome to RegardsViewer Lite", "RegardsViewer Lite", wxPoint(50, 50), wxSize(450, 340), this);
	frameStart->Centre(wxBOTH);
	frameStart->Show(true);

	CViewerFrame::SetViewerMode(true);


	// success: wxApp::OnRun() will be called which will enter the main message
	// loop and the application will run. If we returned false here, the
	// application would exit immediately.
	return true;
}


// Get the file extension filter for all registered image handlers.
//
wxString MyApp::GetImageFilter()
{
	return _("All image files") + wxString(wxT("|"))
		+ m_strImageFilter + wxT("|")
		+ m_strImageFilterList
		+ _("All files") + wxT("|*.*");
}


// Image handler and extension filter handling.
//
// Adds image handlers to the image handler list.
//
void MyApp::AddImageHandler(wxImageHandler* poHandler)
{
	if (poHandler)
	{
		wxString strExtension = poHandler->GetExtension();

		// TODO: Is there a posibility to get files by extension fully
		//       case insensitive?
		//
#ifdef __WXMSW__
		m_strImageFilter += wxT("*.") + strExtension + wxT(";");
		m_strImageFilterList += poHandler->GetMimeType() + wxT(" (*.")
			+ strExtension
			+ wxT(")|*.") + strExtension + wxT("|");
#else
        m_strImageFilter += wxT( "*." ) + strExtension.Lower() + wxT( ";" )
                         +  wxT( "*." ) + strExtension.Upper() + wxT( ";" );
        m_strImageFilterList += poHandler->GetMimeType() + wxT( " (*." )
                                + strExtension
                                + wxT( ")|*." )
                                + strExtension.Lower()
                                + wxT( ";*." )
                                + strExtension.Upper()
                                + wxT( "|" );
#endif
		if (wxImage::FindHandler(poHandler->GetName()))
		{
			delete poHandler;
		}
		else
		{
			wxImage::AddHandler(poHandler);
		}
	}
}
