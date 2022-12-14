#include <header.h>
#include "SqlFaceThumbnail.h"
#include "SqlFacePhoto.h"
using namespace Regards::Sqlite;

CSqlFaceThumbnail::CSqlFaceThumbnail(const wxString & filename, const int &numFace)
	: CThumbnailData(filename)
{
	this->numFace = numFace;
}

CSqlFaceThumbnail::~CSqlFaceThumbnail(void)
{
}

wxImage CSqlFaceThumbnail::GetwxImage()
{
	CSqlFacePhoto sqlThumbnail;
   // printf("Filename : %s \n",CConvertUtility::ConvertToUTF8(filename));
	return sqlThumbnail.GetFace(numFace);
}

bool CSqlFaceThumbnail::TestBitmap()
{
	return true;
}

