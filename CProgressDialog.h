#pragma once

class CProgressDialog : public CDialogEx
{
public:
	CProgressDialog(CHPSDoc * doc, HPS::IONotifier & notifier, bool is_export = false);
		
	enum {
		IDD			= IDD_PROGRESS,			// Dialog resource ID
		TIMER_ID	= 1234					// Custom Timer ID
	};

	bool WasImportSuccessful() { return _success; }

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	afx_msg void OnTimer(UINT_PTR nIDEvent);

	DECLARE_MESSAGE_MAP()

private:

	bool								_success;
	CHPSDoc	*							_doc;
	HPS::IONotifier &					_notifier;
	CProgressCtrl						_progress_ctrl;
	bool								_export;

	// Called when import succeeded.
	void PerformInitialUpdate();
	afx_msg void OnBnClickedButtonCancel();
};


class PDFExportDialog : public CDialogEx
{
	DECLARE_DYNAMIC(PDFExportDialog)

public:
	PDFExportDialog(CWnd * pParent = NULL); 
	virtual ~PDFExportDialog();
	bool Export2DPDF() { return export_2d_pdf; }

	enum { IDD = IDD_PDF_EXPORT_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange * pDX);
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()

private:
	afx_msg void OnBnClicked2D();
	afx_msg void OnBnClicked3D();
	bool export_2d_pdf;
};


