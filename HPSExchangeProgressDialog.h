#pragma once
#include "afxwin.h"
#include "afxdialogex.h"
#include "afxcmn.h"

#include "resource.h"
#include "CHPSDoc.h"
#include "sprk.h"


class CHPSExchangeProgressDialog;

class ImportStatusEventHandler : public HPS::EventHandler
{
public:
	ImportStatusEventHandler()
		: HPS::EventHandler()
		, _progress_dlg(nullptr)
	{}

	ImportStatusEventHandler(CHPSExchangeProgressDialog * in_progress_dlg)
		: HPS::EventHandler()
		, _progress_dlg(in_progress_dlg)
	{}

	virtual ~ImportStatusEventHandler() { Shutdown(); }

	HandleResult Handle(HPS::Event const * in_event) override;

private:
	CHPSExchangeProgressDialog * _progress_dlg;
};



class CHPSExchangeProgressDialog : public CDialogEx
{
	DECLARE_DYNAMIC(CHPSExchangeProgressDialog)

public:
	CHPSExchangeProgressDialog(CHPSDoc * doc, HPS::IONotifier & notifier, HPS::UTF8 const & title);
	virtual ~CHPSExchangeProgressDialog();

	enum 
	{ 
		IDD				= IDD_EXCHANGE_IMPORT_DIALOG,
		TIMER_ID		= 1236,
		STATUS_TIMER_ID	= 1237,
	};

	void SetMessage(HPS::UTF8 const & in_message) { _message = in_message; }
	void AddLogEntry(HPS::UTF8 const & in_log_entry);
	bool WasImportSuccessful() { return _success; }

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnBnClickedCancel();
	afx_msg void OnBnClickedCheckKeepOpen();

	DECLARE_MESSAGE_MAP()

private:
	CEdit						_edit_box;
	CProgressCtrl				_progress_ctrl;
	HPS::IONotifier &			_notifier;
	CHPSDoc	*					_doc;
	bool						_keep_dialog_open;
	bool						_success;
	HPS::UTF8					_message;
	HPS::UTF8					_title;
	std::deque<HPS::UTF8>		_log_messages;
	ImportStatusEventHandler *	_import_status_event;

	void						PerformInitialUpdate();
};
