
#pragma once
#include "FilterWindowParam.h"

namespace Regards
{
	namespace Filter
    {
        class CBilateralFilter : public CFilterWindowParam
        {
        public:
            CBilateralFilter();
            ~CBilateralFilter();
            int GetNameFilter();
            int GetTypeFilter();
            int TypeApplyFilter();

            wxString GetFilterLabel();
			void Filter(CEffectParameter * effectParameter, const wxString & filename, IFiltreEffectInterface * filtreInterface){};
            void Filter(CEffectParameter * effectParameter, cv::Mat & source, const wxString& filename, IFiltreEffectInterface * filtreInterface);
            void FilterChangeParam(CEffectParameter * effectParameter,  CTreeElementValue * valueData, const wxString &key);
            void RenderEffect(CFiltreEffet* filtreEffet, CEffectParameter* effectParameter, const bool& preview);
            bool NeedPreview();
            CEffectParameter* GetEffectPointer();
            CEffectParameter* GetDefaultEffectParameter();
            bool IsSourcePreview();
            void ApplyPreviewEffectSource(CEffectParameter* effectParameter, IBitmapDisplay* bitmapViewer, CFiltreEffet* filtreEffet, CDraw* dessing);
            void ApplyPreviewEffect(CEffectParameter* effectParameter, IBitmapDisplay* bitmapViewer, CFiltreEffet* filtreEffet, CDraw* m_cDessin, int& widthOutput, int& heightOutput);
            CImageLoadingFormat* ApplyEffect(CEffectParameter* effectParameter, IBitmapDisplay* bitmapViewer);
        private:
			wxString libelleEffectsigmaX;
			wxString libelleEffectsigmaP;
            wxString libelleEffectSize;
        };
    }
}

