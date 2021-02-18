#include "stdafx.h"
#include "CHPSConfigurationPane.h"
#include "CHPSDoc.h"
#include "resource.h"

CHPSConfigurationPane::CHPSConfigurationPane() {}

CHPSConfigurationPane::~CHPSConfigurationPane() {}

BEGIN_MESSAGE_MAP(CHPSConfigurationPane, CDockablePane)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_PAINT()
	ON_NOTIFY(NM_DBLCLK, IDC_CONFIGURATION_TREE, OnConfigurationTreeDblClick)
END_MESSAGE_MAP()


int CHPSConfigurationPane::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CDockablePane::OnCreate(lpCreateStruct) == -1)
		return -1;

	CRect rectDummy;
	rectDummy.SetRectEmpty();

	// Create view:
	const DWORD dwStyle = WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_NOTOOLTIPS;

	if (!configurationTree.Create(dwStyle, rectDummy, this, IDC_CONFIGURATION_TREE))
	{
		TRACE0("Failed to create configuration tree\n");
		return -1;      // fail to create
	}

	configurationTree.ModifyStyle(0, TVS_SHOWSELALWAYS);

	if (GetSafeHwnd() == NULL)
	{
		return -1;
	}

	CRect rectClient;
	GetClientRect(rectClient);

	//insert root item
	TVINSERTSTRUCT tvInsert;
	tvInsert.hParent = NULL;
	tvInsert.hInsertAfter = NULL;
	tvInsert.item.mask = TVIF_TEXT;
	tvInsert.item.pszText = _T("No Configurations");

	configurationTree.InsertItem(&tvInsert);

	configurationTree.SetWindowPos(NULL, rectClient.left + 1, rectClient.top + 1, rectClient.Width() - 2, rectClient.Height() - 2, SWP_NOACTIVATE | SWP_NOZORDER);

	return 0;
}


void CHPSConfigurationPane::OnSize(UINT nType, int cx, int cy)
{
	CDockablePane::OnSize(nType, cx, cy);

	CRect rectClient;
	GetClientRect(rectClient);

	configurationTree.SetWindowPos(NULL, rectClient.left + 1, rectClient.top + 1, rectClient.Width() - 2, rectClient.Height() - 2, SWP_NOACTIVATE | SWP_NOZORDER);
}


void CHPSConfigurationPane::OnPaint()
{
	CPaintDC dc(this); // device context for painting

	CRect rectTree;
	configurationTree.GetWindowRect(rectTree);
	ScreenToClient(rectTree);

	rectTree.InflateRect(1, 1);
	dc.Draw3dRect(rectTree, ::GetSysColor(COLOR_3DSHADOW), ::GetSysColor(COLOR_3DSHADOW));
}

void CHPSConfigurationPane::Init()
{
#ifdef USING_EXCHANGE
	CHPSDoc * doc = GetCHPSDoc();
	HPS::CADModel cadModel = doc->GetCADModel();

	if (cadModel.Empty())
		Flush();
	else
	{
		HPS::UTF8 fileFormat = HPS::StringMetadata(cadModel.GetMetadata("FileFormat")).GetValue();
		if (fileFormat == "SolidWorks" || fileFormat == "CATIA V4" || fileFormat == "I-DEAS")
		{
			// only these formats have a concept of configurations

			HPS::Exchange::CADModel exchangeCADModel(cadModel);
			HPS::Exchange::ConfigurationArray configurations;
			if (fileFormat == "SolidWorks")
				configurations = exchangeCADModel.GetConfigurations();
			else
			{
				HPS::UTF8 filename = HPS::StringMetadata(cadModel.GetMetadata("Filename")).GetValue();
				configurations = HPS::Exchange::File::GetConfigurations(filename);
			}

			if (configurations.empty())
				Flush();
			else
			{
				configurationTree.DeleteAllItems();

				TVINSERTSTRUCT tvInsert;
				tvInsert.hParent = NULL;
				tvInsert.hInsertAfter = NULL;
				tvInsert.item.mask = TVIF_TEXT;
				tvInsert.item.pszText = _T("Configurations");
				HTREEITEM root = configurationTree.InsertItem(&tvInsert);

				for (auto const & config : configurations)
					InsertConfigurationInTree(root, config);

				configurationTree.Expand(root, TVE_EXPAND);

				HPS::UTF8Array currentConfiguration = exchangeCADModel.GetCurrentConfiguration();
				if (!currentConfiguration.empty())
				{
					HTREEITEM selectedConfigurationItem = FindConfigurationInTree(currentConfiguration, root);
					if (selectedConfigurationItem != nullptr)
					{
						configurationTree.SetItemState(selectedConfigurationItem, TVIS_BOLD, TVIS_BOLD);
						configurationTree.Expand(configurationTree.GetParentItem(selectedConfigurationItem), TVE_EXPAND);
					}
				}
			}
		}
		else
			Flush();
	}
#endif
}

void CHPSConfigurationPane::Flush()
{
	configurationTree.DeleteAllItems();

	ASSERT(configurationTree.GetCount() == 0);
	TVINSERTSTRUCT tvi;
	tvi.hParent = NULL;
	tvi.hInsertAfter = NULL;
	tvi.item.mask = TVIF_TEXT;
	tvi.item.pszText = _T("No Configurations");
	configurationTree.InsertItem(&tvi);
}

#ifdef USING_EXCHANGE
void CHPSConfigurationPane::InsertConfigurationInTree(HTREEITEM const & root, HPS::Exchange::Configuration const & configuration)
{
	HPS::UTF8 configName = configuration.GetName();
	HPS::WCharArray wchars;
	configName.ToWStr(wchars);
	HTREEITEM configItem = configurationTree.InsertItem(TVIF_TEXT, wchars.data(), 0, 0, 0, 0, 0, root, NULL);

	HPS::Exchange::ConfigurationArray subConfigurations = configuration.GetSubconfigurations();
	for (auto const & subConfig : subConfigurations)
		InsertConfigurationInTree(configItem, subConfig);
}

HTREEITEM CHPSConfigurationPane::FindConfigurationInTree(HPS::UTF8Array const & configuration, HTREEITEM const & root)
{
	if (configurationTree.ItemHasChildren(root) == FALSE)
		return nullptr;

	HTREEITEM childItem = configurationTree.GetChildItem(root);
	while (childItem != nullptr)
	{
		HPS::WCharArray wchars;
		configuration[0].ToWStr(wchars);
		if (configurationTree.GetItemText(childItem) == wchars.data())
		{
			if (configuration.size() == 1)
				return childItem;
			else
				return FindConfigurationInTree(HPS::UTF8Array(configuration.begin() + 1, configuration.end()), childItem);
		}

		childItem = configurationTree.GetNextItem(childItem, TVGN_NEXT);
	}

	return nullptr;
}

HPS::UTF8Array CHPSConfigurationPane::GetSelectedConfiguration()
{
	HPS::UTF8Array newlySelectedConfiguration;

	HTREEITEM item = configurationTree.GetSelectedItem();
	do
	{
		CString text = configurationTree.GetItemText(item);
		if (text == L"Configurations" || text == L"No Configurations")
			break;
		newlySelectedConfiguration.emplace_back(text.GetString());
	} while ((item = configurationTree.GetParentItem(item)) != nullptr);

	std::reverse(newlySelectedConfiguration.begin(), newlySelectedConfiguration.end());

	return newlySelectedConfiguration;
}
#endif

void CHPSConfigurationPane::OnConfigurationTreeDblClick(NMHDR * /*pNMHDR*/, LRESULT * /*pResult*/) 
{
	//NM_TREEVIEW * pNMTreeView = (NM_TREEVIEW *)pNMHDR;

	HTREEITEM item;
	UINT flags;
	DWORD pos = GetMessagePos();
	CPoint point(GET_X_LPARAM(pos), GET_Y_LPARAM(pos));
	configurationTree.ScreenToClient(&point);
	item = configurationTree.HitTest(point, &flags);
	if (item != NULL && (flags & TVHT_ONITEMLABEL))
	{
#	ifdef USING_EXCHANGE
		HPS::UTF8Array configuration = GetSelectedConfiguration();
		GetCHPSDoc()->ImportConfiguration(configuration);
#	endif
	}
}

BOOL CHPSConfigurationPane::OnShowControlBarMenu(CPoint point)
{
	// only show the context menu when clicking on the caption

	CRect rc;
	GetClientRect(&rc);
	ClientToScreen(&rc);
	if (rc.PtInRect(point))
		return TRUE;

	return CDockablePane::OnShowControlBarMenu(point);
}
