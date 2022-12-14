#pragma once
#include <Photos.h>
#include "SqlExecuteRequest.h"

namespace Regards
{
	namespace Sqlite
	{
		class CSqlResult;

		class CSqlFindPhotos : public CSqlExecuteRequest
		{
		public:
			CSqlFindPhotos();
			~CSqlFindPhotos();
            void UpdatePhotosExtension();
			bool SearchPhotos(PhotosVector * photosVector);
			bool SearchPhotos(vector<int> * listPhoto);
			bool SearchPhotos(PhotosVector * photosVector, const wxString &libelleCriteria);
			bool SearchPhotos(PhotosVector * photosVector, const wxString &localisation, const wxString &libelleCriteria);
			bool SearchPhotos(PhotosVector * photosVector, const int &numCategorie, const int &numCatalog);
			bool SearchPhotosByTypeAffichage(PhotosVector * photosVector, const int &typeAffichage, const int &numCatalog);
			wxString GenerateSqlRequest(const int &numCatalog, vector<int> & listFolder, vector<int> & listCriteriaNotIn, const wxString &libelleNotGeo, const double & pertinence = 0.0);
			bool SearchPhotos(const wxString &sqlRequest);
			bool GetAllPhotos(PhotosVector * photosVector);
            bool GetAllVideo(PhotosVector * photosVector);
			bool SearchPhotosByCriteria(PhotosVector* photosVector);

		private:
			
			bool FindIfViewExist();
			int TraitementResultPhotoDataCriteria(CSqlResult* sqlResult);
			int TraitementResultPhoto(CSqlResult * sqlResult);
			int TraitementResultNumPhoto(CSqlResult * sqlResult);
			bool DeleteAllInSearchPhotos();
			wxString GetSearchSQL(vector<int> list);
			int TraitementResult(CSqlResult * sqlResult);
			PhotosVector * m_photosVector;
			vector<int> * m_listPhoto;
			int typeResult;
			wxString table_name;
		};
	}
}

