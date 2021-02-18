#include "StdAfx.h"
#include "resource.h"       // main symbols
#include "CHPSDoc.h"
#include "CHPSView.h"
#include "CProgressDialog.h"


BEGIN_MESSAGE_MAP(CProgressDialog, CDialogEx)
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_BUTTON_CANCEL, &CProgressDialog::OnBnClickedButtonCancel)
END_MESSAGE_MAP()


CProgressDialog::CProgressDialog(CHPSDoc * doc, HPS::IONotifier & notifier, bool is_export)
	: CDialogEx(CProgressDialog::IDD), _doc(doc), _notifier(notifier), _success(false), _export(is_export)
{ }

void CProgressDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_PROGRESS_BAR, _progress_ctrl);
}

BOOL CProgressDialog::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	SetTimer(CProgressDialog::TIMER_ID, 50, NULL);

	// Set our progress bar range
	_progress_ctrl.SetRange(0, 100);

	return TRUE;
}

void CProgressDialog::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == CProgressDialog::TIMER_ID)
	{
		try
		{
			float						complete;

			HPS::IOResult status;
			status = _notifier.Status(complete);

			// Update our progress control with the file load percent complete.
			int							progress = static_cast<int>(complete * 100.0f + 0.5f);
			_progress_ctrl.SetPos(progress);

			// Update dialog box title with percentage complete
			CAtlString					str;
			str.Format(_T("Loading ... (%d%%)"), progress);
			SetWindowText(str.GetString());

			// Exit on any status other than InProgress
			if (status != HPS::IOResult::InProgress)
			{
				if (status == HPS::IOResult::Success && !_export)
					PerformInitialUpdate();
				EndDialog(0);
			}
		}
		catch (HPS::IOException const &)
		{
			// notifier not yet created
		}
	}

	CDialogEx::OnTimer(nIDEvent);
}

void CProgressDialog::PerformInitialUpdate()
{
	// Get our HPS::View.  Note that CHPSView is the MFC view
	CHPSView * mfcView = _doc->GetCHPSView();
	HPS::Type notifierType = _notifier.Type();

	if (notifierType == HPS::Type::ExchangeImportNotifier || notifierType == HPS::Type::ParasolidImportNotifier || notifierType == HPS::Type::DWGImportNotifier)
	{
		HPS::CADModel cadModel;

#ifdef USING_EXCHANGE
		if (notifierType == HPS::Type::ExchangeImportNotifier)
			cadModel = static_cast<HPS::Exchange::ImportNotifier>(_notifier).GetCADModel();
#elif defined(USING_PARASOLID)
		if (notifierType == HPS::Type::ParasolidImportNotifier)
			cadModel = static_cast<HPS::Parasolid::ImportNotifier>(_notifier).GetCADModel();
#elif defined(USING_DWG)
		if (notifierType == HPS::Type::DWGImportNotifier)
			cadModel = static_cast<HPS::DWG::ImportNotifier>(_notifier).GetCADModel();
#endif

		if (!cadModel.Empty())
		{
			cadModel.GetModel().GetSegmentKey().GetPerformanceControl().SetStaticModel(HPS::Performance::StaticModel::Attribute);
			mfcView->AttachView(cadModel.ActivateDefaultCapture().FitWorld());
		}
		else
			ASSERT(0);
	}
	else
	{
		HPS::View view = mfcView->GetCanvas().GetFrontView();

		// Enable static model for better performance
		_doc->GetModel().GetSegmentKey().GetPerformanceControl().SetStaticModel(HPS::Performance::StaticModel::Attribute);

		// Attach the model created in CHPSDoc
		view.AttachModel(_doc->GetModel());

		if (notifierType == HPS::Type::StreamImportNotifier)
		{
			// Load default camera if we have one, otherwise perform a FitWorld to center the model
			HPS::CameraKit defaultCamera;
			HPS::Stream::ImportResultsKit stm_import_results = static_cast<HPS::Stream::ImportNotifier>(_notifier).GetResults();

			if (stm_import_results.ShowDefaultCamera(defaultCamera))
				view.GetSegmentKey().SetCamera(defaultCamera);
			else
				view.FitWorld();
		}
		else
			view.FitWorld();

		// Add a distant light
		mfcView->SetMainDistantLight();
	}

	// Perform initial update after file load.  This first update make take awhile as it's building the static model.
	// We want this update to occur while the loading dialog is still showing.
	SetWindowText(L"Performing Initial Update ...");
	HPS::UpdateNotifier updateNotifier = mfcView->GetCanvas().UpdateWithNotifier(HPS::Window::UpdateType::Exhaustive);
	updateNotifier.Wait();

	_success = true;
}

void CProgressDialog::OnBnClickedButtonCancel()
{
	_notifier.Cancel();

	KillTimer(CProgressDialog::TIMER_ID);
	this->OnCancel();
}


IMPLEMENT_DYNAMIC(PDFExportDialog, CDialogEx)

PDFExportDialog::PDFExportDialog(CWnd * pParent)
	: CDialogEx(PDFExportDialog::IDD, pParent)
	, export_2d_pdf(true)
{}

PDFExportDialog::~PDFExportDialog()
{}

void PDFExportDialog::DoDataExchange(CDataExchange * pDX)
{
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(PDFExportDialog, CDialogEx)
	ON_BN_CLICKED(IDC_RADIO_2D, &PDFExportDialog::OnBnClicked2D)
	ON_BN_CLICKED(IDC_RADIO_3D, &PDFExportDialog::OnBnClicked3D)
END_MESSAGE_MAP()

BOOL PDFExportDialog::OnInitDialog()
{
	CWnd * item = GetDlgItem(IDC_RADIO_2D);
	CButton * button = static_cast<CButton *>(item);
	button->SetCheck(TRUE);

	return TRUE;
}

void PDFExportDialog::OnBnClicked2D()
{
	export_2d_pdf = true;	
}


void PDFExportDialog::OnBnClicked3D()
{
	export_2d_pdf = false;
}

