#include "stdafx.h"
#include "IFCQuantity.h"
#include "IFCReportBuilder.h"
#include <sstream>
#include "hps.h"
#include "sprk_exchange.h"
#include "sprk_publish.h"

//! [InitializeTable]
IFCReportBuilder::IFCReportBuilder()
{
	_mpQuantity = 0;

	InitializeTable();
}

void IFCReportBuilder::InitializeTable()
{
	_strTableStyle = "<style type=\"text/css\"> \
	table.gridtable {\
		font-family: helvetica;\
		font-size:10pt;\
		text-align: left;\
		border-width: 0pt;\
		border-collapse: collapse;\
		width:500pt; \
	}\
	table.gridtable th {\
		border-width: 0pt;\
		border-style: solid;\
		background-color: #dedede;\
		padding: 2pt;\
		height:12pt;\
		min-width:40pt; \
        max-width:180pt; \
        width:150pt; \
	}\
	table.gridtable td {\
		border-width: 0pt;\
		border-style: solid;\
		background-color: #ffffff;\
		padding: 2pt;\
		height:12pt;\
		min-width:40pt; \
        max-width:180pt; \
        width:150pt; \
		}\
	table.gridtable td.link  {\
		text-decoration:underline;\
		color:blue;\
	}\
	table.gridtable td.pass  {\
		background-color: rgb(0,255,0);\
	}\
	table.gridtable td.fail  {\
		background-color: rgb(255,0,0);\
	}\
	</style>;";

	_strTableBegin = "<table class=\"gridtable\"><tr> <th>Component Name</th> <th>Type</th> <th>Quantity</th> <th>Unit Cost</th><th>Total</th></tr>";
	_strTableEnd = "</table>";
}
//! [InitializeTable]

//! [table_contents]
void IFCReportBuilder::InsertTableRow(IFCQuantityRecord & qr)
{
	std::stringstream ss;
	ss << "<tr> <td>" << qr._componentName.GetBytes() << "</td>" << 
			   "<td >" << qr._componentType.GetBytes() << "</td>" <<
			   "<td >" << qr.GetComponentQuantity() << "</td>"
			   "<td >" << qr._unitCost << "</td> "
			   "<td >" << qr._totalCost << "</td> </tr>";

	_strTableContent += ss.str();
}

void IFCReportBuilder::InsertTableTotal()
{
	float totalCost = 0;

	for (auto qr : _mpQuantity->GetQuantities())
		totalCost += qr._totalCost;

	std::stringstream ss;
	ss << "<tr> <td>" << "Total" << "</td>" <<
		"<td >" << "" << "</td>" <<
		"<td >" << "" << "</td>"
		"<td >" << ""<< "</td> "
		"<td >" << "$" << totalCost << "</td> </tr>";

	_strTableContent += ss.str();
}
//! [table_contents]

void IFCReportBuilder::SetQuantities(IFCQuantity * pQuants)
{
	_mpQuantity = pQuants;
}

void IFCReportBuilder::SetCADModel(HPS::CADModel & cadModel)
{
	_cadModel = cadModel;
}

//! [BuildReport]
bool IFCReportBuilder::BuildReport( HPS::UTF8 & path)
{ 
	if (0 == _mpQuantity)
		return false;

	_reportPath = path;

	for (auto qr : _mpQuantity->GetQuantities())
		InsertTableRow(qr);

	InsertTableTotal();
	std::string strTable = _strTableBegin + _strTableContent + _strTableEnd;
	HPS::Publish::DocumentKit documentKit;

	CreatePDF( strTable );

	return true;
}
//! [BuildReport]

void IFCReportBuilder::ClearAll()
{
	_mpQuantity->ClearAll();
}

//! [CreatePDF]
void IFCReportBuilder::CreatePDF(std::string & strTable)
{

	HPS::Publish::ViewKit viewKit;             // corresponds to a particular view of the model
	HPS::Publish::ArtworkKit artworkKit;       // container for Javascript, PMI, and views
	HPS::Publish::AnnotationKit annotationKit; // represents the model as an object in the PDF
	HPS::Publish::PageKit pageKit;             // corresponds to a page inside the PDF
	HPS::Publish::DocumentKit documentKit;     // corresponds to the document itself as a whole
	HPS::Publish::TableKit tableKit;
	HPS::Publish::TextKit textKit;

	//! [page_format]
	pageKit.SetFormat(HPS::Publish::Page::Format::A4); // 595 x 842 pts
	pageKit.SetOrientation(HPS::Publish::Page::Orientation::Portrait);
	//! [page_format]
	
	//
	// Add some header information
	//

	//! [text_label]
	textKit.SetColor(RGBColor(0.25, 0.25, 0.25))
		.SetFont(HPS::Publish::Text::Font::Name::Helvetica)
		.SetSize(24)
		.SetText("Dodgy Construction Company");
	pageKit.AddText(textKit, IntRectangle(50, 350, 805, 830));
	//! [text_label]

	//
	// Lets add the name of the model
	//
	//! [model_name]
	std::string strModelName = "Model:" + _cadModel.GetName();
	textKit.SetColor(RGBColor(0.25, 0.25, 0.25))
		.SetFont(HPS::Publish::Text::Font::Name::Helvetica)
		.SetSize(12)
		.SetText(strModelName.c_str());
	pageKit.AddText(textKit, IntRectangle(50, 350, 788, 800));
	//! [model_name]

	//
	// Put in a logo image
	//
	HPS::Publish::ImageKit imageKit;
	imageKit.SetFile("C:\\Temp\\symbol.bmp");
	imageKit.SetFormat(HPS::Publish::Image::Format::BMP);
	imageKit.SetSize(256, 256);

	HPS::UTF8 filename;
	pageKit.AddImage(imageKit, IntRectangle(0,200, 300, 500));
	imageKit.ShowFile(filename);

	//
	//  Content Max Height = 780
	//
	//! [insert_model]
	annotationKit.SetSource(_cadModel);
	annotationKit.SetPRCBRepCompression(Publish::PRC::BRepCompression::Medium); // set B-rep compression to Medium
	annotationKit.SetPRCTessellationCompression(true);	// use tessellation compression
	pageKit.SetAnnotation(annotationKit, IntRectangle(50, 562, 480, 780));
	//! [insert_model]


#define USE_SLIDE_TABLE
#ifndef USE_SLIDE_TABLE

	//! [add_table]
	tableKit.SetHTML(strTable.c_str());     // specify HTML source
	tableKit.SetHTMLStyle(_strTableStyle.c_str(), HPS::Publish::Source::Type::Code); // specify CSS source
	pageKit.AddTable(tableKit, IntRectangle(50, 450, 50, 450));
	//! [add_table]
	
#else 
		HPS::Publish::SlideTableKit slide_table;
		slide_table.SetHTML(strTable.c_str());     // specify HTML source
		slide_table.SetHTMLStyle(_strTableStyle.c_str(), HPS::Publish::Source::Type::Code); // specify CSS source

	
		// buttons are specified by name using ButtonKit::SetName
		slide_table.SetButtons("previous_button", "next_button");
		pageKit.AddSlideTable(slide_table, IntRectangle(50, 450, 500, 750));
#endif

#ifdef USE_DOCUKIT
	HPS::Publish::File::ExportPDF(documentKit, "c:\\temp\\_test2.pdf", Publish::ExportOptionsKit());
#else
	HPS::Publish::DocumentKey myDocumentKey = HPS::Publish::File::CreateDocument(NULL);

	myDocumentKey.AddPage(pageKit);

	try
	{
		HPS::Publish::File::ExportPDF(myDocumentKey, _reportPath.GetBytes());
	}
	catch (HPS::Exception e)
	{
		myDocumentKey.Delete();
		throw e;
	}
	myDocumentKey.Delete();
#endif

}
//! [CreatePDF]

IFCReportBuilder::~IFCReportBuilder()
{

}