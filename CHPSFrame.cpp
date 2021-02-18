#include "stdafx.h"
#include "CHPSApp.h"
#include "CHPSFrame.h"
#include "SandboxHighlightOp.h"
#define  COMP_PROPS_PANE 1
#define COMP_QUANTS_DLG 1


IMPLEMENT_DYNCREATE(CHPSFrame, CFrameWndEx)

BEGIN_MESSAGE_MAP(CHPSFrame, CFrameWndEx)
	ON_WM_CREATE()
	ON_COMMAND_RANGE(ID_VIEW_APPLOOK_WIN_2000, ID_VIEW_APPLOOK_WINDOWS_7, &CHPSFrame::OnApplicationLook)
	ON_UPDATE_COMMAND_UI_RANGE(ID_VIEW_APPLOOK_WIN_2000, ID_VIEW_APPLOOK_WINDOWS_7, &CHPSFrame::OnUpdateApplicationLook)
	ON_MESSAGE(WM_MFC_SANDBOX_INITIALIZE_BROWSERS, &CHPSFrame::InitializeBrowsers)
	ON_MESSAGE(WM_MFC_SANDBOX_ADD_PROPERTY, &CHPSFrame::AddProperty)
	ON_MESSAGE(WM_MFC_SANDBOX_FLUSH_PROPERTIES, &CHPSFrame::FlushProperties)
	ON_MESSAGE(WM_MFC_SANDBOX_UNSET_ATTRIBUTE, &CHPSFrame::UnsetAttribute)
	ON_COMMAND(ID_OPERATOR_SEGMENT_BROWSER, &CHPSFrame::OnModesSegmentBrowser)
	ON_UPDATE_COMMAND_UI(ID_OPERATOR_SEGMENT_BROWSER, &CHPSFrame::OnUpdateModesSegmentBrowser)
	ON_COMMAND(ID_OPERATOR_MODEL_BROWSER, &CHPSFrame::OnModesModelBrowser)
	ON_UPDATE_COMMAND_UI(ID_OPERATOR_MODEL_BROWSER, &CHPSFrame::OnUpdateModesModelBrowser)
	ON_COMMAND(ID_COMBO_SEL_LEVEL, &CHPSFrame::OnComboSelectionLevel)
	ON_MESSAGE(WM_MFC_SANDBOX_COMPONENT_QUANTIFY, &CHPSFrame::OnComponentQuantify)

END_MESSAGE_MAP()

CHPSFrame::CHPSFrame()
{
	theApp.SetAppLook(theApp.GetInt(_T("ApplicationLook"), ID_VIEW_APPLOOK_WINDOWS_7));
}

CHPSFrame::~CHPSFrame()
{
}

int CHPSFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFrameWndEx::OnCreate(lpCreateStruct) == -1)
		return -1;

	OnApplicationLook(theApp.GetAppLook());
	m_wndRibbonBar.Create(this);
	m_wndRibbonBar.LoadFromResource(IDR_RIBBON);

	// enable Visual Studio 2005 style docking window behavior
	CDockingManager::SetDockingMode(DT_SMART);
	// enable Visual Studio 2005 style docking window auto-hide behavior
	EnableAutoHidePanes(CBRS_ALIGN_ANY);

	// Switch the order of document name and application name on the window title bar. This
	// improves the usability of the taskbar because the document name is visible with the thumbnail.
	ModifyStyle(0, FWS_PREFIXTITLE);

	DWORD dwStyle = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CBRS_FLOAT_MULTI;

	if (!m_segmentBrowserPane.Create(L"Segment Browser", this, CRect(0, 0, 250, 250), TRUE, ID_VIEW_SEGMENTBROWSER, dwStyle | CBRS_LEFT))
	{
		TRACE0("Failed to create Segment Browser pane\n");
		return FALSE; // failed to create
	}

	if (!m_propertiesPane.Create(_T("Properties"), this, CRect(0, 0, 250, 250), TRUE, ID_VIEW_PROPERTIES, dwStyle | CBRS_LEFT))
	{
		TRACE0("Failed to create Properties pane\n");
		return FALSE; // failed to create
	}

#ifdef COMP_PROPS_PANE

	if (!m_componentPropertiesPane.Create(_T("Component Properties"), this, CRect(0, 0, 250, 250), TRUE, ID_PANE_COMPONENT_PROPS, dwStyle | CBRS_LEFT))
	{
		TRACE0("Failed to create Component Properties pane\n");
		return FALSE; // failed to create
	}
#endif

#ifdef COMP_QUANTS_DLG
	if (!m_componentQuantitiesDlg.Create(IDD_DLG_QUANTITY ))
	{
		TRACE0("Failed to create Component Quantities dlg\n");
		return FALSE; // failed to create
	}

#endif


	if (!m_modelBrowserPane.Create(L"Model Browser", this, CRect(0, 0, 250, 250), TRUE, ID_VIEW_MODELBROWSER, dwStyle | CBRS_RIGHT))
	{
		TRACE0("Failed to create Model Browser pane\n");
		return FALSE; // failed to create
	}

	if (!m_configurationPane.Create(L"Configurations", this, CRect(0, 0, 250, 250), TRUE, ID_VIEW_CONFIGURATIONS, dwStyle | CBRS_RIGHT))
	{
		TRACE0("Failed to create Configurations pane\n");
		return FALSE; // failed to create
	}


	if (!m_tabbedPane.Create(L"", this, CRect(0, 0, 250, 250), TRUE, ID_VIEW_TABBED_BROWSER, dwStyle | CBRS_RIGHT))
	{
		TRACE0("Failed to create Tabbed pane\n");
		return FALSE; // failed to create
	}

	EnableDocking(CBRS_LEFT | CBRS_RIGHT);

	m_tabbedPane.EnableDocking(CBRS_LEFT | CBRS_RIGHT);
	m_modelBrowserPane.EnableDocking(CBRS_LEFT | CBRS_RIGHT);
	m_configurationPane.EnableDocking(CBRS_LEFT | CBRS_RIGHT);
	DockPane(&m_tabbedPane);
	m_tabbedPane.AddTab(&m_modelBrowserPane, TRUE, TRUE);
	m_tabbedPane.AddTab(&m_configurationPane, TRUE, FALSE);

	m_tabbedPane.ShowTab(&m_modelBrowserPane, FALSE, FALSE, FALSE);
	m_tabbedPane.ShowTab(&m_configurationPane, FALSE, FALSE, FALSE);

	m_segmentBrowserPane.EnableDocking(CBRS_LEFT | CBRS_RIGHT);
	DockPane(&m_segmentBrowserPane);

	m_propertiesPane.EnableDocking(CBRS_ALIGN_ANY);
	m_propertiesPane.DockToWindow(&m_segmentBrowserPane, CBRS_ALIGN_BOTTOM);

#ifdef COMP_PROPS_PANE
	m_componentPropertiesPane.EnableDocking(CBRS_ALIGN_ANY);
	m_componentPropertiesPane.DockToWindow(&m_tabbedPane, CBRS_ALIGN_LEFT);
	m_componentPropertiesPane.ShowPane(FALSE, FALSE, FALSE);
#endif

	m_segmentBrowserPane.ShowPane(FALSE, FALSE, FALSE);
	m_propertiesPane.ShowPane(FALSE, FALSE, FALSE);

	m_tabbedPane.ShowPane(FALSE, FALSE, FALSE);

	auto element = m_wndRibbonBar.FindByID(ID_COMBO_SEL_LEVEL);
	if (element != nullptr)
	{
		CMFCRibbonComboBox * combo = reinterpret_cast<CMFCRibbonComboBox *>(element);
		combo->AddItem(_T("Segment"));
		combo->AddItem(_T("Entity"));
		combo->AddItem(_T("Subentity"));
		combo->SelectItem((int)SandboxHighlightOperator::SelectionLevel);
	}

	return 0;
}

BOOL CHPSFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if(!CFrameWndEx::PreCreateWindow(cs))
		return FALSE;

	return TRUE;
}

void CHPSFrame::OnApplicationLook(UINT id)
{
	CWaitCursor wait;

	theApp.SetAppLook(id);

	switch (theApp.GetAppLook())
	{
	case ID_VIEW_APPLOOK_WIN_2000:
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManager));
		m_wndRibbonBar.SetWindows7Look(FALSE);
		break;

	case ID_VIEW_APPLOOK_OFF_XP:
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerOfficeXP));
		m_wndRibbonBar.SetWindows7Look(FALSE);
		break;

	case ID_VIEW_APPLOOK_WIN_XP:
		CMFCVisualManagerWindows::m_b3DTabsXPTheme = TRUE;
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));
		m_wndRibbonBar.SetWindows7Look(FALSE);
		break;

	case ID_VIEW_APPLOOK_OFF_2003:
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerOffice2003));
		CDockingManager::SetDockingMode(DT_SMART);
		m_wndRibbonBar.SetWindows7Look(FALSE);
		break;

	case ID_VIEW_APPLOOK_VS_2005:
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerVS2005));
		CDockingManager::SetDockingMode(DT_SMART);
		m_wndRibbonBar.SetWindows7Look(FALSE);
		break;

	case ID_VIEW_APPLOOK_VS_2008:
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerVS2008));
		CDockingManager::SetDockingMode(DT_SMART);
		m_wndRibbonBar.SetWindows7Look(FALSE);
		break;

	case ID_VIEW_APPLOOK_WINDOWS_7:
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows7));
		CDockingManager::SetDockingMode(DT_SMART);
		m_wndRibbonBar.SetWindows7Look(TRUE);
		break;

	default:
		switch (theApp.GetAppLook())
		{
		case ID_VIEW_APPLOOK_OFF_2007_BLUE:
			CMFCVisualManagerOffice2007::SetStyle(CMFCVisualManagerOffice2007::Office2007_LunaBlue);
			break;

		case ID_VIEW_APPLOOK_OFF_2007_BLACK:
			CMFCVisualManagerOffice2007::SetStyle(CMFCVisualManagerOffice2007::Office2007_ObsidianBlack);
			break;

		case ID_VIEW_APPLOOK_OFF_2007_SILVER:
			CMFCVisualManagerOffice2007::SetStyle(CMFCVisualManagerOffice2007::Office2007_Silver);
			break;

		case ID_VIEW_APPLOOK_OFF_2007_AQUA:
			CMFCVisualManagerOffice2007::SetStyle(CMFCVisualManagerOffice2007::Office2007_Aqua);
			break;
		}

		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerOffice2007));
		CDockingManager::SetDockingMode(DT_SMART);
		m_wndRibbonBar.SetWindows7Look(FALSE);
	}

	RedrawWindow(NULL, NULL, RDW_ALLCHILDREN | RDW_INVALIDATE | RDW_UPDATENOW | RDW_FRAME | RDW_ERASE);
	theApp.WriteInt(_T("ApplicationLook"), theApp.GetAppLook());
}

void CHPSFrame::OnUpdateApplicationLook(CCmdUI* pCmdUI)
{
	pCmdUI->SetRadio(theApp.GetAppLook() == pCmdUI->m_nID);
}

LRESULT CHPSFrame::InitializeBrowsers(WPARAM /*w*/, LPARAM /*l*/)
{
	m_segmentBrowserPane.Init();
	m_modelBrowserPane.Init();
	m_configurationPane.Init();
	m_propertiesPane.Flush();
	return 0;
}

LRESULT CHPSFrame::AddProperty(WPARAM w, LPARAM l)
{
	auto treeItem = reinterpret_cast<MFCSceneTreeItem *>(w);
	auto itemType = static_cast<HPS::SceneTree::ItemType>(l);
	if (itemType == HPS::SceneTree::ItemType::None)
		m_propertiesPane.AddProperty(treeItem);
	else
		m_propertiesPane.AddProperty(treeItem, itemType);
	return 0;
}

LRESULT CHPSFrame::FlushProperties(WPARAM /*w*/, LPARAM /*l*/)
{
	m_propertiesPane.Flush();
	return 0;
}

LRESULT CHPSFrame::UnsetAttribute(WPARAM w, LPARAM /*l*/)
{
	auto treeItem = reinterpret_cast<MFCSceneTreeItem *>(w);
	m_propertiesPane.UnsetAttribute(treeItem);
	return 0;
}

LRESULT CHPSFrame::OnComponentQuantify(WPARAM/* w*/, LPARAM /*l*/)
{
	m_componentQuantitiesDlg.AddComponent(&m_TaggedComponent);
	return 0;
}


void CHPSFrame::OnModesSegmentBrowser()
{
	if (m_segmentBrowserPane.IsVisible() || m_propertiesPane.IsVisible())
	{
		m_segmentBrowserPane.ShowPane(FALSE, FALSE, FALSE);
		m_propertiesPane.ShowPane(FALSE, FALSE, FALSE);
	}
	else
	{
		m_segmentBrowserPane.ShowPane(TRUE, TRUE, TRUE);
		m_propertiesPane.ShowPane(FALSE, FALSE, FALSE);
	}

	Invalidate();
	RecalcLayout();
}

void CHPSFrame::OnUpdateModesSegmentBrowser(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_segmentBrowserPane.IsVisible());
}

void CHPSFrame::OnModesModelBrowser()
{
	if (m_modelBrowserPane.IsVisible() || m_configurationPane.IsVisible())
	{
		m_tabbedPane.ShowTab(&m_modelBrowserPane, FALSE, FALSE, FALSE);
		m_tabbedPane.ShowTab(&m_configurationPane, FALSE, FALSE, FALSE);
	}
	else
	{
		m_tabbedPane.ShowTab(&m_modelBrowserPane, TRUE, TRUE, TRUE);
		m_tabbedPane.ShowTab(&m_configurationPane, TRUE, TRUE, TRUE);
	}

	Invalidate();
	RecalcLayout();
	m_tabbedPane.RecalcLayout();
}

void CHPSFrame::OnUpdateModesModelBrowser(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_modelBrowserPane.IsVisible());
}

void CHPSFrame::SetPropertiesPaneVisibility(bool state)
{
	BOOL flag = (state ? TRUE : FALSE);
	m_propertiesPane.ShowPane(flag, flag, flag);

	if (state)
		m_propertiesPane.DockToWindow(&m_segmentBrowserPane, CBRS_ALIGN_BOTTOM);
}

bool CHPSFrame::GetPropertiesPaneVisibility() const
{
	return (m_propertiesPane.IsVisible() != FALSE);
}

//
// Component Properties Pane
//
void CHPSFrame::SetComponentPropertiesPaneVisibility(bool state)
{
	BOOL flag = (state ? TRUE : FALSE);
	m_componentPropertiesPane.ShowPane(flag, flag, flag);

	if (state)
	{
		if ( m_tabbedPane.IsPaneVisible())
			m_componentPropertiesPane.DockToWindow(&m_tabbedPane, CBRS_ALIGN_BOTTOM);
	}
}

bool CHPSFrame::GetComponentPropertiesPaneVisibility() const
{
	return (m_componentPropertiesPane.IsVisible() != FALSE);
}

CHPSComponentPropertiesPane *CHPSFrame::GetComponentPropertiesPane()  const
{
	return const_cast<CHPSComponentPropertiesPane *>(&m_componentPropertiesPane);
}

//
// Component Quantities Dialog
//
void CHPSFrame::SetComponentQuantitiesDialogVisibility(bool state)
{
	if ( state )
		m_componentQuantitiesDlg.ShowWindow(SW_SHOW);
	else
		m_componentQuantitiesDlg.ShowWindow(SW_HIDE);

}

bool CHPSFrame::GetComponentQuantitiesDialogVisibility() const
{
	return (m_componentQuantitiesDlg.IsWindowVisible() != FALSE);
}


void CHPSFrame::OnComboSelectionLevel()
{
	auto comboBox = (CMFCRibbonComboBox const *)m_wndRibbonBar.FindByID(ID_COMBO_SEL_LEVEL);
	
	if (comboBox)
	{		
		int selection = comboBox->GetCurSel();
		SandboxHighlightOperator::SelectionLevel = (HPS::Selection::Level)selection;
	}
}