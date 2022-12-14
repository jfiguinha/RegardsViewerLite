#include <header.h>
#include "ConfigRegards.h"
#include <RegardsConfigParam.h>
#include <LibResource.h>
#include <ParamInit.h>
#include "ViewerParamInit.h"
#include "ViewerParam.h"
using namespace Regards::Viewer;
#ifndef WX_PRECOMP
//(*InternalHeadersPCH(ConfigRegards)
//*)
#endif
//(*InternalHeaders(ConfigRegards)
#include <wx/xrc/xmlres.h>
//*)

//(*IdInit(ConfigRegards)
//*)

BEGIN_EVENT_TABLE(ConfigRegards, wxDialog)
	//(*EventTable(ConfigRegards)
	//*)
END_EVENT_TABLE()

ConfigRegards::ConfigRegards(wxWindow* parent)
{
	isOk = false;
	//(*Initialize(ConfigRegards)
	SetTitle("Regards Configuration");
	wxXmlResource::Get()->LoadObject(this, parent,_T("ConfigRegards"),_T("wxDialog"));
	rbTransitionEffect = static_cast<wxComboBox*>(FindWindow(XRCID("ID_RBTRANSITIONEFFECT")));
	rbTransitionDiaporamaEffect = static_cast<wxComboBox*>(FindWindow(XRCID("ID_RBDIAPORAMATRANSITIONEFFECT")));
	sbDiaporama = static_cast<wxStaticBox*>(FindWindow(XRCID("ID_SBDIAPORAMA")));
	btOk = static_cast<wxButton*>(FindWindow(XRCID("ID_OK")));
	scTime = static_cast<wxSpinCtrl*>(FindWindow(XRCID("ID_SCTIME")));
	stTime = static_cast<wxStaticText*>(FindWindow(XRCID("ID_STTIME")));
	scProcessExif = static_cast<wxSpinCtrl*>(FindWindow(XRCID("ID_SCEXIF")));
	scProcessThumbnail = static_cast<wxSpinCtrl*>(FindWindow(XRCID("ID_SCTHUMBNAIL")));
	btCancel = static_cast<wxButton*>(FindWindow(XRCID("ID_CANCEL")));
	sbThumbnail = static_cast<wxStaticBox*>(FindWindow(XRCID("ID_STATICBOX2")));


	rbDatabaseInMemory = static_cast<wxRadioBox*>(FindWindow(XRCID("ID_RBDATAINMEMORY")));
	rbAutoRotate = static_cast<wxRadioBox*>(FindWindow(XRCID("ID_RBROTATEAUTO")));
	rbInterpolation = static_cast<wxComboBox*>(FindWindow(XRCID("ID_CBINTERPOLATIONFILTER")));

	rbOpenCLOpenGLInterop = static_cast<wxRadioBox*>(FindWindow(XRCID("ID_RBOPENGLOPENCLINTEROP")));

	rbContrastCorrection = static_cast<wxRadioBox*>(FindWindow(XRCID("ID_RBAUTOCONTRAST")));

	txtMusicDiaporamaPath = static_cast<wxTextCtrl*>(FindWindow(XRCID("ID_TXTMUSICDIAPORAMAPATH")));
	btMusicDiaporamaPath = static_cast<wxButton*>(FindWindow(XRCID("ID_MUSICDIAPORAMAPATH")));
	rbVideoDecoderHard = static_cast<wxComboBox*>(FindWindow(XRCID("ID_CBVIDEODECODERHARD")));


	Connect(XRCID("ID_OK"), wxEVT_COMMAND_BUTTON_CLICKED, (wxObjectEventFunction)&ConfigRegards::OnbtnOkClick);
	Connect(XRCID("ID_CANCEL"), wxEVT_COMMAND_BUTTON_CLICKED, (wxObjectEventFunction)&ConfigRegards::OnBtnCancelClick);
	//*)
	Connect(XRCID("ID_MUSICDIAPORAMAPATH"), wxEVT_COMMAND_BUTTON_CLICKED,
	        (wxObjectEventFunction)&ConfigRegards::OnBtnMusicDiaporamaClick);


	init();

	SetAutoLayout(TRUE);
}

void ConfigRegards::OnBtnMusicDiaporamaClick(wxCommandEvent& event)
{
	wxString label = CLibResource::LoadStringFromResource(L"LBLSELECTPICTUREEDITOR", 1);
	wxString allfiles = CLibResource::LoadStringFromResource(L"LBLALLFILES", 1);
	wxString filename = CLibResource::LoadStringFromResource(L"LBLFILESNAME", 1);

	wxFileDialog openFileDialog(nullptr, label, "", filename,
	                            "mp3 " + filename + " (*.mp3)|*.mp3|aac " + filename + " (*.aac)|*.aac|wav " + filename
	                            + " (*.wav)|*.wav", wxFD_OPEN | wxFD_FILE_MUST_EXIST);

	if (openFileDialog.ShowModal() == wxID_OK)
		txtMusicDiaporamaPath->SetValue(openFileDialog.GetPath());
}

ConfigRegards::~ConfigRegards()
{
	//(*Destroy(ConfigRegards)
	//*)
}

void ConfigRegards::init()
{
	this->SetTitle("Configuration");
	CRegardsConfigParam* regardsParam = CParamInit::getInstance();

	int transition = max((regardsParam->GetEffect() - 300), 0);
	if (transition == 0)
		rbTransitionEffect->SetSelection(0);
	else
		rbTransitionEffect->SetSelection(transition);

	int transitionDiaporama = max((regardsParam->GetDiaporamaTransitionEffect() - 400), 0);
	if (transitionDiaporama == 0)
		rbTransitionDiaporamaEffect->SetSelection(0);
	else
		rbTransitionDiaporamaEffect->SetSelection(transitionDiaporama);

	int autoRotate = regardsParam->GetDetectOrientation();
	if (autoRotate == 0)
		rbAutoRotate->SetSelection(1);
	else
		rbAutoRotate->SetSelection(0);

	int autoContrast = regardsParam->GetAutoConstrast();
	if (autoContrast == 0)
		rbContrastCorrection->SetSelection(1);
	else
		rbContrastCorrection->SetSelection(0);

	txtMusicDiaporamaPath->SetValue(regardsParam->GetMusicDiaporama());

	int timeDiaporama = regardsParam->GetDiaporamaTime();
	scTime->SetValue(timeDiaporama);

	int thumbnailProcess = regardsParam->GetThumbnailProcess();
	scProcessThumbnail->SetValue(thumbnailProcess);

	int exifProcess = regardsParam->GetExifProcess();
	scProcessExif->SetValue(exifProcess);

	int dataInMemory = regardsParam->GetDatabaseInMemory();
	if (dataInMemory == 0)
		rbDatabaseInMemory->SetSelection(1);
	else
		rbDatabaseInMemory->SetSelection(0);

	int interpolation = regardsParam->GetInterpolationType();
	rbInterpolation->SetSelection(interpolation);

	int openclOpenGLInterop = regardsParam->GetIsOpenCLOpenGLInteropSupport();
	if (openclOpenGLInterop == 0)
		rbOpenCLOpenGLInterop->SetSelection(1);
	else
		rbOpenCLOpenGLInterop->SetSelection(0);
	
	wxString decoder = regardsParam->GetHardwareDecoder();	
	if (decoder != "")
	{
		int numItem = rbVideoDecoderHard->FindString(decoder);
		rbVideoDecoderHard->SetSelection(numItem);
	}
	else
		rbVideoDecoderHard->SetSelection(0);



}

void ConfigRegards::OnbtnOkClick(wxCommandEvent& event)
{
	isOk = true;
	bool showInfosRestart = false;
	CRegardsConfigParam* regardsParam = CParamInit::getInstance();
	CMainParam* mainparam = CMainParamInit::getInstance();
	int nbProcesseur = thread::hardware_concurrency();

	regardsParam->SetMusicDiaporama(txtMusicDiaporamaPath->GetValue());

	int transition = rbTransitionEffect->GetSelection();
	if (transition == 0)
		regardsParam->SetEffect(0);
	else
		regardsParam->SetEffect(transition + 300);

	int transitionDiaporama = rbTransitionDiaporamaEffect->GetSelection();
	regardsParam->SetDiaporamaTransitionEffect(transitionDiaporama + 400);

	int autoRotate = rbAutoRotate->GetSelection();
	if (autoRotate == 0)
		regardsParam->SetDectectOrientation(1);
	else
		regardsParam->SetDectectOrientation(0);

	int autoContrast = rbContrastCorrection->GetSelection();
	if (autoContrast == 0)
		regardsParam->SetAutoConstrast(1);
	else
		regardsParam->SetAutoConstrast(0);

	int interpolation = rbInterpolation->GetSelection();
	regardsParam->SetInterpolationType(interpolation);

	int timeDiaporama = scTime->GetValue();
	regardsParam->SetDiaporamaTime(timeDiaporama);

	int thumbnailProcess = scProcessThumbnail->GetValue();
	int exifProcess = scProcessExif->GetValue();

	int openclOpenGLInterop = rbOpenCLOpenGLInterop->GetSelection();
	if (openclOpenGLInterop == 0)
		regardsParam->SetIsOpenCLOpenGLInteropSupport(1);
	else
		regardsParam->SetIsOpenCLOpenGLInteropSupport(0);

	wxString oldencoder = regardsParam->GetHardwareEncoder();
	wxString olddecoder = regardsParam->GetHardwareDecoder();

	wxString decoder = rbVideoDecoderHard->GetStringSelection();

	if (decoder == "auto")
		regardsParam->SetHardwareDecoder("");
	else
		regardsParam->SetHardwareDecoder(decoder);

	if (olddecoder != decoder)
		showInfosRestart = true;

	if (thumbnailProcess == 0 || exifProcess == 0)
	{
		wxString errorProcessNumberMin = CLibResource::LoadStringFromResource(L"ErrorProcessNumberMin", 1);
		wxString errorInfo = CLibResource::LoadStringFromResource(L"informationserror", 1);
		wxMessageBox(errorProcessNumberMin, errorInfo);
	}
	else if ((thumbnailProcess + exifProcess) > nbProcesseur && (thumbnailProcess > 1 ||exifProcess > 1))
	{
		wxString errorProcessNumberMax = CLibResource::LoadStringFromResource(L"ErrorProcessNumberMax", 1);
		wxString errorInfo = CLibResource::LoadStringFromResource(L"informationserror", 1);
		wxMessageBox(errorProcessNumberMax, errorInfo);
	}
	else
	{
		regardsParam->SetExifProcess(exifProcess);
		regardsParam->SetThumbnailProcess(thumbnailProcess);

		int dataInMemory = rbDatabaseInMemory->GetSelection();
		if (dataInMemory == 0)
			regardsParam->SetDatabaseInMemory(1);
		else
			regardsParam->SetDatabaseInMemory(0);

		if (showInfosRestart)
		{
			wxString labelRestart = CLibResource::LoadStringFromResource(L"labelRestart", 1);
			wxString lblInfos = CLibResource::LoadStringFromResource(L"labelInformations", 1);
			wxMessageBox(labelRestart, lblInfos);
		}


		this->Close();
	}
}

void ConfigRegards::OnBtnCancelClick(wxCommandEvent& event)
{
	isOk = false;
	this->Close();
}

bool ConfigRegards::IsOk()
{
	return isOk;
}
