#pragma once
class IFCQuantity;
class CHPSDoc;

// CComponentQuantitiesDlg dialog

class CHPSComponentQuantitiesDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CHPSComponentQuantitiesDlg)

public:
	CHPSComponentQuantitiesDlg(CWnd* pParent = nullptr);   // standard constructor
	virtual ~CHPSComponentQuantitiesDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DLG_QUANTITY };
#endif

	CHPSDoc * GetCHPSDoc();


protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()

	void Reset();

public:
	virtual BOOL OnInitDialog();
	CListCtrl    mListQuantities;
	IFCQuantity *mpQuantities;

	void UpdateRowCosts(int iRow, float unitCost);
	void UpdateTotal( bool bInsertNew );

	bool AddComponent(  HPS::Component *pComponent);

	afx_msg void OnBnClickedBtnQuantityReport();
	afx_msg void OnDestroy();
	afx_msg void OnNMDblclkListQuantity(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnBnClickedBtnClearall();
	afx_msg void OnLvnEndlabeleditListQuantity(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnItemchangedListQuantity(NMHDR *pNMHDR, LRESULT *pResult);
};
