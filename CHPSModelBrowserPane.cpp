#include "stdafx.h"
#include "resource.h"
#include "CHPSApp.h"
#include "CHPSDoc.h"
#include "CHPSView.h"
#include "CHPSFrame.h"
#include "IFCUtils.h"

#include "CHPSModelBrowserPane.h"



// this logic for a context menu for a CTreeCtrl came from here: http://support.microsoft.com/kb/222905

BEGIN_MESSAGE_MAP(ModelTreeCtrl, CTreeCtrl)
	ON_NOTIFY_REFLECT(NM_RCLICK, OnRClick)
	ON_WM_CONTEXTMENU()
	ON_COMMAND(ID_MB_CONTEXT_SHOW_HIDE, OnShowHide)
	ON_COMMAND(ID_MB_CONTEXT_ISOLATE, OnIsolate)
	ON_COMMAND(ID_MB_CONTEXT_RESET_VISIBILITY, OnResetVisibility)
	ON_COMMAND(ID_DWGLAYER_TOGGLE, OnDWGLayerToggle)
	ON_COMMAND(ID_MB_CONTEXT_HIGHLIGHT, &ModelTreeCtrl::OnMbContextHighlight)
	ON_COMMAND(ID_MB_CONTEXT_WALKTO, &ModelTreeCtrl::OnMbContextWalkto)
	ON_COMMAND(ID_MB_CONTEXT_ADDTOBOM, &ModelTreeCtrl::OnMbContextAddtoBOM)
	ON_COMMAND(ID_MB_CONTEXT_ADDMODEL, &ModelTreeCtrl::OnMbContextAddmodel)
END_MESSAGE_MAP()

void ModelTreeCtrl::OnRClick(NMHDR * /*pNMHDR*/, LRESULT * pResult)
{
	SendMessage(WM_CONTEXTMENU, (WPARAM)m_hWnd, GetMessagePos());
	*pResult = 1;
}

void ModelTreeCtrl::OnContextMenu(CWnd * /*pWnd*/, CPoint ptMousePos)
{
	ScreenToClient(&ptMousePos);
	UINT flags;
	HTREEITEM item = HitTest(ptMousePos, &flags);
	if (item != NULL && (flags & TVHT_ONITEM) != 0)
	{
		CMenu menu;
		menu.LoadMenuW(IDR_MB_CONTEXT_MENU);
		CMenu * pPopup = menu.GetSubMenu(0);

		CString showHideString;
		contextItem = (MFCComponentTreeItem *)GetItemData(item);
		if (contextItem != nullptr)
		{
			HPS::Component selected_component = contextItem->GetComponent();
			HPS::Component::ComponentType component_type = selected_component.GetComponentType();
			if (selected_component.HasComponentType(HPS::Component::ComponentType::ExchangeComponentMask))
			{
				if (contextItem->IsHidden())
					showHideString = "Show";
				else
					showHideString = "Hide";
				pPopup->ModifyMenuW(ID_MB_CONTEXT_SHOW_HIDE, MF_BYCOMMAND | MF_STRING, ID_MB_CONTEXT_SHOW_HIDE, showHideString);
			}
			else if (component_type == HPS::Component::ComponentType::DWGLayer)
			{
#ifdef USING_DWG
				pPopup = menu.GetSubMenu(1);
				HPS::DWG::Layer dwg_layer(selected_component);
				if (dwg_layer.IsOn())
					showHideString = "Turn OFF";
				else
					showHideString = "Turn ON";
				pPopup->ModifyMenuW(ID_DWGLAYER_TOGGLE, MF_BYCOMMAND | MF_STRING, ID_DWGLAYER_TOGGLE, showHideString);
#endif
			}
			else if (component_type == HPS::Component::ComponentType::DWGModelFile ||
					component_type == HPS::Component::ComponentType::DWGBlockTable ||
					component_type == HPS::Component::ComponentType::DWGLayerTable ||
					component_type == HPS::Component::ComponentType::DWGLayout)
					return;

			ClientToScreen(&ptMousePos);
			pPopup->TrackPopupMenu(TPM_LEFTALIGN, ptMousePos.x, ptMousePos.y, this);
		}
	}
}

void ModelTreeCtrl::OnShowHide()
{
	CHPSView * cview = GetCHPSDoc()->GetCHPSView();
	cview->Unhighlight();

	if (contextItem->IsHidden())
	{
		contextItem->Show();

		HPS::CADModel cadModel = GetCHPSDoc()->GetCADModel();
		if (HPS::ComponentPath(1, &cadModel).IsHidden(cview->GetCanvas()))
			cview->InvalidateZoomKeyPath();		// no longer zoom to key path if we're showing other elements when isolated
		else
			cview->InvalidateSavedCamera();		// no longer in isolate mode
	}
	else
		contextItem->Hide();

	cview->Update();
}

void ModelTreeCtrl::OnDWGLayerToggle()
{
#ifdef USING_DWG
	CHPSView * cview = GetCHPSDoc()->GetCHPSView();
	cview->Unhighlight();

	HPS::DWG::Layer dwg_layer(contextItem->GetComponent());

	if (dwg_layer.IsOn())
		dwg_layer.TurnOff();
	else
		dwg_layer.TurnOn();

	cview->Update();
#endif
}

void ModelTreeCtrl::OnIsolate()
{
	CHPSView * cview = GetCHPSDoc()->GetCHPSView();
	cview->Unhighlight();

	contextItem->Isolate();
	cview->ZoomToKeyPath(contextItem->GetPath().GetKeyPaths()[0]);

	cview->Update();
}

void ModelTreeCtrl::OnResetVisibility()
{
	CHPSView * cview = GetCHPSDoc()->GetCHPSView();
	HPS::Canvas canvas = cview->GetCanvas();
	GetCHPSDoc()->GetCADModel().ResetVisibility(canvas);
	cview->RestoreCamera();
	cview->Update();
}




CTreeCtrl * MFCComponentTree::GetTreeCtrl()
{
	return browser->GetModelTreeCtrl();
}



void MFCComponentTreeItem::OnHighlight(HPS::HighlightOptionsKit const & in_options)
{
	HPS::ComponentTreeItem::OnHighlight(in_options);

	HPS::ComponentTreePtr tree = GetTree();
	if (tree != nullptr)
	{
		CTreeCtrl * treeCtrl = GetTreeCtrl();
		treeCtrl->SetItemState(treeItem, TVIS_BOLD, TVIS_BOLD);
		GetModelBrowserPane()->UpdateWindow();
	}
}

void MFCComponentTreeItem::OnUnhighlight(HPS::HighlightOptionsKit const & in_options)
{
	HPS::ComponentTreeItem::OnUnhighlight(in_options);

	HPS::ComponentTreePtr tree = GetTree();
	if (tree != nullptr)
	{
		CTreeCtrl * treeCtrl = GetTreeCtrl();
		treeCtrl->SetItemState(treeItem, 0, TVIS_BOLD);
		GetModelBrowserPane()->UpdateWindow();
	}
}

void MFCComponentTreeItem::OnHide()
{
	HPS::ComponentTreeItem::OnHide();

	HPS::ComponentTreePtr tree = GetTree();
	if (tree != nullptr)
	{
		CTreeCtrl * treeCtrl = GetTreeCtrl();
		int imageIndex = GetHiddenImageIndex(*this);
		int selectedImageIndex = imageIndex;
		treeCtrl->SetItemImage(treeItem, imageIndex, selectedImageIndex);
		GetModelBrowserPane()->UpdateWindow();
	}
}

void MFCComponentTreeItem::OnShow()
{
	HPS::ComponentTreeItem::OnShow();

	HPS::ComponentTreePtr tree = GetTree();
	if (tree != nullptr)
	{
		CTreeCtrl * treeCtrl = GetTreeCtrl();
		int imageIndex = GetImageIndex(*this);
		int selectedImageIndex = imageIndex;
		treeCtrl->SetItemImage(treeItem, imageIndex, selectedImageIndex);
		GetModelBrowserPane()->UpdateWindow();
	}
}

int MFCComponentTreeItem::GetImageIndex(HPS::ComponentTreeItem const & item)
{
	int index = 0;

	HPS::ComponentTreePtr tree = GetTree();
	if (tree != nullptr)
	{
		CHPSModelBrowserPane::Image imageType = CHPSModelBrowserPane::Unknown;

		switch (item.GetItemType())
		{
			case HPS::ComponentTree::ItemType::ExchangeViewGroup:
			case HPS::ComponentTree::ItemType::ExchangeAnnotationViewGroup:
			{
				imageType = CHPSModelBrowserPane::ViewGroup;
			}	break;

			case HPS::ComponentTree::ItemType::ExchangePMIGroup:
			{
				imageType = CHPSModelBrowserPane::PMIGroup;
			}	break;

			case HPS::ComponentTree::ItemType::ExchangeModelGroup:
			{
				imageType = CHPSModelBrowserPane::RIGeometrySet;
			}	break;

			case HPS::ComponentTree::ItemType::DWGModelFile:
			case HPS::ComponentTree::ItemType::ExchangeModelFile:
			{
				imageType = CHPSModelBrowserPane::ModelFile;
			}	break;

			case HPS::ComponentTree::ItemType::ExchangeComponent:
			{
				switch (item.GetComponent().GetComponentType())
				{
					case HPS::Component::ComponentType::ExchangeProductOccurrence:
					{
						HPS::ComponentArray subcomponents = item.GetComponent().GetSubcomponents();
						auto it = std::find_if(subcomponents.begin(), subcomponents.end(),
							[](HPS::Component const & comp) { return comp.GetComponentType() == HPS::Component::ComponentType::ExchangeProductOccurrence; });
						if (it == subcomponents.end())
							imageType = CHPSModelBrowserPane::ProductStructureWithoutChildren;
						else
							imageType = CHPSModelBrowserPane::ProductStructureWithChildren;
					}	break;

					case HPS::Component::ComponentType::ExchangeRISet:
					{
						imageType = CHPSModelBrowserPane::RIGeometrySet;
					}	break;

					case HPS::Component::ComponentType::ExchangeRIPlane:
					{
						imageType = CHPSModelBrowserPane::RIPlane;
					}	break;

					case HPS::Component::ComponentType::ExchangeRIDirection:
					{
						imageType = CHPSModelBrowserPane::RIDirection;
					}	break;

					case HPS::Component::ComponentType::ExchangeRICoordinateSystem:
					{
						imageType = CHPSModelBrowserPane::RICoordinateSystem;
					}	break;

					case HPS::Component::ComponentType::ExchangeRIBRepModel:
					case HPS::Component::ComponentType::ExchangeRIPolyBRepModel:
					{
						imageType = CHPSModelBrowserPane::RISolid;
					}	break;

					case HPS::Component::ComponentType::ExchangeRICurve:
					case HPS::Component::ComponentType::ExchangeRIPolyWire:
					{
						imageType = CHPSModelBrowserPane::RICurve;
					}	break;

					case HPS::Component::ComponentType::ExchangeRIPointSet:
					{
						imageType = CHPSModelBrowserPane::RIPointCloud;
					}	break;

					case HPS::Component::ComponentType::ExchangeView:
					{
						if (HPS::BooleanMetadata(item.GetComponent().GetMetadata("IsAnnotationCapture")).GetValue() == true)
							imageType = CHPSModelBrowserPane::AnnotationView;
						else
							imageType = CHPSModelBrowserPane::View;
					}	break;

					case HPS::Component::ComponentType::ExchangePMI:
					case HPS::Component::ComponentType::ExchangePMIText:
					case HPS::Component::ComponentType::ExchangePMIRichText:
					{
						imageType = CHPSModelBrowserPane::PMIText;
					}	break;

					case HPS::Component::ComponentType::ExchangePMIGDT:
					{
						imageType = CHPSModelBrowserPane::PMITolerance;
					}	break;

					case HPS::Component::ComponentType::ExchangePMIRoughness:
					{
						imageType = CHPSModelBrowserPane::PMIRoughness;
					}	break;

					case HPS::Component::ComponentType::ExchangePMILineWelding:
					{
						imageType = CHPSModelBrowserPane::PMILineWelding;
					}	break;

					case HPS::Component::ComponentType::ExchangePMISpotWelding:
					{
						imageType = CHPSModelBrowserPane::PMISpotWelding;
					}	break;

					case HPS::Component::ComponentType::ExchangePMIDatum:
					{
						imageType = CHPSModelBrowserPane::PMIDatum;
					}	break;

					case HPS::Component::ComponentType::ExchangePMIDimension:
					{
						imageType = CHPSModelBrowserPane::PMIDimensionDistance;
					}	break;

					case HPS::Component::ComponentType::ExchangePMIBalloon:
					{
						imageType = CHPSModelBrowserPane::PMIBalloon;
					}	break;

					case HPS::Component::ComponentType::ExchangePMICoordinate:
					{
						imageType = CHPSModelBrowserPane::PMICoordinate;
					}	break;

					case HPS::Component::ComponentType::ExchangePMIFastener:
					{
						imageType = CHPSModelBrowserPane::PMIFastener;
					}	break;

					case HPS::Component::ComponentType::ExchangePMILocator:
					{
						imageType = CHPSModelBrowserPane::PMILocator;
					}	break;

					case HPS::Component::ComponentType::ExchangePMIMeasurementPoint:
					{
						imageType = CHPSModelBrowserPane::PMIMeasurementPoint;
					}	break;

					case HPS::Component::ComponentType::ExchangeDrawingModel:
					{
						imageType = CHPSModelBrowserPane::DrawingModel;
					}	break;

					case HPS::Component::ComponentType::ExchangeDrawingSheet:
					{
						imageType = CHPSModelBrowserPane::DrawingSheet;
					}	break;

					case HPS::Component::ComponentType::ExchangeDrawingView:
					{
						imageType = CHPSModelBrowserPane::DrawingView;
					}	break;

					default:
					{
						imageType = CHPSModelBrowserPane::Unknown;
					}	break;
				}
			}	break;

			case HPS::ComponentTree::ItemType::ParasolidModelFile:
			{
				imageType = CHPSModelBrowserPane::ParasolidModel;
			}	break;

			case HPS::ComponentTree::ItemType::ParasolidComponent:
			{
				switch (item.GetComponent().GetComponentType())
				{
					case HPS::Component::ComponentType::ParasolidAssembly:
					{
						imageType = CHPSModelBrowserPane::ParasolidAssembly;
					}	break;

					case HPS::Component::ComponentType::ParasolidTopoBody:
					{
						imageType = CHPSModelBrowserPane::ParasolidBody;
					}	break;

					case HPS::Component::ComponentType::ParasolidInstance:
					{
						HPS::Component::ComponentType component_type = item.GetComponent().GetSubcomponents()[0].GetComponentType();
						if (component_type == HPS::Component::ComponentType::ParasolidAssembly)
							imageType = CHPSModelBrowserPane::ParasolidAssembly;
						else if (component_type == HPS::Component::ComponentType::ParasolidTopoBody)
							imageType = CHPSModelBrowserPane::ParasolidBody;
					} break;

					default:
					{
						imageType = CHPSModelBrowserPane::Unknown;
					}	break;
				}
			}	break;

			case HPS::ComponentTree::ItemType::DWGComponent:
			{
				HPS::Component::ComponentType component_type = item.GetComponent().GetComponentType();
				switch (component_type)
				{
				case HPS::Component::ComponentType::DWGBlockTable:
				case HPS::Component::ComponentType::DWGLayerTable:
				case HPS::Component::ComponentType::DWGBlockTableRecord:
					{
						imageType = CHPSModelBrowserPane::RIGeometrySet;
						break;
					}
				case HPS::Component::ComponentType::DWGLayer:
					{
						imageType = CHPSModelBrowserPane::DrawingSheet;
						break;
					}
				case HPS::Component::ComponentType::DWGEntity:
					{
						imageType = CHPSModelBrowserPane::RISolid;
						break;
					}
				case HPS::Component::ComponentType::DWGLayout:
					{
						imageType = CHPSModelBrowserPane::AnnotationView;
						break;
					}
				}

			} break;

			default:
			{
				imageType = CHPSModelBrowserPane::Unknown;
			}	break;
		}

		index = std::static_pointer_cast<MFCComponentTree>(tree)->GetModelBrowserPane()->GetImageBaseIndex(imageType);
	}
	return index;
}

int MFCComponentTreeItem::GetHiddenImageIndex(HPS::ComponentTreeItem const & item)
{
	int index = GetImageIndex(item);

	if (index != -1)
		index += 2;

	return index;
}




CHPSModelBrowserPane::CHPSModelBrowserPane() : currentTreeCtrl(nullptr) {}

CHPSModelBrowserPane::~CHPSModelBrowserPane() {}

BEGIN_MESSAGE_MAP(CHPSModelBrowserPane, CDockablePane)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_PAINT()
	ON_NOTIFY(NM_CLICK, IDC_MODEL_BROWSER, OnClick)
	ON_NOTIFY(TVN_ITEMEXPANDING, IDC_MODEL_BROWSER, OnItemExpanding)
	ON_NOTIFY(TVN_SELCHANGED, IDC_MODEL_BROWSER, OnSelectionChanged)
END_MESSAGE_MAP()


int CHPSModelBrowserPane::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CDockablePane::OnCreate(lpCreateStruct) == -1)
		return -1;

	CRect rectDummy;
	rectDummy.SetRectEmpty();

	const DWORD dwStyle = WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS;

	if (!emptyTree.Create(dwStyle, rectDummy, this, IDC_MODEL_BROWSER))
	{
		TRACE0("Failed to create model browser\n");
		return -1;      // fail to create
	}

	if (GetSafeHwnd() == NULL)
	{
		return -1;
	}

	TVINSERTSTRUCT tvi;
	tvi.hParent = NULL;
	tvi.hInsertAfter = NULL;
	tvi.item.mask = TVIF_TEXT;
	tvi.item.pszText = _T("No Model");

	emptyTree.InsertItem(&tvi);

	CRect rectClient;
	GetClientRect(rectClient);

	emptyTree.SetWindowPos(NULL, rectClient.left + 1, rectClient.top + 1, rectClient.Width() - 2, rectClient.Height() - 2, SWP_NOACTIVATE | SWP_NOZORDER);

	imageList.Create(16, 16, ILC_COLOR32, 0, 1);

	for (int i = IDB_MODEL_BROWSER_BEGIN; i <= IDB_MODEL_BROWSER_END; ++i)
	{
		imageBaseIndices.push_back(imageList.GetImageCount());
		CBitmap bitmap;
		bitmap.LoadBitmapW(i);
		imageList.Add(&bitmap, RGB(0, 255, 0));
	}

	if (!modelTree.Create(dwStyle, rectDummy, this, IDC_MODEL_BROWSER))
	{
		TRACE0("Failed to create model browser\n");
		ASSERT(0);
	}
	modelTree.SetImageList(&imageList, TVSIL_NORMAL);

	modelTree.SetWindowPos(NULL, rectClient.left + 1, rectClient.top + 1, rectClient.Width() - 2, rectClient.Height() - 2, SWP_NOACTIVATE | SWP_NOZORDER);

	currentTreeCtrl = &emptyTree;

	return 0;
}

void CHPSModelBrowserPane::OnSize(UINT nType, int cx, int cy)
{
	CDockablePane::OnSize(nType, cx, cy);

	CRect rectClient;
	GetClientRect(rectClient);

	emptyTree.SetWindowPos(NULL, rectClient.left + 1, rectClient.top + 1, rectClient.Width() - 2, rectClient.Height() - 2, SWP_NOACTIVATE | SWP_NOZORDER);
	modelTree.SetWindowPos(NULL, rectClient.left + 1, rectClient.top + 1, rectClient.Width() - 2, rectClient.Height() - 2, SWP_NOACTIVATE | SWP_NOZORDER);
}

void CHPSModelBrowserPane::OnPaint()
{
	CPaintDC dc(this); // device context for painting

	CRect rectTree;
	currentTreeCtrl->GetWindowRect(rectTree);
	ScreenToClient(rectTree);

	rectTree.InflateRect(1, 1);
	dc.Draw3dRect(rectTree, ::GetSysColor(COLOR_3DSHADOW), ::GetSysColor(COLOR_3DSHADOW));
}

void CHPSModelBrowserPane::Init()
{
	CHPSDoc * doc = GetCHPSDoc();
	HPS::CADModel cadModel = doc->GetCADModel();

	if (cadModel.Empty())
		Flush();
	else
	{
		if (componentTree)
		{
			componentTree->Flush();
			componentTree.reset();
		}
		emptyTree.ShowWindow(SW_HIDE);
		modelTree.ShowWindow(SW_SHOWNORMAL);
		currentTreeCtrl = &modelTree;

		componentTree = std::make_shared<MFCComponentTree>(doc->GetCHPSView()->GetCanvas(), this);
		auto root = std::make_shared<MFCComponentTreeItem>(componentTree, cadModel);
		componentTree->SetRoot(root);

		HPS::HighlightOptionsKit highlight_options;
		highlight_options.SetStyleName("highlight_style").SetNotification(true);
		componentTree->SetHighlightOptions(highlight_options);

		// auto expand out a few levels
		HTREEITEM rootTreeItem = modelTree.GetRootItem();
		reinterpret_cast<MFCComponentTreeItem *>(modelTree.GetItemData(rootTreeItem))->Expand();
		HTREEITEM modelGroupItem = modelTree.GetNextItem(rootTreeItem, TVGN_CHILD);
		reinterpret_cast<MFCComponentTreeItem *>(modelTree.GetItemData(modelGroupItem))->Expand();
	}
}

void CHPSModelBrowserPane::Flush()
{
	modelTree.ShowWindow(SW_HIDE);
	emptyTree.ShowWindow(SW_SHOWNORMAL);
	currentTreeCtrl = &emptyTree;
}

void CHPSModelBrowserPane::OnItemExpanding(NMHDR * pNMHDR, LRESULT * pResult)
{
	NM_TREEVIEW * pNMTreeView = (NM_TREEVIEW *)pNMHDR;
	TVITEM treeItem = pNMTreeView->itemNew;
	MFCComponentTreeItem * componentItem = (MFCComponentTreeItem *)treeItem.lParam;
	if (pNMTreeView->action == TVE_EXPAND)
		componentItem->Expand();
	else
		componentItem->Collapse();
	*pResult = 0;
}

void CHPSModelBrowserPane::OnSelection(MFCComponentTreeItem * componentItem)
{
	if (componentItem == nullptr)
		return;

	HPS::ComponentTree::ItemType itemType = componentItem->GetItemType();
	HPS::Component::ComponentType componentType = componentItem->GetComponent().GetComponentType();

	if ((itemType == HPS::ComponentTree::ItemType::ExchangeComponent && componentType != HPS::Component::ComponentType::ExchangeFilter)
		|| itemType == HPS::ComponentTree::ItemType::ParasolidComponent
		|| itemType == HPS::ComponentTree::ItemType::DWGComponent)
	{
#	ifdef USING_EXCHANGE
		if (componentType == HPS::Component::ComponentType::ExchangeView)
		{
			auto capture = HPS::Capture(componentItem->GetComponent());
			GetCHPSDoc()->GetCHPSView()->ActivateCapture(capture);
		}
		else if (componentType == HPS::Component::ComponentType::ExchangeDrawingSheet)
		{
			HPS::View newView = HPS::Exchange::Sheet(componentItem->GetComponent()).Activate();
			GetCHPSDoc()->GetCHPSView()->AttachView(newView);
		}
		else
#elif defined USING_DWG
		if (componentType == HPS::Component::ComponentType::DWGLayout)
		{
			auto layout = HPS::DWG::Layout(componentItem->GetComponent());
			GetCHPSDoc()->GetCHPSView()->ActivateCapture(layout);
			return;
		}
#	endif
		if (componentItem->IsHidden() == false)
		{
#		if defined(USING_EXCHANGE) || defined(USING_PARASOLID) ||defined(USING_DWG)

			if (componentType != HPS::Component::ComponentType::DWGBlockTable &&
				componentType != HPS::Component::ComponentType::DWGLayerTable &&
				componentType != HPS::Component::ComponentType::DWGLayer)
			{
				CHPSView * cview = GetCHPSDoc()->GetCHPSView();
				cview->Unhighlight();

				componentItem->Highlight();

				cview->Update();
			}
#		endif
		}
	}
}

void CHPSModelBrowserPane::OnSelectionChanged(NMHDR * pNMHDR, LRESULT * pResult)
{
	NM_TREEVIEW * pNMTreeView = (NM_TREEVIEW *)pNMHDR;
	TVITEM treeItem = pNMTreeView->itemNew;
	MFCComponentTreeItem * componentItem = (MFCComponentTreeItem *)treeItem.lParam;

	OnSelection(componentItem);

	//componentItem->GetComponent().
	//AfxGetApp()->m_pMainWnd->PostMessageW(WM_MFC_SANDBOX_ADD_PROPERTY, reinterpret_cast<WPARAM>(sceneItem), 0);


	*pResult = 0;
}

void CHPSModelBrowserPane::OnClick(NMHDR * /*pNMHDR*/, LRESULT * /*pResult*/) 
{
	HTREEITEM item;
	UINT flags;
	DWORD pos = GetMessagePos();
	CPoint point(GET_X_LPARAM(pos), GET_Y_LPARAM(pos));

	if (currentTreeCtrl != nullptr)
	{
		currentTreeCtrl->ScreenToClient(&point);
		item = currentTreeCtrl->HitTest(point, &flags);
		if (item != NULL && (flags & TVHT_ONITEMLABEL))
		{
			HTREEITEM previously_selected_item = currentTreeCtrl->GetSelectedItem();
			if (previously_selected_item != item)
			{
				currentTreeCtrl->SelectItem(item);	//selecting an item which was not previously selected will trigger OnSelectionChanged
				return;
			}
			else
			{
				DWORD_PTR data = currentTreeCtrl->GetItemData(item);
				if (data != NULL)
					OnSelection((MFCComponentTreeItem *)data);
			}
		}
	}
}



void ModelTreeCtrl::OnMbContextHighlight()
{

	CHPSView * cview = GetCHPSDoc()->GetCHPSView();
	cview->Unhighlight();

	HPS::Canvas canvas = cview->GetCanvas();

	HPS::HighlightOptionsKit highlightOptions;
	highlightOptions.SetStyleName("highlight_style").SetNotification(true);
	highlightOptions.SetSubentityHighlighting(true);


	//contextItem->GetPath().Highlight(canvas, highlightOptions,false);


	HPS::KeyPathArray kpa = contextItem->GetPath().GetKeyPaths();
	HPS::KeyPath kp0 = kpa[0];
	HPS::WindowKey wkey = canvas.GetWindowKey();
	wkey.GetHighlightControl().Reset();
	wkey.GetHighlightControl().Highlight(kp0, highlightOptions,true);

	highlightOptions.SetNotification(false);

	HPS::Component const & leaf_component = contextItem->GetPath().Front();

	for (auto const & reference : leaf_component.GetReferences())
		HPS::ComponentPath(1, &reference).Highlight(canvas, highlightOptions,true);


	cview->Update();
}


void ModelTreeCtrl::OnMbContextWalkto()
{

	CHPSView * pView = GetCHPSDoc()->GetCHPSView();

	HPS::View frontView = pView->GetCanvas().GetFrontView();
		
	IFCUtils::MoveCameraToIFCSpace(contextItem->GetPath(), frontView);


}


void ModelTreeCtrl::OnMbContextAddtoBOM()
{
	HPS::Component const & leaf_component = contextItem->GetPath().Front();
	CHPSFrame* pFrame = (CHPSFrame*)(AfxGetApp()->m_pMainWnd);
	pFrame->TagComponent(leaf_component);
	AfxGetApp()->m_pMainWnd->PostMessageW(WM_MFC_SANDBOX_COMPONENT_QUANTIFY, 0, 0);
}

//! [OnMbContextAddmodel]
void ModelTreeCtrl::OnMbContextAddmodel()
{
	HPS::ComponentPath mergePath = contextItem->GetPath();

	CHPSDoc *pDoc = GetCHPSDoc();

	pDoc->GetCHPSView()->OnFileOpenAndMerge(mergePath);

}
//! [OnMbContextAddmodel]