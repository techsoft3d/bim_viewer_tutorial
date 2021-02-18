#pragma once

#include "sprk.h"

class MFCSceneTree : public HPS::SceneTree
{
public:
	MFCSceneTree(HPS::Canvas const & in_canvas, CTreeCtrl * in_ctrl)
		: HPS::SceneTree(in_canvas), m_pTreeCtrl(in_ctrl) {}

	CTreeCtrl * GetTreeCtrl() const { return m_pTreeCtrl; }

	void Flush() override
	{
		HPS::SceneTree::Flush();
		m_pTreeCtrl->DeleteAllItems();
	}

private:
	CTreeCtrl * m_pTreeCtrl;
};

class MFCSceneTreeItem : public HPS::SceneTreeItem
{
public:
	MFCSceneTreeItem(HPS::SceneTreePtr const & in_tree, HPS::Model const & in_model)
		: HPS::SceneTreeItem(in_tree, in_model), m_pTreeItem(nullptr) {}

	MFCSceneTreeItem(HPS::SceneTreePtr const & in_tree, HPS::View const & in_view)
		: HPS::SceneTreeItem(in_tree, in_view), m_pTreeItem(nullptr) {}

	MFCSceneTreeItem(HPS::SceneTreePtr const & in_tree, HPS::Layout const & in_layout)
		: HPS::SceneTreeItem(in_tree, in_layout), m_pTreeItem(nullptr) {}

	MFCSceneTreeItem(HPS::SceneTreePtr const & in_tree, HPS::Canvas const & in_canvas)
		: HPS::SceneTreeItem(in_tree, in_canvas), m_pTreeItem(nullptr) {}

	MFCSceneTreeItem(HPS::SceneTreePtr const & in_tree, HPS::Key const & in_key, HPS::SceneTree::ItemType in_type, char const * in_title = nullptr)
		: HPS::SceneTreeItem(in_tree, in_key, in_type, in_title), m_pTreeItem(nullptr) {}

	virtual ~MFCSceneTreeItem()
	{
		// if we delete items during a collapse, MFC won't let us expand a collapsed tree
		//GetTreeCtrl()->DeleteItem(GetTreeItem());
	}

	HPS::SceneTreeItemPtr AddChild(HPS::Key const & in_key, HPS::SceneTree::ItemType in_type, char const * in_title) override
	{
		auto child = std::make_shared<MFCSceneTreeItem>(GetTree(), in_key, in_type, in_title);

		HPS::WCharArray wstr;
		child->GetTitle().ToWStr(wstr);

		TVINSERTSTRUCT is;
		is.hParent = GetTreeItem();
		is.hInsertAfter = TVI_LAST;
		is.item.mask = TVIF_CHILDREN | TVIF_TEXT | TVIF_PARAM | TVIF_STATE;
		is.item.cChildren = child->HasChildren();
		is.item.lParam = (LPARAM)child.get();
		is.item.pszText = (LPTSTR)wstr.data();
		is.item.stateMask = TVIS_STATEIMAGEMASK;
		is.item.state = INDEXTOSTATEIMAGEMASK(GetImageIndex(*child, Unselected));

		child->SetTreeItem(GetTreeCtrl()->InsertItem(&is));

		return child;
	}

	void Expand() override
	{
		CTreeCtrl * treeCtrl = GetTreeCtrl();
		if (treeCtrl != nullptr)
		{
			if (GetTreeItem() == nullptr)
				HPS::SceneTreeItem::Expand();
			else
			{
				bool is_expanded = ((treeCtrl->GetItemState(GetTreeItem(), TVIS_EXPANDED) & TVIS_EXPANDED) != 0);
				if (is_expanded == false)
				{
					HPS::SceneTreeItem::Expand();
					treeCtrl->SetItemState(GetTreeItem(), TVIS_EXPANDED, TVIS_EXPANDED);
					// this will trigger the TVN_ITEMEXPANDING message, which will in turn trigger this function
					// so to avoid stack overflow, we set the item state as TVIS_EXPANDED to avoid invoking this repeatedly
					treeCtrl->Expand(GetTreeItem(), TVE_EXPAND);
				}
				else
					treeCtrl->SetItemState(GetTreeItem(), 0, TVIS_EXPANDED);
			}
		}
	}

	void Collapse() override
	{
		HPS::SceneTreeItem::Collapse();
		// must remove all child elements since they will be reinserted
		CTreeCtrl * treeCtrl = GetTreeCtrl();
		if (treeCtrl != nullptr)
			treeCtrl->Expand(GetTreeItem(), TVE_COLLAPSE | TVE_COLLAPSERESET);
	}

	void Select() override
	{
		HPS::SceneTreeItem::Select();
		CTreeCtrl * treeCtrl = GetTreeCtrl();
		if (treeCtrl != nullptr)
			treeCtrl->SetItemState(m_pTreeItem, TVIS_BOLD | INDEXTOSTATEIMAGEMASK(GetImageIndex(*this, Selected)), TVIS_BOLD | TVIS_STATEIMAGEMASK);
	}

	void Unselect() override
	{
		HPS::SceneTreeItem::Unselect();
		CTreeCtrl * treeCtrl = GetTreeCtrl();
		if (treeCtrl != nullptr)
			treeCtrl->SetItemState(m_pTreeItem, INDEXTOSTATEIMAGEMASK(GetImageIndex(*this, Unselected)), TVIS_BOLD | TVIS_STATEIMAGEMASK);
	}

	inline HTREEITEM GetTreeItem() const { return m_pTreeItem; }

	inline CTreeCtrl * GetTreeCtrl() const
	{
		HPS::SceneTreePtr tree = GetTree();
		if (tree != nullptr)
			return std::static_pointer_cast<MFCSceneTree>(tree)->GetTreeCtrl();
		return nullptr;
	}

private:
	enum SelectionState
	{
		Selected,
		Unselected
	};

	inline void SetTreeItem(HTREEITEM in_item) { m_pTreeItem = in_item; }
	
	int GetImageIndex(HPS::SceneTreeItem const & item, SelectionState state)
	{
		HPS::SceneTree::ItemType itemType = item.GetItemType();
		switch (itemType)
		{
			case HPS::SceneTree::ItemType::Segment:
				return (state == Unselected ? 1 : 2);

			case HPS::SceneTree::ItemType::ConditionalExpression:
			case HPS::SceneTree::ItemType::NamedStyle:
			case HPS::SceneTree::ItemType::SegmentStyle:
			case HPS::SceneTree::ItemType::StyleGroup:
			case HPS::SceneTree::ItemType::Include:
			case HPS::SceneTree::ItemType::IncludeGroup:
			case HPS::SceneTree::ItemType::Portfolio:
			case HPS::SceneTree::ItemType::PortfolioGroup:
				return 5;

			case HPS::SceneTree::ItemType::StaticModelSegment:
				return 12;

			case HPS::SceneTree::ItemType::Reference:
				return 13;

			default:
			{
				if (item.HasItemType(HPS::SceneTree::ItemType::Attribute) || itemType == HPS::SceneTree::ItemType::AttributeGroup)
					return 4;
				else if (item.HasItemType(HPS::SceneTree::ItemType::Geometry) || itemType == HPS::SceneTree::ItemType::GeometryGroup
					|| item.HasItemType(HPS::SceneTree::ItemType::Definition) || item.HasItemType(HPS::SceneTree::ItemType::DefinitionGroup))
					return (state == Unselected ? 3 : 6);
				else if (item.HasItemType(HPS::SceneTree::ItemType::Group))
					return (state == Unselected ? 15 : 7);
			}
		}

		return -1;
	}

	HTREEITEM m_pTreeItem;
};

class CHPSTreeCtrl : public CTreeCtrl
{
public:
	CHPSTreeCtrl();

private:
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRClick(NMHDR * pNMHDR, LRESULT * pResult);
	afx_msg void OnContextMenu(CWnd * pWnd, CPoint ptMousePos);

	afx_msg void OnAddMaterial();
	afx_msg void OnAddCamera();
	afx_msg void OnAddModellingMatrix();
	afx_msg void OnAddTextureMatrix();
	afx_msg void OnAddCulling();
	afx_msg void OnAddCurveAttribute();
	afx_msg void OnAddCylinderAttribute();
	afx_msg void OnAddEdgeAttribute();
	afx_msg void OnAddLightingAttribute();
	afx_msg void OnAddLineAttribute();
	afx_msg void OnAddMarkerAttribute();
	afx_msg void OnAddSurfaceAttribute();
	afx_msg void OnAddSelectability();
	afx_msg void OnAddSphereAttribute();
	afx_msg void OnAddSubwindow();
	afx_msg void OnAddTextAttribute();
	afx_msg void OnAddTransparency();
	afx_msg void OnAddVisibility();
	afx_msg void OnAddVisualEffects();
	afx_msg void OnAddPerformance();
	afx_msg void OnAddDrawingAttribute();
	afx_msg void OnAddHiddenLineAttribute();
	afx_msg void OnAddMaterialPalette();
	afx_msg void OnAddContourLine();
	afx_msg void OnAddCondition();
	afx_msg void OnAddBounding();
	afx_msg void OnAddAttributeLock();
	afx_msg void OnAddTransformMask();
	afx_msg void OnAddColorInterpolation();
	afx_msg void OnAddCuttingSectionAttribute();
	afx_msg void OnAddPriority();
	afx_msg void OnUnsetAttribute();
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);

	DECLARE_MESSAGE_MAP()

private:
	MFCSceneTreeItem * selectedSceneTreeItem;
	std::unordered_map<HPS::SceneTree::ItemType, HPS::Search::Type> searchTypeMap;
};

class CHPSSegmentBrowserPane : public CDockablePane
{
public:
	CHPSSegmentBrowserPane();
	virtual ~CHPSSegmentBrowserPane();

	void Init();

	void AdjustLayout();

protected:
	CHPSTreeCtrl m_TreeCtrl;
	CImageList m_ImageList;
	CComboBox m_ComboBox;
	CFont m_Font;
	CButton m_CheckBox;

	std::shared_ptr<MFCSceneTree> m_pSceneTree;

	virtual BOOL OnShowControlBarMenu(CPoint pt);

	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnPaint();
	afx_msg void OnSetFocus(CWnd * pOldWnd);
	afx_msg void OnItemExpanding(NMHDR * pNMHDR, LRESULT * pResult);
	afx_msg void OnSelectionChanged(NMHDR * pNMHDR, LRESULT * pResult);
	afx_msg void OnProperitesCheckBox();
	afx_msg void OnUpdatePropertiesCheckBox(CCmdUI *pCmdUI);
	afx_msg void OnSelectedRoot();

	DECLARE_MESSAGE_MAP()
};

