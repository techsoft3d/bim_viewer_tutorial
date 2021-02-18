#include "stdafx.h"
#include "HPSExchangeProgressDialog.h"
#include "afxdialogex.h"
#include "CHPSView.h"
#include <mutex>

#ifdef USING_EXCHANGE
#include "sprk_exchange.h"
#endif

std::mutex mtx;

namespace
{
	bool isAbsolutePath(char const * path)
	{
		bool starts_with_drive_letter = strlen(path) >= 3
			&& isalpha(path[0])
			&& path[1] == ':'
			&& strchr("/\\", path[2]);

		bool starts_with_network_drive_prefix = strlen(path) >= 2
			&& strchr("/\\", path[0])
			&& strchr("/\\", path[1]);

		return starts_with_drive_letter || starts_with_network_drive_prefix;
	}
}

HPS::EventHandler::HandleResult ImportStatusEventHandler::Handle(HPS::Event const * in_event)
{
	if (_progress_dlg)
	{
		HPS::UTF8 message = static_cast<HPS::ImportStatusEvent const *>(in_event)->import_status_message;
		if (message.IsValid())
		{
			bool update_message = true;

			if (message == HPS::UTF8("Import and Tessellation"))
				_progress_dlg->SetMessage(HPS::UTF8("Stage 1/3 : Import and Tessellation"));
			else if (message == HPS::UTF8("Creating Graphics Database"))
				_progress_dlg->SetMessage(HPS::UTF8("Stage 2/3 : Creating Graphics Database"));
			else if (isAbsolutePath(message))
			{
				std::string path = message;
				message = path.substr(path.find_last_of("/\\") + 1).c_str();
				_progress_dlg->AddLogEntry(HPS::UTF8(message));
				update_message = false;
			}
			else
				update_message = false;

			if (update_message)
				_progress_dlg->SetTimer(CHPSExchangeProgressDialog::STATUS_TIMER_ID, 15, NULL);
		}
	}
	return HPS::EventHandler::HandleResult::Handled;
}


IMPLEMENT_DYNAMIC(CHPSExchangeProgressDialog, CDialogEx)

BEGIN_MESSAGE_MAP(CHPSExchangeProgressDialog, CDialogEx)
	ON_WM_TIMER()
	ON_BN_CLICKED(IDCANCEL, &CHPSExchangeProgressDialog::OnBnClickedCancel)
	ON_BN_CLICKED(IDC_CHECK_KEEP_OPEN, &CHPSExchangeProgressDialog::OnBnClickedCheckKeepOpen)
END_MESSAGE_MAP()

CHPSExchangeProgressDialog::CHPSExchangeProgressDialog(CHPSDoc * doc, HPS::IONotifier & notifier, HPS::UTF8 const & in_title)
	: CDialogEx(CHPSExchangeProgressDialog::IDD)
	, _doc(doc)
	, _notifier(notifier)
	, _title(in_title)
	, _success(false)
	, _keep_dialog_open(false)
{
	_import_status_event = new ImportStatusEventHandler(this);
}

CHPSExchangeProgressDialog::~CHPSExchangeProgressDialog()
{
	delete _import_status_event;
}

void CHPSExchangeProgressDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT_MESSAGES, _edit_box);
	DDX_Control(pDX, IDC_PROGRESS_BAR, _progress_ctrl);
}

BOOL CHPSExchangeProgressDialog::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	SetTimer(CHPSExchangeProgressDialog::TIMER_ID, 50, NULL);

	// Set our progress bar range
	_progress_ctrl.SetRange(0, 100);
	_progress_ctrl.ModifyStyle(0, PBS_MARQUEE);
	_progress_ctrl.SetMarquee(TRUE, 50);

	GetDlgItem(IDC_IMPORT_MESSAGE)->SetWindowTextW(L"Stage 1/3 : Import and Tessellation");

	HPS::WCharArray wtitle;
	_title.ToWStr(wtitle);
	std::wstring str_title(wtitle.data());
	size_t found = str_title.find_last_of(L"/");
	if (found == std::wstring::npos)
	{
		found = str_title.find_last_of(L"\\");
		if (found == std::wstring::npos)
			SetWindowText(str_title.data());
		else
			SetWindowText(str_title.substr(found + 1).data());
	}
	else 
		SetWindowText(str_title.substr(found + 1).data());

	HPS::Database::GetEventDispatcher().Subscribe(*_import_status_event, HPS::Object::ClassID<HPS::ImportStatusEvent>());

	return TRUE;
}

void CHPSExchangeProgressDialog::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == CHPSExchangeProgressDialog::TIMER_ID)
	{
		try 
		{
			{
				//update the import log
				std::lock_guard<std::mutex> lock(mtx);

				if (!_log_messages.empty())
				{
					int length = _edit_box.GetWindowTextLengthW();
					wchar_t * buffer = new wchar_t[length + 1];
					_edit_box.GetWindowTextW(buffer, length + 1);

					HPS::UTF8 text(buffer);
					delete [] buffer;

					for (auto it = _log_messages.begin(), e = _log_messages.end(); it != e; ++it)
					{
						if (text != HPS::UTF8(""))
							text += HPS::UTF8("\r\n");
						text = text + HPS::UTF8("Reading ") + *it;
					}
					_log_messages.clear();

					HPS::WCharArray wtext;
					text.ToWStr(wtext);
					_edit_box.SetWindowTextW(wtext.data());
					_edit_box.LineScroll(_edit_box.GetLineCount());
				}
			}

			HPS::IOResult status;
			status = _notifier.Status();

			if (status != HPS::IOResult::InProgress)
			{
				HPS::Database::GetEventDispatcher().UnSubscribe(*_import_status_event);
				KillTimer(nIDEvent);

				if (status == HPS::IOResult::Success)
					PerformInitialUpdate();

				if (!_keep_dialog_open)
					EndDialog(0);
			}

		}
		catch (HPS::IOException const &)
		{
			// notifier not yet created
		}
	}
	else if (nIDEvent == CHPSExchangeProgressDialog::STATUS_TIMER_ID)
	{
		//update the import message
		if (_message.IsValid())
		{
			HPS::WCharArray wchar_message;
			_message.ToWStr(wchar_message);
			GetDlgItem(IDC_IMPORT_MESSAGE)->SetWindowText(wchar_message.data());

			KillTimer(nIDEvent);
			_message = HPS::UTF8();
		}
	}
}

void CHPSExchangeProgressDialog::PerformInitialUpdate()
{
#ifdef USING_EXCHANGE
	GetDlgItem(IDCANCEL)->EnableWindow(FALSE);
	GetDlgItem(IDC_IMPORT_MESSAGE)->SetWindowText(L"Stage 3/3 : Performing Initial Update");

	CHPSView * mfcView = _doc->GetCHPSView();

	HPS::CADModel cadModel;
	cadModel = static_cast<HPS::Exchange::ImportNotifier>(_notifier).GetCADModel();

	if (!cadModel.Empty())
	{
		cadModel.GetModel().GetSegmentKey().GetPerformanceControl().SetStaticModel(HPS::Performance::StaticModel::Attribute);
		mfcView->AttachView(cadModel.ActivateDefaultCapture().FitWorld());
	}
	else
		ASSERT(0);

	HPS::UpdateNotifier updateNotifier = mfcView->GetCanvas().UpdateWithNotifier(HPS::Window::UpdateType::Exhaustive);
	updateNotifier.Wait();

	GetDlgItem(IDCANCEL)->EnableWindow(TRUE);
	GetDlgItem(IDCANCEL)->SetWindowTextW(L"Close Dialog");
	_progress_ctrl.SetMarquee(FALSE, 9999);
	_progress_ctrl.SetRange(0, 100);
	_progress_ctrl.SetPos(100);
	GetDlgItem(IDC_IMPORT_MESSAGE)->SetWindowText(L"Import Complete");
	_progress_ctrl.Invalidate();
	_progress_ctrl.UpdateWindow();

	_success = true;
#endif
}


void CHPSExchangeProgressDialog::OnBnClickedCancel()
{
	if (_success)
		EndDialog(0);
	else
	{
		_notifier.Cancel();

		KillTimer(CHPSExchangeProgressDialog::TIMER_ID);
		this->OnCancel();
	}
}


void CHPSExchangeProgressDialog::OnBnClickedCheckKeepOpen()
{
	CWnd * item = GetDlgItem(IDC_CHECK_KEEP_OPEN);
	CButton * button = static_cast<CButton *>(item);

	int check_status = button->GetCheck();
	if (check_status == BST_CHECKED)
		_keep_dialog_open = true;
	else if (check_status == BST_UNCHECKED)
		_keep_dialog_open = false;
}

void CHPSExchangeProgressDialog::AddLogEntry(HPS::UTF8 const & in_log_entry)
{
	std::lock_guard<std::mutex> lock(mtx);
	_log_messages.push_back(in_log_entry);
}