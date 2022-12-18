#pragma once
#include "SqlExecuteRequest.h"

namespace Regards
{
	namespace Sqlite
	{
		class CSqlResult;

		class CSqlFindLocalisation : public CSqlExecuteRequest
		{
		public:
			CSqlFindLocalisation();
			~CSqlFindLocalisation();
			bool SearchUniqueCriteria(vector<wxString> * localisationVector, const wxString &day, const wxString &month, const wxString &year);

		private:

			int TraitementResult(CSqlResult * sqlResult);
			vector<wxString> * localisationVector;
		};
	}
}

