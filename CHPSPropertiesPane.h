#pragma once

#include "sprk.h"

class MFCSceneTreeItem;

namespace Property
{
	class RootProperty;
}


class CHPSPropertiesPane : public CDockablePane
{
public:
	CHPSPropertiesPane();
	virtual ~CHPSPropertiesPane();

	void AddProperty(
		MFCSceneTreeItem * treeItem);
	void AddProperty(
		MFCSceneTreeItem * treeItem,
		HPS::SceneTree::ItemType itemType);

	void UnsetAttribute(
		MFCSceneTreeItem * treeItem);

	void Flush();

	void AdjustLayout();

protected:
	CMFCPropertyGridCtrl propertyCtrl;
	CButton applyButton;
	CFont font;

	virtual BOOL OnShowControlBarMenu(CPoint pt);

	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnPaint();
	afx_msg void OnApplyButton();
	afx_msg void OnUpdateApplyButton(CCmdUI *pCmdUI);

	DECLARE_MESSAGE_MAP()

private:
	void ReExpandTree();
	void UpdateCanvas();

private:
	MFCSceneTreeItem * sceneTreeItem;
	std::unique_ptr<Property::RootProperty> rootProperty;
};

