#pragma once

#include "CHPSSegmentBrowserPane.h"
#include "CHPSPropertiesPane.h"
#include "CHPSModelBrowserPane.h"
#include "CHPSConfigurationPane.h"
#include "CHPSComponentPropertiesPane.h"
#include "CComponentQuantitiesDlg.h"


class CHPSFrame : public CFrameWndEx
{
protected:
	virtual ~CHPSFrame();
	virtual BOOL		PreCreateWindow(CREATESTRUCT& cs);

public:
	void SetPropertiesPaneVisibility(bool state);
	bool GetPropertiesPaneVisibility() const;

	void SetComponentPropertiesPaneVisibility(bool state);
	bool GetComponentPropertiesPaneVisibility() const;

	void SetComponentQuantitiesDialogVisibility(bool state);
	bool GetComponentQuantitiesDialogVisibility() const;

	void  TagComponent(HPS::Component tagged) { m_TaggedComponent = tagged; }
	void  UntagComponent() { m_TaggedComponent.Reset(); }

	CHPSComponentPropertiesPane *GetComponentPropertiesPane() const;

protected:
	CHPSFrame();
	DECLARE_DYNCREATE(CHPSFrame)

	CMFCRibbonBar			m_wndRibbonBar;
	CHPSSegmentBrowserPane	m_segmentBrowserPane;
	CHPSPropertiesPane		m_propertiesPane;
	CTabbedPane				m_tabbedPane;
	CHPSModelBrowserPane	m_modelBrowserPane;
	CHPSConfigurationPane	m_configurationPane;
	CHPSComponentPropertiesPane	m_componentPropertiesPane;
	CHPSComponentQuantitiesDlg		m_componentQuantitiesDlg;
	HPS::Component                  m_TaggedComponent;

	afx_msg int			OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void		OnApplicationLook(UINT id);
	afx_msg void		OnUpdateApplicationLook(CCmdUI* pCmdUI);
	afx_msg LRESULT		InitializeBrowsers(WPARAM w, LPARAM l);
	afx_msg LRESULT		AddProperty(WPARAM w, LPARAM l);
	afx_msg void		OnModesSegmentBrowser();
	afx_msg void		OnUpdateModesSegmentBrowser(CCmdUI *pCmdUI);
	afx_msg LRESULT		FlushProperties(WPARAM w, LPARAM l);
	afx_msg void		OnModesModelBrowser();
	afx_msg void		OnUpdateModesModelBrowser(CCmdUI *pCmdUI);
	afx_msg void		OnComboSelectionLevel();
	afx_msg LRESULT		UnsetAttribute(WPARAM w, LPARAM l);
	afx_msg LRESULT		OnComponentQuantify(WPARAM w, LPARAM l);


	DECLARE_MESSAGE_MAP()
};


