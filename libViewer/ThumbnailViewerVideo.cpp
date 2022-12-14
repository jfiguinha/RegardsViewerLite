#include <header.h>
#include "ThumbnailViewerVideo.h"
#include "MainWindow.h"
#include <window_id.h>
using namespace Regards::Viewer;

CThumbnailViewerVideo::CThumbnailViewerVideo(wxWindow* parent, wxWindowID id, const CThemeThumbnail & themeThumbnail, const bool &testValidity)
	: CThumbnailVideo(parent, id, themeThumbnail, testValidity)
{
	filename = L"";
	idWindowToRefresh = THUMBNAILVIDEOWINDOW;
	moveOnPaint = false;
}


CThumbnailViewerVideo::~CThumbnailViewerVideo(void)
{

}

void CThumbnailViewerVideo::OnPictureClick(CThumbnailData * data)
{
	CMainWindow * mainWindow = (CMainWindow *)this->FindWindowById(MAINVIEWERWINDOWID);
	if (mainWindow != nullptr && data != nullptr)
	{
#ifdef FFMPEG
		int timePosition = data->GetTimePosition();
#else
		int timePosition = data->GetTimePosition() * 1000;
#endif
		wxCommandEvent evt(wxEVENT_PICTUREVIDEOCLICK);
		evt.SetExtraLong(timePosition);
		mainWindow->GetEventHandler()->AddPendingEvent(evt);
	}
}