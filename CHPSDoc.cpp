#include "stdafx.h"
#include "CHPSApp.h"
#include "CHPSFrame.h"
#include "CHPSDoc.h"
#include "CHPSView.h"
#include "IFCSampleModel.h"
#include "CProgressDialog.h"
#include "HPSExchangeProgressDialog.h"


#include <propkey.h>

IMPLEMENT_DYNCREATE(CHPSDoc, CDocument)
BEGIN_MESSAGE_MAP(CHPSDoc, CDocument)
END_MESSAGE_MAP()

CHPSDoc::CHPSDoc()
{
	// TODO: add one-time construction code here
	_pIfcSampleModel = new IFCSampleModel();
}

CHPSDoc::~CHPSDoc()
{
	delete _pIfcSampleModel;
}

void CHPSDoc::CreateNewModel()
{
	// Delete our model if we have one already
	if (_model.Type() != HPS::Type::None)
		_model.Delete();

	// Delete old CADModel
	_cadModel.Delete();

	// Create a new model for our view to attach to
	_model = HPS::Factory::CreateModel();

	// Lose old default camera
	_defaultCamera.Reset();
}

BOOL CHPSDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

	// Create a new blank model
	CreateNewModel();

	// Restore scene defaults if we already initialized our canvas
	if (GetCHPSView()->GetCanvas().Type() != HPS::Type::None)
	{
		GetCHPSView()->GetCanvas().GetWindowKey().GetHighlightControl().UnhighlightEverything();
		GetCHPSView()->SetupSceneDefaults();
	}

	PostMessage(GetCHPSFrame()->GetSafeHwnd(), WM_MFC_SANDBOX_INITIALIZE_BROWSERS, 0, 0);

	return TRUE;
}


BOOL CHPSDoc::OnOpenDocument(LPCTSTR lpszPathName)
{
	if (!CDocument::OnOpenDocument(lpszPathName))
		return FALSE;

	CStringA ansiPath(lpszPathName);

	const char *ext = ::strrchr(ansiPath, '.');
	if (!ext)
		return FALSE;
	ext++;

	const char *visualizeExtensions[] = {
		"hsf",
		"stl",
		"obj",
		"pts",
		"ptx",
		"xyz",
	};
	int const extCount = sizeof(visualizeExtensions) / sizeof(visualizeExtensions[0]);
	
	bool visualizeHandled = false;
	for (int i = 0; i < extCount; i++) {
		if (!_stricmp(ext, visualizeExtensions[i])) {
			visualizeHandled = true;
			break;
		}
	}

	// Create a model which we will import our scene into
	CreateNewModel();

	// Flush any existing highlights
	GetCHPSView()->GetCanvas().GetWindowKey().GetHighlightControl().UnhighlightEverything();

	bool success = false;
	if (!_stricmp(ext, "hsf"))
		success = ImportHSFFile(lpszPathName);
	else if (!_stricmp(ext, "stl"))
		success = ImportSTLFile(lpszPathName);
	else if (!_stricmp(ext, "obj"))
		success = ImportOBJFile(lpszPathName);
	else if (!_stricmp(ext, "ptx") || !_stricmp(ext, "pts") || !_stricmp(ext, "xyz"))
		success = ImportPointCloudFile(lpszPathName);
#ifdef USING_PARASOLID
	else if (!_stricmp(ext, "x_t") || !_stricmp(ext, "xmt_txt") || !_stricmp(ext, "x_b") || !_stricmp(ext, "xmt_bin"))
		success = ImportParasolidFile(lpszPathName);
#endif
#ifdef USING_DWG
#ifndef _DEBUG
	else if (!_stricmp(ext, "dwg") || !_stricmp(ext, "dxf"))
		success = ImportDWGFile(lpszPathName);
#endif
#endif
#ifdef USING_EXCHANGE
	else
	{
		HPS::Exchange::ImportOptionsKit options;
		options.SetPMI(true);
		success = ImportExchangeFile(lpszPathName,options);
	}
#else
	else
	{
		GetCHPSFrame()->MessageBox(_T("Unsupported file extension"), _T("File import error"), MB_ICONERROR | MB_OK);
		return FALSE;
	}
#endif

	if (success)
	{
		CHPSApp * app = (CHPSApp *)AfxGetApp();
		app->addToRecentFileList(lpszPathName);
		CHPSView * cview = GetCHPSView();
		cview->UpdatePlanes();

		if (_cadModel.Empty() && _defaultCamera.Empty())
			cview->GetCanvas().GetFrontView().ComputeFitWorldCamera(_defaultCamera);

		if (!_stricmp(ext, "ifc"))
		{
			HPS::Type objectType = _cadModel.Type();

 			cview->OnLoadedIFCFile( this );
		}
	}
	else
	{
		GetCHPSView()->GetCanvas().GetFrontView().AttachModel(_model);
		GetCHPSView()->GetCanvas().Update();
	}

	PostMessage(GetCHPSFrame()->GetSafeHwnd(), WM_MFC_SANDBOX_INITIALIZE_BROWSERS, 0, 0);

	return TRUE;
}

BOOL CHPSDoc::OnMergeExchangeModel(LPCTSTR lpszPathName, HPS::ComponentPath mergePath)
{
	CStringA ansiPath(lpszPathName);

	const char *ext = ::strrchr(ansiPath, '.');
	if (!ext)
		return FALSE;
	ext++;

	// Flush any existing highlights
	GetCHPSView()->GetCanvas().GetWindowKey().GetHighlightControl().UnhighlightEverything();

	HPS::Exchange::ImportOptionsKit options;
	options.SetLocation(mergePath);
	bool success = MergeExchangeFile(lpszPathName, options);

	if (success)
	{
		CHPSApp * app = (CHPSApp *)AfxGetApp();
		app->addToRecentFileList(lpszPathName);
		CHPSView * cview = GetCHPSView();
		cview->UpdatePlanes();

		if (_cadModel.Empty() && _defaultCamera.Empty())
			cview->GetCanvas().GetFrontView().ComputeFitWorldCamera(_defaultCamera);

		if (!_stricmp(ext, "ifc"))
		{
			HPS::Type objectType = _cadModel.Type();

			cview->OnLoadedIFCFile(this, TRUE);
		}
	}
	

	PostMessage(GetCHPSFrame()->GetSafeHwnd(), WM_MFC_SANDBOX_INITIALIZE_BROWSERS, 0, 0);

	return TRUE;
}

namespace
{
	CAtlString getErrorString(
		HPS::IOResult status,
		LPCTSTR lpszPathName)
	{
		CAtlString str;
		switch (status)
		{
			case HPS::IOResult::FileNotFound:
			{
				str.Format(_T("Could not locate file %s"), lpszPathName);
			}	break;

			case HPS::IOResult::UnableToOpenFile:
			{
				str.Format(_T("Unable to open file %s"), lpszPathName);
			}	break;

			case HPS::IOResult::InvalidOptions:
			{
				str.Format(_T("Invalid options"));
			}	break;

			case HPS::IOResult::InvalidSegment:
			{
				str.Format(_T("Invalid segment"));
			}	break;

			case HPS::IOResult::UnableToLoadLibraries:
			{
				str.Format(_T("Unable to load libraries"));
			}	break;

			case HPS::IOResult::VersionIncompatibility:
			{
				str.Format(_T("Version incompatability"));
			}	break;

			case HPS::IOResult::InitializationFailed:
			{
				str.Format(_T("Initialization failed"));
			}	break;

			case HPS::IOResult::UnsupportedFormat:
			{
				str.Format(_T("Unsupported format"));
			}	break;

			case HPS::IOResult::Canceled:
			{
				str.Format(_T("IO Canceled"));
			}	break;

			case HPS::IOResult::Failure:
			default:
			{
				str.Format(_T("Error loading file %s"), lpszPathName);
			}
		}
		return str;
	}
}

bool CHPSDoc::ImportHSFFile(LPCTSTR lpszPathName)
{
	bool success = false;
	HPS::IOResult status = HPS::IOResult::Failure;
	HPS::Stream::ImportNotifier notifier;


	// HPS::Stream::File::Import can throw HPS::IOException
	try
	{
		// Specify the model segment as the segment to import to
		HPS::Stream::ImportOptionsKit ioOpts;
		ioOpts.SetSegment(_model.GetSegmentKey());
		ioOpts.SetAlternateRoot(_model.GetLibraryKey());
		ioOpts.SetPortfolio(_model.GetPortfolioKey());

		// Initiate import.  Import is done on a separate thread.
		HPS::UTF8 filename(lpszPathName);
		notifier = HPS::Stream::File::Import(filename.GetBytes(), ioOpts);

		// Spawn modal dialog box with progress bar.  Initial scene update will be performed by dialog
		// box since initial update may take awhile and we want the progress bar to still be up during that update.
		CProgressDialog dlg(this, notifier);
		dlg.DoModal();
		success = dlg.WasImportSuccessful();
		status = notifier.Status();
	}
	catch (HPS::IOException const & ex)
	{
		status = ex.result;
	}

	if (status != HPS::IOResult::Success)
	{
		CAtlString str = getErrorString(status, lpszPathName);
		GetCHPSFrame()->MessageBox(str.GetString(), _T("File import error"), MB_ICONERROR | MB_OK);
	}
	else
		notifier.GetResults().ShowDefaultCamera(_defaultCamera);

	return success;
}

bool CHPSDoc::ImportSTLFile(LPCTSTR lpszPathName)
{
	bool success = false;
	HPS::IOResult			status = HPS::IOResult::Failure;

	// HPS::STL::File::Import can throw HPS::IOException
	try
	{
		// Specify the model segment as the segment to import to
		HPS::STL::ImportOptionsKit ioOpts = HPS::STL::ImportOptionsKit::GetDefault();
		ioOpts.SetSegment(_model.GetSegmentKey());

		// Initiate import.  Import is done on a separate thread.
		HPS::UTF8						filename(lpszPathName);
		HPS::STL::ImportNotifier		notifier = HPS::STL::File::Import(filename.GetBytes(), ioOpts);

		// Spawn modal dialog box with progress bar.  Initial scene update will be performed by dialog
		// box since initial update may take awhile and we want the progress bar to still be up during that update.
		CProgressDialog					dlg(this, notifier);
		dlg.DoModal();
		success = dlg.WasImportSuccessful();
		status = notifier.Status();
	}
	catch (HPS::IOException const & ex)
	{
		status = ex.result;
	}

	if (status != HPS::IOResult::Success)
	{
		CAtlString str = getErrorString(status, lpszPathName);
		GetCHPSFrame()->MessageBox(str.GetString(), _T("File import error"), MB_ICONERROR | MB_OK);
	}

	return success;
}

bool CHPSDoc::ImportOBJFile(LPCTSTR lpszPathName)
{
	bool success = false;
	HPS::IOResult			status = HPS::IOResult::Failure;

	// HPS::OBJ::File::Import can throw HPS::IOException
	try
	{
		// Specify the model segment as the segment to import to
		HPS::OBJ::ImportOptionsKit			ioOpts;
		ioOpts.SetSegment(_model.GetSegmentKey());
		ioOpts.SetPortfolio(_model.GetPortfolioKey());

		// Initiate import.  Import is done on a separate thread.
		HPS::UTF8						filename(lpszPathName);
		HPS::OBJ::ImportNotifier		notifier = HPS::OBJ::File::Import(filename.GetBytes(), ioOpts);

		// Spawn modal dialog box with progress bar.  Initial scene update will be performed by dialog
		// box since initial update may take awhile and we want the progress bar to still be up during that update.
		CProgressDialog					dlg(this, notifier);
		dlg.DoModal();
		success = dlg.WasImportSuccessful();
		status = notifier.Status();
	}
	catch (HPS::IOException const & ex)
	{
		status = ex.result;
	}

	if (status != HPS::IOResult::Success)
	{
		CAtlString str = getErrorString(status, lpszPathName);
		GetCHPSFrame()->MessageBox(str.GetString(), _T("File import error"), MB_ICONERROR | MB_OK);
	}

	return success;
}

bool CHPSDoc::ImportPointCloudFile(LPCTSTR lpszPathName)
{
	bool success = false;
	HPS::IOResult			status = HPS::IOResult::Failure;

	// HPS::PointCloud::File::Import can throw HPS::IOException
	try
	{
		// Specify the model segment as the segment to import to
		HPS::PointCloud::ImportOptionsKit			ioOpts;
		ioOpts.SetSegment(_model.GetSegmentKey());

		// Initiate import.  Import is done on a separate thread.
		HPS::UTF8						filename(lpszPathName);
		HPS::PointCloud::ImportNotifier		notifier = HPS::PointCloud::File::Import(filename.GetBytes(), ioOpts);

		// Spawn modal dialog box with progress bar.  Initial scene update will be performed by dialog
		// box since initial update may take awhile and we want the progress bar to still be up during that update.
		CProgressDialog					dlg(this, notifier);
		dlg.DoModal();
		success = dlg.WasImportSuccessful();
		status = notifier.Status();
	}
	catch (HPS::IOException const & ex)
	{
		status = ex.result;
	}

	if (status != HPS::IOResult::Success)
	{
		CAtlString str = getErrorString(status, lpszPathName);
		GetCHPSFrame()->MessageBox(str.GetString(), _T("File import error"), MB_ICONERROR | MB_OK);
	}

	return success;
}

#ifdef USING_EXCHANGE
void CHPSDoc::ImportConfiguration(HPS::UTF8Array const & configuration)
{
	if (configuration.empty())
		return;

	HPS::UTF8 filename = HPS::StringMetadata(_cadModel.GetMetadata("Filename")).GetValue();
	HPS::WCharArray wchars;
	filename.ToWStr(wchars);

	HPS::Exchange::ImportOptionsKit options;
	options.SetConfiguration(configuration);

	bool success = ImportExchangeFile(wchars.data(), options);

	if (success)
		GetCHPSView()->UpdatePlanes();
	else
	{
		GetCHPSView()->GetCanvas().GetFrontView().AttachModel(_model);
		GetCHPSView()->GetCanvas().Update();
	}

	PostMessage(GetCHPSFrame()->GetSafeHwnd(), WM_MFC_SANDBOX_INITIALIZE_BROWSERS, 0, 0);
}

static void getConfiguration(HPS::Exchange::ConfigurationArray const & configs, HPS::UTF8Array & selectedConfig)
{
	if (configs.empty())
		return;

	selectedConfig.push_back(configs[0].GetName());
	getConfiguration(configs[0].GetSubconfigurations(), selectedConfig);
}

bool CHPSDoc::ImportExchangeFile(LPCTSTR lpszPathName, HPS::Exchange::ImportOptionsKit const & options)
{
	bool success = false;

	HPS::Exchange::ImportNotifier notifier;
	HPS::IOResult status = HPS::IOResult::Failure;
	std::string message;

	try
	{
		HPS::Exchange::ImportOptionsKit ioOpts = options;
		ioOpts.SetBRepMode(HPS::Exchange::BRepMode::BRepAndTessellation);
		ioOpts.SetIFCMetadata(true);
		ioOpts.SetAttributes(true);
		ioOpts.SetIFCOwnerHistoryOptimization(false);

		HPS::UTF8 filename(lpszPathName);
		HPS::Exchange::File::Format format = HPS::Exchange::File::GetFormat(filename);

		HPS::UTF8Array selectedConfig;
		HPS::Exchange::ConfigurationArray configs;
		if (format == HPS::Exchange::File::Format::CATIAV4
			&& !ioOpts.ShowConfiguration(selectedConfig)
			&& !(configs = HPS::Exchange::File::GetConfigurations(filename)).empty())
		{
			// If this is a CATIA V4 file w/ configurations a configuration must be specified to perform the import.
			// Since no configuration was specified, we'll pick the first one and import it, otherwise, the import will throw an exception.
			getConfiguration(configs, selectedConfig);
			ioOpts.SetConfiguration(selectedConfig);
		}

		CHPSExchangeProgressDialog dlg(this, notifier, filename);

		notifier = HPS::Exchange::File::Import(filename, ioOpts);

		dlg.DoModal();
		success = dlg.WasImportSuccessful();
		status = notifier.Status();
	}
	catch (HPS::IOException const & ex)
	{
		status = ex.result;
		message = ex.what();
	}

	_cadModel.Delete();

	if (status != HPS::IOResult::Success)
	{
		CAtlString str;
		if (status == HPS::IOResult::Failure)
			str.Format(_T("Error loading file %s:\n\n\t%s"), lpszPathName, std::wstring(message.begin(), message.end()).data());
		else
			str = getErrorString(status, lpszPathName);
		GetCHPSFrame()->MessageBox(str.GetString(), _T("File import error"), MB_ICONERROR | MB_OK);
	}
	else
		_cadModel = notifier.GetCADModel();

	return success;
}

bool CHPSDoc::MergeExchangeFile(LPCTSTR lpszPathName, HPS::Exchange::ImportOptionsKit const & options)
{
	bool success = false;

	HPS::Exchange::ImportNotifier notifier;
	HPS::IOResult status = HPS::IOResult::Failure;
	std::string message;

	try
	{
		HPS::Exchange::ImportOptionsKit ioOpts = options;
		ioOpts.SetBRepMode(HPS::Exchange::BRepMode::BRepAndTessellation);
		ioOpts.SetIFCMetadata(true);
		ioOpts.SetAttributes(true);
		ioOpts.SetIFCOwnerHistoryOptimization(false);

		HPS::UTF8 filename(lpszPathName);
		HPS::Exchange::File::Format format = HPS::Exchange::File::GetFormat(filename);

		HPS::UTF8Array selectedConfig;
		HPS::Exchange::ConfigurationArray configs;
		if (format == HPS::Exchange::File::Format::CATIAV4
			&& !ioOpts.ShowConfiguration(selectedConfig)
			&& !(configs = HPS::Exchange::File::GetConfigurations(filename)).empty())
		{
			// If this is a CATIA V4 file w/ configurations a configuration must be specified to perform the import.
			// Since no configuration was specified, we'll pick the first one and import it, otherwise, the import will throw an exception.
			getConfiguration(configs, selectedConfig);
			ioOpts.SetConfiguration(selectedConfig);
		}

		CHPSExchangeProgressDialog dlg(this, notifier, filename);

		notifier = HPS::Exchange::File::Import(filename, ioOpts);

		dlg.DoModal();
		success = dlg.WasImportSuccessful();
		status = notifier.Status();
	}
	catch (HPS::IOException const & ex)
	{
		status = ex.result;
		message = ex.what();
	}

	if (status != HPS::IOResult::Success)
	{
		CAtlString str;
		if (status == HPS::IOResult::Failure)
			str.Format(_T("Error loading file %s:\n\n\t%s"), lpszPathName, std::wstring(message.begin(), message.end()).data());
		else
			str = getErrorString(status, lpszPathName);
		GetCHPSFrame()->MessageBox(str.GetString(), _T("File import error"), MB_ICONERROR | MB_OK);
	}

	return success;
}
#endif

#ifdef USING_PARASOLID
bool CHPSDoc::ImportParasolidFile(LPCTSTR lpszPathName, HPS::Parasolid::ImportOptionsKit const & options)
{
	bool success = false;
	HPS::Parasolid::ImportNotifier notifier;
	HPS::IOResult status = HPS::IOResult::Failure;
	std::string message;

	try
	{
		HPS::Parasolid::ImportOptionsKit ioOpts = options;
		HPS::UTF8 filename(lpszPathName);

		HPS::Parasolid::Format format;
		if (!ioOpts.ShowFormat(format))
		{
			char const * ext = ::strrchr(filename.GetBytes(), '.');
			++ext;

			if (!_stricmp(ext, "x_t") || !_stricmp(ext, "xmt_txt"))
				format = HPS::Parasolid::Format::Text;
			else // assuming this is not a neutral binary file
				format = HPS::Parasolid::Format::Binary;

			ioOpts.SetFormat(format);
		}

		notifier = HPS::Parasolid::File::Import(filename, ioOpts);

		CProgressDialog dlg(this, notifier);
		dlg.DoModal();
		success = dlg.WasImportSuccessful();
		status = notifier.Status();

	}
	catch (HPS::IOException const & ex)
	{
		status = ex.result;
		message = ex.what();
	}

	_cadModel.Delete();

	if (status != HPS::IOResult::Success)
	{
		CAtlString str;
		if (status == HPS::IOResult::Failure)
			str.Format(_T("Error loading file %s:\n\n\t%s"), lpszPathName, std::wstring(message.begin(), message.end()).data());
		else
			str = getErrorString(status, lpszPathName);
		GetCHPSFrame()->MessageBox(str.GetString(), _T("File import error"), MB_ICONERROR | MB_OK);
	}
	else
		_cadModel = notifier.GetCADModel();

	return success;
}
#endif

#ifdef USING_DWG
bool CHPSDoc::ImportDWGFile(LPCTSTR lpszPathName, HPS::DWG::ImportOptionsKit const & options)
{
	bool success = false;
	HPS::DWG::ImportNotifier notifier;
	HPS::IOResult status = HPS::IOResult::Failure;
	std::string message;

	try
	{
		HPS::DWG::ImportOptionsKit ioOpts = options;
		HPS::UTF8 filename(lpszPathName);

		notifier = HPS::DWG::File::Import(filename, ioOpts);

		CProgressDialog dlg(this, notifier);
		dlg.DoModal();
		success = dlg.WasImportSuccessful();
		status = notifier.Status();

	}
	catch (HPS::IOException const & ex)
	{
		status = ex.result;
		message = ex.what();
	}

	_cadModel.Delete();

	if (status != HPS::IOResult::Success)
	{
		CAtlString str;
		if (status == HPS::IOResult::Failure)
			str.Format(_T("Error loading file %s:\n\n\t%s"), lpszPathName, std::wstring(message.begin(), message.end()).data());
		else
			str = getErrorString(status, lpszPathName);
		GetCHPSFrame()->MessageBox(str.GetString(), _T("File import error"), MB_ICONERROR | MB_OK);
	}
	else
		_cadModel = notifier.GetCADModel();

	return success;
}
#endif

CHPSView *	CHPSDoc::GetCHPSView()
{
	// We should always have an attached View
	POSITION			pos = GetFirstViewPosition();
	ASSERT(pos != NULL);

	CView *				cView = GetNextView(pos);
	CHPSView *			cHPSView = reinterpret_cast<CHPSView *>(cView);
	return cHPSView;
}

CHPSFrame * CHPSDoc::GetCHPSFrame()
{
	return reinterpret_cast<CHPSFrame *>(GetCHPSView()->GetParentFrame());
}

CHPSDoc * GetCHPSDoc()
{
	return static_cast<CHPSDoc *>(static_cast<CFrameWnd *>(AfxGetApp()->m_pMainWnd)->GetActiveDocument());
}

