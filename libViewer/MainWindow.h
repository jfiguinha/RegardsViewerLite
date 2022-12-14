#pragma once
#include <Photos.h>
#include <WindowMain.h>
using namespace Regards::Window;

class CRegardsBitmap;
class CImageLoadingFormat;
class CPictureCategorieLoadData;
class CPictureCategorie;
class IStatusBarInterface;
class CFFmpegTranscoding;
class CompressionAudioVideoOption;

namespace Regards
{
	namespace Viewer
	{
		class CMainWindow;
		class CImageList;

		class CToolbar;
		class CMainParam;
		class CCentralWindow;


		class CMainWindow : public CWindowMain
		{
		public:
			CMainWindow(wxWindow* parent, wxWindowID id, IStatusBarInterface* statusbar, const bool& openFirstFile);
			~CMainWindow() override;
			void UpdateScreenRatio() override;

			bool SetFullscreen();
			bool SetFullscreenMode();
			bool SetScreen();
			void TransitionEnd();

			bool OpenFolder(const wxString& path);
			bool IsFullscreen();
			void ShowToolbar();
			bool IsVideo();

			void SetText(const int& numPos, const wxString& libelle);
			void SetRangeProgressBar(const int& range);
			void SetPosProgressBar(const int& position);

			void OpenFile(const wxString& fileToOpen);

			bool GetProcessEnd() override;
			void OnOpenFileOrFolder(wxCommandEvent& event);
			wxString GetFilename();

			void SaveParameter() override;

		private:
			void SetDataToStatusBar(void* thumbnailMessage, const wxString& message);

			bool FindNextValidFile();
			bool FindPreviousValidFile();

			void OnUpdateInfos(wxCommandEvent& event);
			void OnShowToolbar(wxCommandEvent& event);
			void OnStatusSetText(wxCommandEvent& event);

			void OnSetRangeProgressBar(wxCommandEvent& event);
			void OnSetValueProgressBar(wxCommandEvent& event);

			void OnRefreshPicture(wxCommandEvent& event);
			void UpdateStatusBarMessage(wxCommandEvent& event);

			void OnExit(wxCommandEvent& event);
			void InitPictures(wxCommandEvent& event);
			void PictureVideoClick(wxCommandEvent& event);
			void CriteriaChange(wxCommandEvent& event);

			void OnPrint(wxCommandEvent& event);
			void OnPictureClick(wxCommandEvent& event);
			void Md5Checking(wxCommandEvent& event);
			void PrintPreview(wxCommandEvent& event);

			void OnUpdateExifThumbnail(wxCommandEvent& event);

			void SetScreenEvent(wxCommandEvent& event);
			void RefreshFolderList(wxCommandEvent& event);

			void OnUpdateFolder(wxCommandEvent& event);

			void Resize() override;
			void ProcessIdle() override;
			void OnIdle(wxIdleEvent& evt);
			void OnEndThumbnail(wxCommandEvent& event);
			static void CheckMD5(void* param);


			void UpdateCriteria();
			void RefreshFolder();
			void UpdateFolder();
			void PhotoProcess(CPhotos* photo);

			wxString tempVideoFile = "";
			wxString tempAudioVideoFile = "";
			bool fullscreen;
			wxGauge* progressBar;
			wxStatusBar* statusBar;
			CCentralWindow* centralWnd;

			IStatusBarInterface* statusBarViewer;
			wxRect posWindow;
			PhotosVector pictures;

			wxString localFilename;
			int nbProcessMD5;

			bool showToolbar;
			CMainParam* viewerParam;
			bool multithread;
			bool needToReload;
			int typeAffichage;
			bool updateCriteria;
			bool updateFolder;
			bool refreshFolder;
			int numElementTraitement;
			bool start;
			bool criteriaSendMessage;
			bool checkVersion;
			bool setViewerMode = false;
			//CompressionAudioVideoOption* compressAudioVideoOption = nullptr;
			CFFmpegTranscoding* ffmpegEncoder = nullptr;
			wxString fileOut = "";
			wxString fileOutAudio = "";
			wxString fileOutVideo = "";
			wxString filepathVideo = "";
			wxString firstFileToShow = "";
			bool needToRemux = false;
			bool isAudio = false;

		};
	}
}