#pragma once
class IFCQuantity;
#include "hps.h"
#include <string>
struct IFCQuantityRecord;

class IFCReportBuilder
{
	IFCQuantity    *_mpQuantity;
	HPS::CADModel    _cadModel;
	HPS::UTF8      _reportPath;
	std::string     _strTableStyle;
	std::string    _strTableBegin;
	std::string    _strTableContent;
	std::string    _strTableEnd;

	void InitializeTable();
	void InsertTableRow(IFCQuantityRecord &record);
	void InsertTableTotal();

	void CreatePDF( std::string &strTable);

public:
	IFCReportBuilder();
	~IFCReportBuilder();

	void SetQuantities(IFCQuantity * pQuants);
	void SetCADModel(HPS::CADModel &cadModel);
	bool BuildReport( HPS::UTF8 &path);

	void ClearAll();

};

