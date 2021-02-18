#pragma once

#include "afxdockablepane.h"
#include "sprk.h"

class CHPSModelBrowserPane;
class MFCComponentTree;
class MFCComponentTreeItem;


typedef std::shared_ptr<MFCComponentTree> MFCComponentTreePtr;


class ModelTreeCtrl : public CTreeCtrl
{
	afx_msg void OnRClick(NMHDR * pNMHDR, LRESULT * pResult);
	afx_msg void OnContextMenu(CWnd * pWnd, CPoint ptMousePos);
	afx_msg void OnShowHide();
	afx_msg void OnDWGLayerToggle();
	afx_msg void OnIsolate();
	afx_msg void OnResetVisibility();
	DECLARE_MESSAGE_MAP()

	MFCComponentTreeItem * contextItem;
public:
	afx_msg void OnMbContextHighlight();
	afx_msg void OnMbContextWalkto();
	afx_msg void OnMbContextAddtoBOM();
	afx_msg void OnMbContextAddmodel();
};


class MFCComponentTree : public HPS::ComponentTree
{
public:
	MFCComponentTree(HPS::Canvas const & in_canvas, CHPSModelBrowserPane * in_browser)
		: HPS::ComponentTree(in_canvas), browser(in_browser) {}

	CTreeCtrl * GetTreeCtrl();
	CHPSModelBrowserPane * GetModelBrowserPane() { return browser; }

	virtual void Flush() override
	{
		HPS::ComponentTree::Flush();
		GetTreeCtrl()->DeleteAllItems();
	}

private:
	CHPSModelBrowserPane * browser;
};

class MFCComponentTreeItem : public HPS::ComponentTreeItem
{
public:
	MFCComponentTreeItem(HPS::ComponentTreePtr const & in_tree, HPS::CADModel const & in_cad_model)
		: HPS::ComponentTreeItem(in_tree, in_cad_model), treeItem(nullptr) {}

	MFCComponentTreeItem(HPS::ComponentTreePtr const & in_tree, HPS::Component const & in_component, HPS::ComponentTree::ItemType in_type)
		: HPS::ComponentTreeItem(in_tree, in_component, in_type), treeItem(nullptr) {}

	virtual HPS::ComponentTreeItemPtr AddChild(HPS::Component const & in_component, HPS::ComponentTree::ItemType in_type)
	{
		auto child = std::make_shared<MFCComponentTreeItem>(GetTree(), in_component, in_type);

		HPS::UTF8 title = child->GetTitle();
		if (title.Empty())
			title = "Unnamed";
		HPS::WCharArray wstr;
		title.ToWStr(wstr);

		TVINSERTSTRUCT is;
		is.hParent = GetTreeItem();
		is.hInsertAfter = TVI_LAST;
		is.item.mask = TVIF_CHILDREN | TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
		is.item.cChildren = child->HasChildren();
		is.item.lParam = (LPARAM)child.get();
		is.item.pszText = (LPTSTR)wstr.data();
		is.item.iImage = GetImageIndex(*child);
		is.item.iSelectedImage = is.item.iImage;

		child->SetTreeItem(GetTreeCtrl()->InsertItem(&is));

		return child;
	}

	virtual void Expand() override
	{
		CTreeCtrl * treeCtrl = GetTreeCtrl();
		if (treeCtrl != nullptr)
		{
			if (GetTreeItem() == nullptr)
				HPS::ComponentTreeItem::Expand();
			else
			{
				bool is_expanded = ((treeCtrl->GetItemState(GetTreeItem(), TVIS_EXPANDED) & TVIS_EXPANDED) != 0);
				if (is_expanded == false)
				{
					HPS::ComponentTreeItem::Expand();
					treeCtrl->SetItemState(treeItem, TVIS_EXPANDED, TVIS_EXPANDED);
					// this will trigger the TVN_ITEMEXPANDING message, which will in turn trigger this function
					// so to avoid stack overflow, we set the item state as TVIS_EXPANDED to avoid invoking this repeatedly
					treeCtrl->Expand(GetTreeItem(), TVE_EXPAND);
				}
				else
					treeCtrl->SetItemState(GetTreeItem(), 0, TVIS_EXPANDED);
			}
		}
	}

	virtual void Collapse() override
	{
		HPS::ComponentTreeItem::Collapse();
		// must remove all child elements since they will be reinserted
		CTreeCtrl * treeCtrl = GetTreeCtrl();
		if (treeCtrl != nullptr)
			treeCtrl->Expand(GetTreeItem(), TVE_COLLAPSE | TVE_COLLAPSERESET);
	}

	virtual void OnHighlight(HPS::HighlightOptionsKit const & in_options) override;
	virtual void OnUnhighlight(HPS::HighlightOptionsKit const & in_options) override;

	virtual void OnHide() override;
	virtual void OnShow() override;

	inline CHPSModelBrowserPane * GetModelBrowserPane() const { return std::static_pointer_cast<MFCComponentTree>(GetTree())->GetModelBrowserPane(); }

private:
	inline void SetTreeItem(HTREEITEM in_item) { treeItem = in_item; }
	inline HTREEITEM GetTreeItem() const { return treeItem; }
	inline CTreeCtrl * GetTreeCtrl() const { return std::static_pointer_cast<MFCComponentTree>(GetTree())->GetTreeCtrl(); }
	int GetImageIndex(HPS::ComponentTreeItem const & item);
	int GetHiddenImageIndex(HPS::ComponentTreeItem const & item);

	HTREEITEM treeItem;
};

class CHPSModelBrowserPane : public CDockablePane
{
public:
	enum Image
	{
		Unknown = 0,

		ModelFile,
		ProductStructureWithChildren,
		ProductStructureWithoutChildren,
		RISolid,
		RISurface,
		RIPlane,
		RILine,
		RIArc,
		RICompCurve,
		RICurve,
		RIPoint,
		RIPointCloud,
		RIDirection,
		RICoordinateSystem,
		RIGeometrySet,

		View,
		AnnotationView,
		ViewGroup,

		PMIText,
		PMITolerance,
		PMIDimensionDistance,
		PMIDimensionLength,
		PMIDimensionRadius,
		PMIDimensionDiameter,
		PMIDimensionAngle,
		PMIDatum,
		PMIDatumTarget,
		PMIRoughness,
		PMICircleCenter,
		PMILineWelding,
		PMISpotWelding,
		PMIBalloon,
		PMICoordinate,
		PMIFastener,
		PMILocator,
		PMIMeasurementPoint,
		PMIArrow,
		PMITable,
		PMIGroup,

		DrawingModel,
		DrawingSheet,
		DrawingView,
		DrawingViewBack,
		DrawingViewBackground,
		DrawingViewBottom,
		DrawingViewDetail,
		DrawingViewFront,
		DrawingViewIso,
		DrawingViewLeft,
		DrawingViewRight,
		DrawingViewSection,
		DrawingViewTop,

		ParasolidModel = ModelFile,
		ParasolidAssembly = ProductStructureWithoutChildren,
		ParasolidBody = RISolid,
	};

	CHPSModelBrowserPane();
	~CHPSModelBrowserPane();

	void Init();
	void Flush();

	CImageList * GetImageList() { return &imageList; }
	CTreeCtrl * GetModelTreeCtrl() { return &modelTree; }

	int GetImageBaseIndex(Image type) const { return imageBaseIndices[type]; }

private:
	MFCComponentTreePtr componentTree;
	ModelTreeCtrl modelTree;
	CTreeCtrl emptyTree;
	CTreeCtrl * currentTreeCtrl;
	CImageList imageList;
	HPS::IntArray imageBaseIndices;

	void OnSelection(MFCComponentTreeItem * componentItem);

	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnPaint();
	afx_msg void OnItemExpanding(NMHDR * pNMHDR, LRESULT * pResult);
	afx_msg void OnSelectionChanged(NMHDR * pNMHDR, LRESULT * pResult);
	afx_msg void OnClick(NMHDR* pNMHDR, LRESULT* pResult);
public:
};

