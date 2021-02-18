#pragma once

#include "afxdockablepane.h"
#include "sprk.h"

#ifdef USING_EXCHANGE
#include "sprk_exchange.h"
#endif

class CHPSConfigurationPane : public CDockablePane
{
public:
	CHPSConfigurationPane();
	~CHPSConfigurationPane();

	void			Init();
	void			Flush();

private:
	CTreeCtrl		configurationTree;

#ifdef USING_EXCHANGE
	void			InsertConfigurationInTree(HTREEITEM const & root, HPS::Exchange::Configuration const & configuration);
	HTREEITEM		FindConfigurationInTree(HPS::UTF8Array const & configuration, HTREEITEM const & root);
	HPS::UTF8Array	GetSelectedConfiguration();
#endif

	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnPaint();
	afx_msg void OnConfigurationTreeDblClick(NMHDR * pNMHDR, LRESULT * pResult);
	BOOL OnShowControlBarMenu(CPoint point);
};

