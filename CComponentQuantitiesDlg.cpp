// CComponentQuantitiesDlg.cpp : implementation file
//

#include "stdafx.h"
#include "CHPSDoc.h"
#include "CHPSView.h"
#include "CComponentQuantitiesDlg.h"
#include "IFCUtils.h"
#include "afxdialogex.h"
#include "resource.h"
#include "IFCQuantity.h"
#include "IFCSamplePreview.h"
#include "IFCReportBuilder.h"
#include <algorithm>

// CComponentQuantitiesDlg dialog

IMPLEMENT_DYNAMIC(CHPSComponentQuantitiesDlg, CDialogEx)

CHPSComponentQuantitiesDlg::CHPSComponentQuantitiesDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DLG_QUANTITY, pParent)
{
	mpQuantities = 0;
}

CHPSComponentQuantitiesDlg::~CHPSComponentQuantitiesDlg()
{
}

void CHPSComponentQuantitiesDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_QUANTITY, mListQuantities);
}


BEGIN_MESSAGE_MAP(CHPSComponentQuantitiesDlg, CDialogEx)
	ON_BN_CLICKED(IDC_BTN_QUANTITY_REPORT, &CHPSComponentQuantitiesDlg::OnBnClickedBtnQuantityReport)
	ON_WM_DESTROY()
	ON_BN_CLICKED(IDC_BTN_CLEARALL, &CHPSComponentQuantitiesDlg::OnBnClickedBtnClearall)
	ON_NOTIFY(LVN_ENDLABELEDIT, IDC_LIST_QUANTITY, &CHPSComponentQuantitiesDlg::OnLvnEndlabeleditListQuantity)
	ON_NOTIFY(NM_DBLCLK, IDC_LIST_QUANTITY, &CHPSComponentQuantitiesDlg::OnNMDblclkListQuantity)

	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST_QUANTITY, &CHPSComponentQuantitiesDlg::OnLvnItemchangedListQuantity)
END_MESSAGE_MAP()



// CComponentQuantitiesDlg message handlers


void CHPSComponentQuantitiesDlg::Reset()
{
	mpQuantities->ClearAll();

	mListQuantities.SetRedraw(FALSE);
	mListQuantities.DeleteAllItems();
	UpdateTotal(true);
	mListQuantities.SetRedraw(TRUE);
	mListQuantities.RedrawWindow();

}

BOOL CHPSComponentQuantitiesDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	LRESULT dwStyle;
	dwStyle = mListQuantities.SendMessage(LVM_GETEXTENDEDLISTVIEWSTYLE, 0, 0);
	dwStyle |= LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES;
	mListQuantities.SendMessage(LVM_SETEXTENDEDLISTVIEWSTYLE, 0, dwStyle);


	mListQuantities.InsertColumn(1, _T("Component Name          "));
	mListQuantities.InsertColumn(2, _T("IFC Type                "));
	mListQuantities.InsertColumn(3, _T("Quantity  "));
	mListQuantities.InsertColumn(0, _T("Unit Cost "));
	mListQuantities.InsertColumn(4, _T("Total Cost"));
	mListQuantities.SetColumnWidth(0, LVSCW_AUTOSIZE_USEHEADER);
	mListQuantities.SetColumnWidth(1, LVSCW_AUTOSIZE_USEHEADER);
	mListQuantities.SetColumnWidth(2, LVSCW_AUTOSIZE_USEHEADER);
	mListQuantities.SetColumnWidth(3, LVSCW_AUTOSIZE_USEHEADER);
	mListQuantities.SetColumnWidth(4, LVSCW_AUTOSIZE_USEHEADER);
	INT colOrder[] = { 1,2,3,0,4 };
	mListQuantities.SetColumnOrderArray(5, colOrder);

	CFont myFont;

	myFont.CreateFont(12, 0, 0, 0, FW_HEAVY, true, false,
		0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
		FIXED_PITCH | FF_MODERN, _T("Courier New"));

	CHeaderCtrl* header = mListQuantities.GetHeaderCtrl();
	header->SetFont(&myFont);

	mpQuantities = new IFCQuantity();
	UpdateTotal(true);



	
	return TRUE;  // return TRUE unless you set the focus to a control
				  // EXCEPTION: OCX Property Pages should return FALSE
}


void CHPSComponentQuantitiesDlg::UpdateRowCosts(int iRow, float unitCost)
{
	IFCQuantityRecord &qr = mpQuantities->GetQuantities().at(iRow);
	qr.SetUnitCost(unitCost);
	qr._totalCost = unitCost * qr._componentGroup.size(); 
	CString strTotal;
	strTotal.Format(_T("$%3.2f"), qr._totalCost);

	mListQuantities.SetItemText(iRow, 4, strTotal);

}

//
// Creates or updates the last row of the report containing the grand total 
//
void CHPSComponentQuantitiesDlg::UpdateTotal(bool bInsertNew )
{
	int nIndex = mListQuantities.GetItemCount();

	if (bInsertNew)
		nIndex = mListQuantities.InsertItem(nIndex, _T("")); // new item
	else
		nIndex--; // index of last item in current list

	float totalCost = 0;
	for (auto qr : mpQuantities->GetQuantities())
	{
		totalCost += qr._totalCost;
	}

	CString strTotal;
	strTotal.Format(_T("$%3.2f"), totalCost);

	mListQuantities.SetItemText(nIndex, 0, _T(""));
	mListQuantities.SetItemText(nIndex, 1, _T("Total"));
	mListQuantities.SetItemText(nIndex, 2, _T(""));
	mListQuantities.SetItemText(nIndex, 3, _T(""));
	mListQuantities.SetItemText(nIndex, 4, strTotal);

}

bool CHPSComponentQuantitiesDlg::AddComponent( HPS::Component *pComponent)
{
	CHPSDoc * pDoc = static_cast<CHPSDoc *>(static_cast<CFrameWnd *>(AfxGetApp()->m_pMainWnd)->GetActiveDocument());
	HPS::CADModel model = pDoc->GetCADModel();

	HPS::ComponentPathArray compPaths;
	HPS::ComponentArray     ancestors;
	HPS::UTF8 strName = pComponent->GetName();
	HPS::Metadata typeData = pComponent->GetMetadata("TYPE");

	if (typeData.Empty())
		return false;

	HPS::UTF8 strType = IFCUtils::GetMetadataValueAsUTF8(typeData);

	IFCUtils::FindIFCComponentByTypeAndName( model, strType, strName,ancestors,compPaths );

	CString strCnt;
	strCnt.Format(_T("%d"), compPaths.size());
	UTF8 ustrCnt(strCnt.GetString());

	IFCQuantityRecord &qr = mpQuantities->AddQuantityRecord(pComponent, strName, strType, compPaths);

	int nIndex = mListQuantities.GetItemCount()-1; // overwrite current total

	mListQuantities.SetItemText(nIndex, 0, _T("0"));
	mListQuantities.SetItemText(nIndex, 1, CString(strName));
	mListQuantities.SetItemText(nIndex, 2, CString(strType));
	mListQuantities.SetItemText(nIndex, 3, strCnt);

	mListQuantities.SetRedraw(FALSE);

	CHeaderCtrl* pHeader = (CHeaderCtrl*)mListQuantities.GetDlgItem(0);
	int nColumnCount = pHeader->GetItemCount();

	for (int i = 0; i < nColumnCount; i++)
	{
		mListQuantities.SetColumnWidth(i, LVSCW_AUTOSIZE);
		int nColumnWidth = mListQuantities.GetColumnWidth(i);
		mListQuantities.SetColumnWidth(i, LVSCW_AUTOSIZE_USEHEADER);
		int nHeaderWidth = mListQuantities.GetColumnWidth(i);
		mListQuantities.SetColumnWidth(i, std::max(nColumnWidth, nHeaderWidth));
	}

	UpdateRowCosts( nIndex, qr._unitCost );
	UpdateTotal(true);  // add new total row

	mListQuantities.SetRedraw(TRUE);
	
	return false;
}

//! [OnBnClickedBtnQuantityReport]
void CHPSComponentQuantitiesDlg::OnBnClickedBtnQuantityReport()
{
	CHPSDoc * pDoc = static_cast<CHPSDoc *>(static_cast<CFrameWnd *>(AfxGetApp()->m_pMainWnd)->GetActiveDocument());
	HPS::CADModel model = pDoc->GetCADModel();


	CString defaultExt(".pdf");
	CString defaultName(model.GetName());
	CString extensions("PDFDocument |*.pdf");

	CFileDialog pdfDlg(FALSE, defaultExt, defaultName, OFN_CREATEPROMPT | OFN_EXPLORER | OFN_OVERWRITEPROMPT , extensions );
	
	auto result = pdfDlg.DoModal();
	if (result != IDOK) 
		return; 

	IFCReportBuilder builder;
	builder.SetQuantities(mpQuantities);
	builder.SetCADModel(model);

	HPS::UTF8 pdfPath(pdfDlg.GetPathName());

	try 
	{
		builder.BuildReport( pdfPath);
	}
	catch (HPS::Exception e)
	{
		CString msg(e.what());
		MessageBox( _T("Exception Creating PDF"), msg, MB_ICONWARNING | MB_OK);
	}
}
//! [OnBnClickedBtnQuantityReport]


void CHPSComponentQuantitiesDlg::OnDestroy()
{
	CDialogEx::OnDestroy();

	delete mpQuantities;
	mpQuantities = 0;
}


CHPSDoc * CHPSComponentQuantitiesDlg::GetCHPSDoc()
{
	return static_cast<CHPSDoc *>(static_cast<CFrameWnd *>(AfxGetApp()->m_pMainWnd)->GetActiveDocument());
}

void CHPSComponentQuantitiesDlg::OnNMDblclkListQuantity(NMHDR *pNMHDR, LRESULT *pResult)
{
	//LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	// TODO: Add your control notification handler code here
	//LPNMITEMACTIVATE pitem = (LPNMITEMACTIVATE)pNMHDR;


	int row = mListQuantities.GetSelectionMark();

	if ((row < 0 ) || ( row == mpQuantities->GetQuantities().size() )) // last row is always the total 
		return; 

	CHPSView * pView = GetCHPSDoc()->GetCHPSView();

	HPS::Canvas canvas = pView->GetCanvas();
	HPS::HighlightOptionsKit hKit;
	hKit.SetStyleName("highlight_style").SetNotification(true);
	for (auto cp : mpQuantities->GetQuantities()[row]._componentGroup)
	{
		cp.Highlight(canvas, hKit, true);
	}


	pView->Update();

	*pResult = 0;
}


void CHPSComponentQuantitiesDlg::OnBnClickedBtnClearall()
{
	Reset();
	
}


void CHPSComponentQuantitiesDlg::OnLvnEndlabeleditListQuantity(NMHDR *pNMHDR, LRESULT *pResult)
{
	NMLVDISPINFO *pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
	// TODO: Add your control notification handler code here
	
	if (NULL != pDispInfo->item.pszText)
	{
		//
		// Check the new string is a number
		//
		LPTSTR endPtr;

		float fValue    = _tcstof(pDispInfo->item.pszText, &endPtr);
		size_t nChar    = _tcslen(pDispInfo->item.pszText);
		size_t  nParsed = endPtr - pDispInfo->item.pszText;

		if (nChar == nParsed)
		{
			int iRow = pDispInfo->item.iItem;
			mListQuantities.SetItemText(iRow, 0, pDispInfo->item.pszText);
			UpdateRowCosts(iRow, fValue );
			UpdateTotal(false);
		}
	}
	*pResult = 0;
}
 

void CHPSComponentQuantitiesDlg::OnLvnItemchangedListQuantity(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	// TODO: Add your control notification handler code here


	int row = pNMLV->iItem;

	if ((row < 0) || (row == mpQuantities->GetQuantities().size())) // last row is always the total 
		return;

	CHPSView * pView = GetCHPSDoc()->GetCHPSView();

	auto qr = mpQuantities->GetQuantities()[row];
	pView->GetPreviewWindow()->UpdatePreview(qr);

	*pResult = 0;
}
