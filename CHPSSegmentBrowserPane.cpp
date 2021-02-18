#include "stdafx.h"
#include "CHPSApp.h"
#include "CHPSDoc.h"
#include "CHPSFrame.h"
#include "CHPSView.h"
#include "CHPSSegmentBrowserPane.h"






CHPSSegmentBrowserPane::CHPSSegmentBrowserPane()
{
}


CHPSSegmentBrowserPane::~CHPSSegmentBrowserPane()
{
}


BEGIN_MESSAGE_MAP(CHPSSegmentBrowserPane, CDockablePane)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_PAINT()
	ON_WM_SETFOCUS()
	ON_WM_LBUTTONDOWN()
	ON_NOTIFY(TVN_ITEMEXPANDING, IDC_SEGMENT_TREE, OnItemExpanding)
	ON_NOTIFY(TVN_SELCHANGED, IDC_SEGMENT_TREE, OnSelectionChanged)
	ON_BN_CLICKED(IDB_PROPERTIES_CHECK_BOX, OnProperitesCheckBox)
	ON_UPDATE_COMMAND_UI(IDB_PROPERTIES_CHECK_BOX, OnUpdatePropertiesCheckBox)
	ON_CBN_SELCHANGE(IDC_ROOT_COMBO_BOX, OnSelectedRoot)
END_MESSAGE_MAP()



int CHPSSegmentBrowserPane::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CDockablePane::OnCreate(lpCreateStruct) == -1)
		return -1;

	CRect rectDummy;
	rectDummy.SetRectEmpty();

	// Create view:
	const DWORD dwViewStyle = WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS;

	if (!m_TreeCtrl.Create(dwViewStyle, rectDummy, this, IDC_SEGMENT_TREE))
	{
		TRACE0("Failed to create segment tree\n");
		return -1;      // fail to create
	}

	m_ImageList.Create(IDB_SEGMENT_BROWSER, 16, 1, RGB(255,255,255));
	m_TreeCtrl.SetImageList(&m_ImageList, TVSIL_STATE);

	// maintain a constant text size
	int const textSizePoints = 10;
	int dpiY = GetDeviceCaps(GetDC()->GetSafeHdc(), LOGPIXELSY);
	int fontHeightPixels = MulDiv(textSizePoints, dpiY, 72);	// there are 72 points per inch

	VERIFY(m_Font.CreateFont(
		-fontHeightPixels,         // nHeight
		0,                         // nWidth
		0,                         // nEscapement
		0,                         // nOrientation
		FW_NORMAL,                 // nWeight
		FALSE,                     // bItalic
		FALSE,                     // bUnderline
		0,                         // cStrikeOut
		ANSI_CHARSET,              // nCharSet
		OUT_DEFAULT_PRECIS,        // nOutPrecision
		CLIP_DEFAULT_PRECIS,       // nClipPrecision
		DEFAULT_QUALITY,           // nQuality
		DEFAULT_PITCH | FF_SWISS,  // nPitchAndFamily
		_T("Arial")));                 // lpszFacename

	if (!m_ComboBox.Create(WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, rectDummy, this, IDC_ROOT_COMBO_BOX))
	{
		TRACE0("Failed to create combo box\n");
		return -1;
	}
	m_ComboBox.AddString(_T("Model"));
	m_ComboBox.AddString(_T("View"));
	m_ComboBox.AddString(_T("Layout"));
	m_ComboBox.AddString(_T("Canvas"));

	m_ComboBox.SetCurSel(0);
	m_ComboBox.SetFont(&m_Font, TRUE);

	if (!m_CheckBox.Create(_T("Properties"), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, rectDummy, this, IDB_PROPERTIES_CHECK_BOX))
	{
		TRACE0("Failed to create check box\n");
		return -1;
	}
	m_CheckBox.SetFont(&m_Font);

	AdjustLayout();

	return 0;
}

void CHPSSegmentBrowserPane::OnSize(UINT nType, int cx, int cy)
{
	CDockablePane::OnSize(nType, cx, cy);
	AdjustLayout();
}

void CHPSSegmentBrowserPane::Init()
{
	CHPSDoc * doc = GetCHPSDoc();
	m_pSceneTree = std::make_shared<MFCSceneTree>(doc->GetCHPSView()->GetCanvas(), &m_TreeCtrl);
	OnSelectedRoot();
}

void CHPSSegmentBrowserPane::AdjustLayout()
{
	if (GetSafeHwnd() == NULL)
	{
		return;
	}

	CRect rectClient;
	GetClientRect(rectClient);

	SIZE checkBoxSize;
	m_CheckBox.GetIdealSize(&checkBoxSize);

	// Calculate the size needed to host the largest string in the combo box
	CDC * dc = m_ComboBox.GetDC();
	TEXTMETRIC tm;
	dc->GetTextMetrics(&tm);
	CSize sz = dc->GetTextExtent(L"Layout");
	sz.cx += tm.tmAveCharWidth;
	m_ComboBox.ReleaseDC(dc);

	sz.cx += GetSystemMetrics(SM_CXVSCROLL) + 2 * GetSystemMetrics(SM_CXEDGE);
	int dpi_x = GetDeviceCaps(GetDC()->GetSafeHdc(), LOGPIXELSX);
	sz.cx = MulDiv(sz.cx, dpi_x, 96);

	int comboWidth = sz.cx;
	int comboHeight = sz.cy;

	m_ComboBox.SetWindowPos(NULL, rectClient.left + 3, rectClient.top + 2, comboWidth, comboHeight, SWP_NOACTIVATE | SWP_NOZORDER);
	m_CheckBox.SetWindowPos(NULL, rectClient.left + comboWidth + 20, rectClient.top + 4, checkBoxSize.cx, checkBoxSize.cy, SWP_NOACTIVATE | SWP_NOZORDER);

	CRect rectCheckBox;
	m_CheckBox.GetClientRect(rectCheckBox);
	m_TreeCtrl.SetWindowPos(NULL, rectClient.left + 3, rectClient.top + (int)(rectCheckBox.Height() * 1.5f), rectClient.Width() - 4, rectClient.Height() - rectCheckBox.Height() - 6, SWP_NOACTIVATE | SWP_NOZORDER);
}

void CHPSSegmentBrowserPane::OnPaint()
{
	CPaintDC dc(this); // device context for painting

	CRect paneRect;
	GetWindowRect(paneRect);
	ScreenToClient(paneRect);

	CBrush bg;
	bg.CreateStockObject(WHITE_BRUSH);
	dc.FillRect(&paneRect, &bg);

	CRect rectTree;
	m_TreeCtrl.GetWindowRect(rectTree);
	ScreenToClient(rectTree);

	rectTree.InflateRect(1, 1);
	dc.Draw3dRect(rectTree, ::GetSysColor(COLOR_3DSHADOW), ::GetSysColor(COLOR_3DSHADOW));
}

void CHPSSegmentBrowserPane::OnSetFocus(CWnd* pOldWnd)
{
	CDockablePane::OnSetFocus(pOldWnd);

	m_TreeCtrl.SetFocus();
}

void CHPSSegmentBrowserPane::OnItemExpanding(NMHDR * pNMHDR, LRESULT * pResult)
{
	NM_TREEVIEW * pNMTreeView = (NM_TREEVIEW *)pNMHDR;
	TVITEM treeItem = pNMTreeView->itemNew;
	MFCSceneTreeItem * sceneItem = (MFCSceneTreeItem *)treeItem.lParam;
	if (pNMTreeView->action == TVE_EXPAND)
		sceneItem->Expand();
	else
		sceneItem->Collapse();
	*pResult = 0;
}

void CHPSSegmentBrowserPane::OnSelectionChanged(NMHDR * pNMHDR, LRESULT * pResult)
{
	NM_TREEVIEW * pNMTreeView = (NM_TREEVIEW *)pNMHDR;
	TVITEM treeItem = pNMTreeView->itemNew;
	MFCSceneTreeItem * sceneItem = (MFCSceneTreeItem *)treeItem.lParam;
	AfxGetApp()->m_pMainWnd->PostMessageW(WM_MFC_SANDBOX_ADD_PROPERTY, reinterpret_cast<WPARAM>(sceneItem), 0);
	*pResult = 0;
}

BOOL CHPSSegmentBrowserPane::OnShowControlBarMenu(CPoint pt)
{
	CRect rc;
	GetClientRect(&rc);
	ClientToScreen(&rc);
	if (rc.PtInRect(pt))
		return TRUE;
	return CDockablePane::OnShowControlBarMenu(pt);
}

void CHPSSegmentBrowserPane::OnProperitesCheckBox()
{
	CButton * checkBox = (CButton *) GetDlgItem(IDB_PROPERTIES_CHECK_BOX);
	bool current = (checkBox->GetCheck() == BST_CHECKED ? true : false);
	CHPSFrame * frame = GetCHPSDoc()->GetCHPSFrame();
	frame->SetPropertiesPaneVisibility(current);
}

void CHPSSegmentBrowserPane::OnUpdatePropertiesCheckBox(CCmdUI *pCmdUI)
{
	CHPSFrame * frame = GetCHPSDoc()->GetCHPSFrame();
	pCmdUI->SetCheck(frame->GetPropertiesPaneVisibility());
}

void CHPSSegmentBrowserPane::OnSelectedRoot()
{
	enum Root
	{
		Model = 0,
		View,
		Layout,
		Canvas
	};

	CComboBox * comboBox = (CComboBox *)GetDlgItem(IDC_ROOT_COMBO_BOX);
	int currentSelection = comboBox->GetCurSel();
	CHPSDoc * doc = GetCHPSDoc();

	HPS::Canvas canvas = doc->GetCHPSView()->GetCanvas();
	bool has_canvas = canvas.Type() != HPS::Type::None;
	HPS::Layout layout = canvas.GetAttachedLayout();
	bool has_layout = has_canvas && layout.Type() != HPS::Type::None;
	HPS::View view = (has_layout && layout.GetLayerCount() > 0 ? layout.GetFrontView() : HPS::View());
	bool has_view = has_layout && view.Type() != HPS::Type::None && view.GetSegmentKey().Type() != HPS::Type::None;
	HPS::Model model = (has_view ? view.GetAttachedModel() : HPS::Model());
	bool has_model = has_view && model.Type() != HPS::Type::None && model.GetSegmentKey().Type() != HPS::Type::None;

	HPS::SceneTreeItemPtr root;
	switch (currentSelection)
	{
		case Model:
		{
			if (has_model)
				root = std::make_shared<MFCSceneTreeItem>(m_pSceneTree, model);
		}	break;

		case View:
		{
			if (has_view)
				root = std::make_shared<MFCSceneTreeItem>(m_pSceneTree, view);
		}	break;

		case Layout:
		{
			if (has_layout)
				root = std::make_shared<MFCSceneTreeItem>(m_pSceneTree, layout);
		}	break;

		case Canvas:
		{
			if (has_canvas)
				root = std::make_shared<MFCSceneTreeItem>(m_pSceneTree, canvas);
		}	break;

		default:
			ASSERT(0);
	}

	if (root)
		m_pSceneTree->SetRoot(root);
	else 
		m_pSceneTree->Flush();

	AfxGetApp()->m_pMainWnd->PostMessageW(WM_MFC_SANDBOX_FLUSH_PROPERTIES, 0, 0);
}




BEGIN_MESSAGE_MAP(CHPSTreeCtrl, CTreeCtrl)
	ON_WM_LBUTTONDOWN()
	ON_NOTIFY_REFLECT(NM_RCLICK, OnRClick)
	ON_WM_CONTEXTMENU()
	ON_COMMAND(ID_ADDATTRIBUTE_MATERIAL, OnAddMaterial)
	ON_COMMAND(ID_ADDATTRIBUTE_CAMERA, OnAddCamera)
	ON_COMMAND(ID_ADDATTRIBUTE_MODELLINGMATRIX, OnAddModellingMatrix)
	ON_COMMAND(ID_ADDATTRIBUTE_TEXTUREMATRIX, OnAddTextureMatrix)
	ON_COMMAND(ID_ADDATTRIBUTE_CULLING, OnAddCulling)
	ON_COMMAND(ID_ADDATTRIBUTE_CURVEATTRIBUTE, OnAddCurveAttribute)
	ON_COMMAND(ID_ADDATTRIBUTE_CYLINDERATTRIBUTE, OnAddCylinderAttribute)
	ON_COMMAND(ID_ADDATTRIBUTE_EDGEATTRIBUTE, OnAddEdgeAttribute)
	ON_COMMAND(ID_ADDATTRIBUTE_LIGHTINGATTRIBUTE, OnAddLightingAttribute)
	ON_COMMAND(ID_ADDATTRIBUTE_LINEATTRIBUTE, OnAddLineAttribute)
	ON_COMMAND(ID_ADDATTRIBUTE_MARKERATTRIBUTE, OnAddMarkerAttribute)
	ON_COMMAND(ID_ADDATTRIBUTE_SURFACEATTRIBUTE, OnAddSurfaceAttribute)
	ON_COMMAND(ID_ADDATTRIBUTE_SELECTABILITY, OnAddSelectability)
	ON_COMMAND(ID_ADDATTRIBUTE_SPHEREATTRIBUTE, OnAddSphereAttribute)
	ON_COMMAND(ID_ADDATTRIBUTE_SUBWINDOW, OnAddSubwindow)
	ON_COMMAND(ID_ADDATTRIBUTE_TEXTATTRIBUTE, OnAddTextAttribute)
	ON_COMMAND(ID_ADDATTRIBUTE_TRANSPARENCY, OnAddTransparency)
	ON_COMMAND(ID_ADDATTRIBUTE_VISIBILITY, OnAddVisibility)
	ON_COMMAND(ID_ADDATTRIBUTE_VISUALEFFECTS, OnAddVisualEffects)
	ON_COMMAND(ID_ADDATTRIBUTE_PERFORMANCE, OnAddPerformance)
	ON_COMMAND(ID_ADDATTRIBUTE_DRAWINGATTRIBUTE, OnAddDrawingAttribute)
	ON_COMMAND(ID_ADDATTRIBUTE_HIDDENLINEATTRIBUTE, OnAddHiddenLineAttribute)
	ON_COMMAND(ID_ADDATTRIBUTE_MATERIALPALETTE, OnAddMaterialPalette)
	ON_COMMAND(ID_ADDATTRIBUTE_CONTOURLINE, OnAddContourLine)
	ON_COMMAND(ID_ADDATTRIBUTE_CONDITION, OnAddCondition)
	ON_COMMAND(ID_ADDATTRIBUTE_BOUNDING, OnAddBounding)
	ON_COMMAND(ID_ADDATTRIBUTE_ATTRIBUTELOCK, OnAddAttributeLock)
	ON_COMMAND(ID_ADDATTRIBUTE_TRANSFORMMASK, OnAddTransformMask)
	ON_COMMAND(ID_ADDATTRIBUTE_COLORINTERPOLATION, OnAddColorInterpolation)
	ON_COMMAND(ID_ADDATTRIBUTE_CUTTINGSECTIONATTRIBUTE, OnAddCuttingSectionAttribute)
	ON_COMMAND(ID_ADDATTRIBUTE_PRIORITY, OnAddPriority)
	ON_COMMAND(ID_ATTRIBUTECONTEXTMENU_UNSET, OnUnsetAttribute)
	ON_WM_KEYDOWN()
END_MESSAGE_MAP()

CHPSTreeCtrl::CHPSTreeCtrl()
	: CTreeCtrl()
	, selectedSceneTreeItem(nullptr)
{
	searchTypeMap[HPS::SceneTree::ItemType::CuttingSectionGroup] = HPS::Search::Type::CuttingSection;
	searchTypeMap[HPS::SceneTree::ItemType::ShellGroup] = HPS::Search::Type::Shell;
	searchTypeMap[HPS::SceneTree::ItemType::MeshGroup] = HPS::Search::Type::Mesh;
	searchTypeMap[HPS::SceneTree::ItemType::GridGroup] = HPS::Search::Type::Grid;
	searchTypeMap[HPS::SceneTree::ItemType::NURBSSurfaceGroup] = HPS::Search::Type::NURBSSurface;
	searchTypeMap[HPS::SceneTree::ItemType::CylinderGroup] = HPS::Search::Type::Cylinder;
	searchTypeMap[HPS::SceneTree::ItemType::SphereGroup] = HPS::Search::Type::Sphere;
	searchTypeMap[HPS::SceneTree::ItemType::PolygonGroup] = HPS::Search::Type::Polygon;
	searchTypeMap[HPS::SceneTree::ItemType::CircleGroup] = HPS::Search::Type::Circle;
	searchTypeMap[HPS::SceneTree::ItemType::CircularWedgeGroup] = HPS::Search::Type::CircularWedge;
	searchTypeMap[HPS::SceneTree::ItemType::EllipseGroup] = HPS::Search::Type::Ellipse;
	searchTypeMap[HPS::SceneTree::ItemType::LineGroup] = HPS::Search::Type::Line;
	searchTypeMap[HPS::SceneTree::ItemType::NURBSCurveGroup] = HPS::Search::Type::NURBSCurve;
	searchTypeMap[HPS::SceneTree::ItemType::CircularArcGroup] = HPS::Search::Type::CircularArc;
	searchTypeMap[HPS::SceneTree::ItemType::EllipticalArcGroup] = HPS::Search::Type::EllipticalArc;
	searchTypeMap[HPS::SceneTree::ItemType::InfiniteLineGroup] = HPS::Search::Type::InfiniteLine;
	searchTypeMap[HPS::SceneTree::ItemType::InfiniteRayGroup] = HPS::Search::Type::InfiniteRay;
	searchTypeMap[HPS::SceneTree::ItemType::MarkerGroup] = HPS::Search::Type::Marker;
	searchTypeMap[HPS::SceneTree::ItemType::TextGroup] = HPS::Search::Type::Text;
	searchTypeMap[HPS::SceneTree::ItemType::ReferenceGroup] = HPS::Search::Type::Reference;
	searchTypeMap[HPS::SceneTree::ItemType::DistantLightGroup] = HPS::Search::Type::DistantLight;
	searchTypeMap[HPS::SceneTree::ItemType::SpotlightGroup] = HPS::Search::Type::Spotlight;
}

void CHPSTreeCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
	UINT uFlags = 0;
	HTREEITEM hti = HitTest(point, &uFlags);
	if (hti)
	{
		TVITEM tvi;
		tvi.mask = TVIF_HANDLE | TVIF_PARAM;
		tvi.hItem = hti;
		GetItem(&tvi);
		auto treeItem = reinterpret_cast<MFCSceneTreeItem *>(tvi.lParam);
		if (treeItem->GetKey().Type() == HPS::Type::None)
			this->DeleteItem(treeItem->GetTreeItem());

		if (uFlags & TVHT_ONITEMSTATEICON)
		{
			if (treeItem->IsHighlighted())
				treeItem->Unhighlight();
			else
				treeItem->Highlight();
		}
	}
	CTreeCtrl::OnLButtonDown(nFlags, point);
}

void CHPSTreeCtrl::OnRClick(NMHDR * /*pNMHDR*/, LRESULT * pResult)
{
	SendMessage(WM_CONTEXTMENU, (WPARAM)m_hWnd, GetMessagePos());
	*pResult = 1;
}

void CHPSTreeCtrl::OnContextMenu(CWnd * /*pWnd*/, CPoint ptMousePos)
{
	selectedSceneTreeItem = nullptr;

	ScreenToClient(&ptMousePos);
	UINT flags;
	HTREEITEM selectedItem = HitTest(ptMousePos, &flags);
	if (selectedItem != NULL && (flags & TVHT_ONITEM) != 0)
	{
		enum MenuType
		{
			None = -1,
			SegmentMenu,
			AttributeMenu,
		};
		MenuType menuType = None;
		selectedSceneTreeItem = reinterpret_cast<MFCSceneTreeItem *>(GetItemData(selectedItem));
		HPS::SceneTree::ItemType itemType = selectedSceneTreeItem->GetItemType();
		if (itemType == HPS::SceneTree::ItemType::Segment)
			menuType = SegmentMenu;
		else if (selectedSceneTreeItem->HasItemType(HPS::SceneTree::ItemType::Attribute)
			&& itemType != HPS::SceneTree::ItemType::Portfolio
			&& !selectedSceneTreeItem->GetKey().HasType(HPS::Type::WindowKey))				// Window attributes cannot be unset
			menuType = AttributeMenu;

		if (menuType != None)
		{
			CMenu menu;
			menu.LoadMenuW(IDR_SB_CONTEXT_MENU);
			CMenu * popup = menu.GetSubMenu(menuType);
			ClientToScreen(&ptMousePos);
			popup->TrackPopupMenu(TPM_LEFTALIGN, ptMousePos.x, ptMousePos.y, this);
		}
	}
}

void CHPSTreeCtrl::OnAddMaterial()
{
	AfxGetApp()->m_pMainWnd->PostMessageW(
		WM_MFC_SANDBOX_ADD_PROPERTY,
		reinterpret_cast<WPARAM>(selectedSceneTreeItem),
		static_cast<LPARAM>(HPS::SceneTree::ItemType::Material));
	selectedSceneTreeItem = nullptr;
}

void CHPSTreeCtrl::OnAddCamera()
{
	AfxGetApp()->m_pMainWnd->PostMessageW(
		WM_MFC_SANDBOX_ADD_PROPERTY,
		reinterpret_cast<WPARAM>(selectedSceneTreeItem),
		static_cast<LPARAM>(HPS::SceneTree::ItemType::Camera));
	selectedSceneTreeItem = nullptr;
}

void CHPSTreeCtrl::OnAddModellingMatrix()
{
	AfxGetApp()->m_pMainWnd->PostMessageW(
		WM_MFC_SANDBOX_ADD_PROPERTY,
		reinterpret_cast<WPARAM>(selectedSceneTreeItem),
		static_cast<LPARAM>(HPS::SceneTree::ItemType::ModellingMatrix));
	selectedSceneTreeItem = nullptr;
}

void CHPSTreeCtrl::OnAddTextureMatrix()
{
	AfxGetApp()->m_pMainWnd->PostMessageW(
		WM_MFC_SANDBOX_ADD_PROPERTY,
		reinterpret_cast<WPARAM>(selectedSceneTreeItem),
		static_cast<LPARAM>(HPS::SceneTree::ItemType::TextureMatrix));
	selectedSceneTreeItem = nullptr;
}

void CHPSTreeCtrl::OnAddCulling()
{
	AfxGetApp()->m_pMainWnd->PostMessageW(
		WM_MFC_SANDBOX_ADD_PROPERTY,
		reinterpret_cast<WPARAM>(selectedSceneTreeItem),
		static_cast<LPARAM>(HPS::SceneTree::ItemType::Culling));
	selectedSceneTreeItem = nullptr;
}

void CHPSTreeCtrl::OnAddCurveAttribute()
{
	AfxGetApp()->m_pMainWnd->PostMessageW(
		WM_MFC_SANDBOX_ADD_PROPERTY,
		reinterpret_cast<WPARAM>(selectedSceneTreeItem),
		static_cast<LPARAM>(HPS::SceneTree::ItemType::CurveAttribute));
	selectedSceneTreeItem = nullptr;
}

void CHPSTreeCtrl::OnAddCylinderAttribute()
{
	AfxGetApp()->m_pMainWnd->PostMessageW(
		WM_MFC_SANDBOX_ADD_PROPERTY,
		reinterpret_cast<WPARAM>(selectedSceneTreeItem),
		static_cast<LPARAM>(HPS::SceneTree::ItemType::CylinderAttribute));
	selectedSceneTreeItem = nullptr;
}

void CHPSTreeCtrl::OnAddEdgeAttribute()
{
	AfxGetApp()->m_pMainWnd->PostMessageW(
		WM_MFC_SANDBOX_ADD_PROPERTY,
		reinterpret_cast<WPARAM>(selectedSceneTreeItem),
		static_cast<LPARAM>(HPS::SceneTree::ItemType::EdgeAttribute));
	selectedSceneTreeItem = nullptr;
}

void CHPSTreeCtrl::OnAddLightingAttribute()
{
	AfxGetApp()->m_pMainWnd->PostMessageW(
		WM_MFC_SANDBOX_ADD_PROPERTY,
		reinterpret_cast<WPARAM>(selectedSceneTreeItem),
		static_cast<LPARAM>(HPS::SceneTree::ItemType::LightingAttribute));
	selectedSceneTreeItem = nullptr;
}

void CHPSTreeCtrl::OnAddLineAttribute()
{
	AfxGetApp()->m_pMainWnd->PostMessageW(
		WM_MFC_SANDBOX_ADD_PROPERTY,
		reinterpret_cast<WPARAM>(selectedSceneTreeItem),
		static_cast<LPARAM>(HPS::SceneTree::ItemType::LineAttribute));
	selectedSceneTreeItem = nullptr;
}

void CHPSTreeCtrl::OnAddMarkerAttribute()
{
	AfxGetApp()->m_pMainWnd->PostMessageW(
		WM_MFC_SANDBOX_ADD_PROPERTY,
		reinterpret_cast<WPARAM>(selectedSceneTreeItem),
		static_cast<LPARAM>(HPS::SceneTree::ItemType::MarkerAttribute));
	selectedSceneTreeItem = nullptr;
}

void CHPSTreeCtrl::OnAddSurfaceAttribute()
{
	AfxGetApp()->m_pMainWnd->PostMessageW(
		WM_MFC_SANDBOX_ADD_PROPERTY,
		reinterpret_cast<WPARAM>(selectedSceneTreeItem),
		static_cast<LPARAM>(HPS::SceneTree::ItemType::SurfaceAttribute));
	selectedSceneTreeItem = nullptr;
}

void CHPSTreeCtrl::OnAddSelectability()
{
	AfxGetApp()->m_pMainWnd->PostMessageW(
		WM_MFC_SANDBOX_ADD_PROPERTY,
		reinterpret_cast<WPARAM>(selectedSceneTreeItem),
		static_cast<LPARAM>(HPS::SceneTree::ItemType::Selectability));
	selectedSceneTreeItem = nullptr;
}

void CHPSTreeCtrl::OnAddSphereAttribute()
{
	AfxGetApp()->m_pMainWnd->PostMessageW(
		WM_MFC_SANDBOX_ADD_PROPERTY,
		reinterpret_cast<WPARAM>(selectedSceneTreeItem),
		static_cast<LPARAM>(HPS::SceneTree::ItemType::SphereAttribute));
	selectedSceneTreeItem = nullptr;
}

void CHPSTreeCtrl::OnAddSubwindow()
{
	AfxGetApp()->m_pMainWnd->PostMessageW(
		WM_MFC_SANDBOX_ADD_PROPERTY,
		reinterpret_cast<WPARAM>(selectedSceneTreeItem),
		static_cast<LPARAM>(HPS::SceneTree::ItemType::Subwindow));
	selectedSceneTreeItem = nullptr;
}

void CHPSTreeCtrl::OnAddTextAttribute()
{
	AfxGetApp()->m_pMainWnd->PostMessageW(
		WM_MFC_SANDBOX_ADD_PROPERTY,
		reinterpret_cast<WPARAM>(selectedSceneTreeItem),
		static_cast<LPARAM>(HPS::SceneTree::ItemType::TextAttribute));
	selectedSceneTreeItem = nullptr;
}

void CHPSTreeCtrl::OnAddTransparency()
{
	AfxGetApp()->m_pMainWnd->PostMessageW(
		WM_MFC_SANDBOX_ADD_PROPERTY,
		reinterpret_cast<WPARAM>(selectedSceneTreeItem),
		static_cast<LPARAM>(HPS::SceneTree::ItemType::Transparency));
	selectedSceneTreeItem = nullptr;
}

void CHPSTreeCtrl::OnAddVisibility()
{
	AfxGetApp()->m_pMainWnd->PostMessageW(
		WM_MFC_SANDBOX_ADD_PROPERTY,
		reinterpret_cast<WPARAM>(selectedSceneTreeItem),
		static_cast<LPARAM>(HPS::SceneTree::ItemType::Visibility));
	selectedSceneTreeItem = nullptr;
}

void CHPSTreeCtrl::OnAddVisualEffects()
{
	AfxGetApp()->m_pMainWnd->PostMessageW(
		WM_MFC_SANDBOX_ADD_PROPERTY,
		reinterpret_cast<WPARAM>(selectedSceneTreeItem),
		static_cast<LPARAM>(HPS::SceneTree::ItemType::VisualEffects));
	selectedSceneTreeItem = nullptr;
}

void CHPSTreeCtrl::OnAddPerformance()
{
	AfxGetApp()->m_pMainWnd->PostMessageW(
		WM_MFC_SANDBOX_ADD_PROPERTY,
		reinterpret_cast<WPARAM>(selectedSceneTreeItem),
		static_cast<LPARAM>(HPS::SceneTree::ItemType::Performance));
	selectedSceneTreeItem = nullptr;
}

void CHPSTreeCtrl::OnAddDrawingAttribute()
{
	AfxGetApp()->m_pMainWnd->PostMessageW(
		WM_MFC_SANDBOX_ADD_PROPERTY,
		reinterpret_cast<WPARAM>(selectedSceneTreeItem),
		static_cast<LPARAM>(HPS::SceneTree::ItemType::DrawingAttribute));
	selectedSceneTreeItem = nullptr;
}

void CHPSTreeCtrl::OnAddHiddenLineAttribute()
{
	AfxGetApp()->m_pMainWnd->PostMessageW(
		WM_MFC_SANDBOX_ADD_PROPERTY,
		reinterpret_cast<WPARAM>(selectedSceneTreeItem),
		static_cast<LPARAM>(HPS::SceneTree::ItemType::LineAttribute));
	selectedSceneTreeItem = nullptr;
}

void CHPSTreeCtrl::OnAddMaterialPalette()
{
	AfxGetApp()->m_pMainWnd->PostMessageW(
		WM_MFC_SANDBOX_ADD_PROPERTY,
		reinterpret_cast<WPARAM>(selectedSceneTreeItem),
		static_cast<LPARAM>(HPS::SceneTree::ItemType::MaterialPalette));
	selectedSceneTreeItem = nullptr;
}

void CHPSTreeCtrl::OnAddContourLine()
{
	AfxGetApp()->m_pMainWnd->PostMessageW(
		WM_MFC_SANDBOX_ADD_PROPERTY,
		reinterpret_cast<WPARAM>(selectedSceneTreeItem),
		static_cast<LPARAM>(HPS::SceneTree::ItemType::ContourLine));
	selectedSceneTreeItem = nullptr;
}

void CHPSTreeCtrl::OnAddCondition()
{
	AfxGetApp()->m_pMainWnd->PostMessageW(
		WM_MFC_SANDBOX_ADD_PROPERTY,
		reinterpret_cast<WPARAM>(selectedSceneTreeItem),
		static_cast<LPARAM>(HPS::SceneTree::ItemType::Condition));
	selectedSceneTreeItem = nullptr;
}

void CHPSTreeCtrl::OnAddBounding()
{
	AfxGetApp()->m_pMainWnd->PostMessageW(
		WM_MFC_SANDBOX_ADD_PROPERTY,
		reinterpret_cast<WPARAM>(selectedSceneTreeItem),
		static_cast<LPARAM>(HPS::SceneTree::ItemType::Bounding));
	selectedSceneTreeItem = nullptr;
}

void CHPSTreeCtrl::OnAddAttributeLock()
{
	AfxGetApp()->m_pMainWnd->PostMessageW(
		WM_MFC_SANDBOX_ADD_PROPERTY,
		reinterpret_cast<WPARAM>(selectedSceneTreeItem),
		static_cast<LPARAM>(HPS::SceneTree::ItemType::AttributeLock));
	selectedSceneTreeItem = nullptr;
}

void CHPSTreeCtrl::OnAddTransformMask()
{
	AfxGetApp()->m_pMainWnd->PostMessageW(
		WM_MFC_SANDBOX_ADD_PROPERTY,
		reinterpret_cast<WPARAM>(selectedSceneTreeItem),
		static_cast<LPARAM>(HPS::SceneTree::ItemType::TransformMask));
	selectedSceneTreeItem = nullptr;
}

void CHPSTreeCtrl::OnAddColorInterpolation()
{
	AfxGetApp()->m_pMainWnd->PostMessageW(
		WM_MFC_SANDBOX_ADD_PROPERTY,
		reinterpret_cast<WPARAM>(selectedSceneTreeItem),
		static_cast<LPARAM>(HPS::SceneTree::ItemType::ColorInterpolation));
	selectedSceneTreeItem = nullptr;
}

void CHPSTreeCtrl::OnAddCuttingSectionAttribute()
{
	AfxGetApp()->m_pMainWnd->PostMessageW(
		WM_MFC_SANDBOX_ADD_PROPERTY,
		reinterpret_cast<WPARAM>(selectedSceneTreeItem),
		static_cast<LPARAM>(HPS::SceneTree::ItemType::CuttingSectionAttribute));
	selectedSceneTreeItem = nullptr;
}

void CHPSTreeCtrl::OnAddPriority()
{
	AfxGetApp()->m_pMainWnd->PostMessageW(
		WM_MFC_SANDBOX_ADD_PROPERTY,
		reinterpret_cast<WPARAM>(selectedSceneTreeItem),
		static_cast<LPARAM>(HPS::SceneTree::ItemType::Priority));
	selectedSceneTreeItem = nullptr;
}

void CHPSTreeCtrl::OnUnsetAttribute()
{
	AfxGetApp()->m_pMainWnd->PostMessageW(
		WM_MFC_SANDBOX_UNSET_ATTRIBUTE,
		reinterpret_cast<WPARAM>(selectedSceneTreeItem),
		reinterpret_cast<LPARAM>(nullptr));
	selectedSceneTreeItem = nullptr;
}


void CHPSTreeCtrl::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (nChar == VK_DELETE)
	{
		HTREEITEM selectedItem = GetSelectedItem();
		if (selectedItem != NULL)
		{
			selectedSceneTreeItem = reinterpret_cast<MFCSceneTreeItem *>(GetItemData(selectedItem));
			HPS::SceneTree::ItemType itemType = selectedSceneTreeItem->GetItemType();

			auto canBeDeleted = [this](HTREEITEM item)
			{
				auto tree_item = reinterpret_cast<MFCSceneTreeItem *>(GetItemData(item));
				if (tree_item->GetItemType() == HPS::SceneTree::ItemType::Segment ||
					tree_item->HasItemType(HPS::SceneTree::ItemType::Geometry) ||
					tree_item->HasItemType(HPS::SceneTree::ItemType::GeometryGroupMask))
					return true;

				return false;
			};

			auto getRelatives = [&](HTREEITEM in_selected_item, HTREEITEM & out_parent, HTREEITEM & out_next_sibling, HTREEITEM & out_prev_sibling)
			{
				out_parent = GetParentItem(in_selected_item);
				while (out_parent != NULL && !canBeDeleted(out_parent))
					out_parent = GetParentItem(out_parent);

				out_next_sibling = GetNextSiblingItem(in_selected_item);
				while (out_next_sibling != NULL && !canBeDeleted(out_next_sibling))
					out_next_sibling = GetNextSiblingItem(out_next_sibling);

				out_prev_sibling = GetPrevSiblingItem(in_selected_item);
				while (out_prev_sibling != NULL && !canBeDeleted(out_prev_sibling))
					out_prev_sibling = GetPrevSiblingItem(out_prev_sibling);
			};

			HTREEITEM parent, sibling, prev_sibling;

			auto deleteSelectedItem = [&]()
			{
				if (sibling != NULL)
					SelectItem(sibling);
				else if (prev_sibling != NULL)
					SelectItem(prev_sibling);
				else
					SelectItem(parent);

				DeleteItem(selectedItem);
				selectedSceneTreeItem = nullptr;

				CHPSFrame * main_frame = (CHPSFrame *)(theApp.m_pMainWnd);
				CHPSView * active_view = (CHPSView *)(main_frame->GetActiveView());
				active_view->GetCanvas().Update();
			};

			if (itemType == HPS::SceneTree::ItemType::Segment)
			{
				getRelatives(selectedItem, parent, sibling, prev_sibling);
				if (parent == NULL)
					return;

				selectedSceneTreeItem->GetKey().Delete();
				deleteSelectedItem();
			}
			else if (selectedSceneTreeItem->HasItemType(HPS::SceneTree::ItemType::Geometry))
			{
				getRelatives(selectedItem, parent, sibling, prev_sibling);
				selectedSceneTreeItem->GetKey().Delete();
				deleteSelectedItem();
			}
			else if (selectedSceneTreeItem->HasItemType(HPS::SceneTree::ItemType::GeometryGroupMask))
			{
				getRelatives(selectedItem, parent, sibling, prev_sibling);

				HPS::SegmentKey groupKey(selectedSceneTreeItem->GetKey());
				HPS::SearchResults results;
				groupKey.Find(searchTypeMap[selectedSceneTreeItem->GetItemType()], HPS::Search::Space::SegmentOnly, results);
				HPS::SearchResultsIterator it = results.GetIterator();
				while (it.IsValid())
				{
					it.GetItem().Delete();
					it.Next();
				}

				deleteSelectedItem();
			}
		}
	}

	CTreeCtrl::OnKeyDown(nChar, nRepCnt, nFlags);
}
