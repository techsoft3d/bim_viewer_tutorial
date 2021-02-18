#include "stdafx.h"
#include "afxwinappex.h"
#include "afxdialogex.h"
#include "CHPSApp.h"
#include "CHPSFrame.h"

#include "CHPSDoc.h"
#include "CHPSView.h"
#include "IFCSampleApp.h"

#include "hoops_license.h"

#include <sstream>

// The one and only CHPSApp object
CHPSApp theApp;

BEGIN_MESSAGE_MAP(CHPSApp, CWinAppEx)
	ON_COMMAND(ID_FILE_NEW, &CWinAppEx::OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, &CWinAppEx::OnFileOpen)
END_MESSAGE_MAP()

CHPSApp::CHPSApp()
	: _appLook(0), _world(NULL)
{
	SetAppID(_T("TechSoft3D.mfc_sandbox"));
}

BOOL CHPSApp::InitInstance()
{
	// Initialize HPS::World
	_world = new HPS::World(HOOPS_LICENSE);
	_world->SetMaterialLibraryDirectory("../../samples/data/materials");
	this->setFontDirectory("../../samples/fonts");

	_pSampleApp = new IFCSampleApp();

#ifdef USING_EXCHANGE
	{
		std::wstring buffer;
		buffer.resize(32767);
		if (GetEnvironmentVariable(L"HEXCHANGE_INSTALL_DIR", &buffer[0], static_cast<DWORD>(buffer.size())))
		{
			std::wstringstream bin_dir;
			bin_dir << buffer.data() << L"/bin";
#	ifdef WIN64
			bin_dir << L"/win64\0";
#	else
			bin_dir << L"/win32\0";
#	endif
			_world->SetExchangeLibraryDirectory(HPS::UTF8(bin_dir.str().data()));
		}
	}
#endif

#ifdef USING_PUBLISH
	{
		std::wstring buffer;
		buffer.resize(32767);
		if (GetEnvironmentVariable(L"HPUBLISH_INSTALL_DIR", &buffer[0], static_cast<DWORD>(buffer.size())))
		{
			std::wstringstream resource_dir;
			resource_dir << buffer.data() << L"/bin/resource";
			_world->SetPublishResourceDirectory(HPS::UTF8(resource_dir.str().data()));
		}
	}

#endif

#ifdef USING_PARASOLID
	{
		std::wstring buffer;
		buffer.resize(32767);
		if (GetEnvironmentVariable(L"PARASOLID_INSTALL_DIR", &buffer[0], static_cast<DWORD>(buffer.size())))
		{
			std::wstring subdir;
#	ifdef WIN64
#		if	(_MSC_VER >= 1900)
			subdir = L"/base64_v140/";
#		endif
#	else
#		if	(_MSC_VER >= 1900)
			subdir = L"/base32_v140/";
#		endif
#	endif
			std::wstringstream parasolid_schema_dir;
			parasolid_schema_dir << buffer.data() << subdir << "schema";
			_world->SetParasolidSchemaDirectory(HPS::UTF8(parasolid_schema_dir.str().data()));
		}
	}
#endif

	// Common MFC Initialization
	INITCOMMONCONTROLSEX		InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinAppEx::InitInstance();
	if (!AfxOleInit())
	{
		AfxMessageBox(IDP_OLE_INIT_FAILED);
		return FALSE;
	}

	EnableTaskbarInteraction(FALSE);

	SetRegistryKey(_T("Tech Soft 3D"));
	LoadStdProfileSettings();

	InitContextMenuManager();
	InitKeyboardManager();
	InitTooltipManager();
	CMFCToolTipInfo ttParams;
	ttParams.m_bVislManagerTheme = TRUE;
	theApp.GetTooltipManager()->SetTooltipParams(AFX_TOOLTIP_TYPE_ALL,
		RUNTIME_CLASS(CMFCToolTipCtrl), &ttParams);

	CSingleDocTemplate* pDocTemplate;
	pDocTemplate = new CSingleDocTemplate(
		IDR_MAINFRAME,
		RUNTIME_CLASS(CHPSDoc),
		RUNTIME_CLASS(CHPSFrame),       // main SDI frame window
		RUNTIME_CLASS(CHPSView));
	if (!pDocTemplate)
		return FALSE;
	AddDocTemplate(pDocTemplate);

	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);
	EnableShellOpen();
	if (!ProcessShellCommand(cmdInfo))
		return FALSE;

	m_pMainWnd->ShowWindow(SW_SHOW);
	m_pMainWnd->UpdateWindow();
	m_pMainWnd->DragAcceptFiles();

	Unregister();

	return TRUE;
}

int CHPSApp::ExitInstance()
{
	// Free HPS::World resources
 	ASSERT(_world);
 	delete _world;
 	_world = NULL;

	AfxOleTerm(FALSE);
	return CWinAppEx::ExitInstance();
}

void CHPSApp::addToRecentFileList(CString path)
{
	AddToRecentFileList(path);
}

void CHPSApp::setFontDirectory(const char* fontDir)
{
	if(_world && fontDir) _world->SetFontDirectory(fontDir);
}