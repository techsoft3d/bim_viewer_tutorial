#pragma once

#include "sprk.h"
class CHPSComponentPropertiesPane;

//! [IFCHighlightHandler::Declaration]

class IFCHighlightHandler : public HPS::EventHandler
{
public:
	~IFCHighlightHandler() { Shutdown(); }
	EventHandler::HandleResult Handle(HPS::Event const * pInEvent);

	CHPSComponentPropertiesPane * _parentPane;
	HPS::Component _currentComponent;
};

//! [IFCHighlightHandler::Declaration]

//! [CHPSComponentPropertiesPane::Declaration]

class CHPSComponentPropertiesPane : public CDockablePane
{
public:
	CHPSComponentPropertiesPane();
	virtual ~CHPSComponentPropertiesPane();

	void SetComponentProperties(HPS::Component &component);

	void Flush();
	void AdjustLayout();

protected:
	CMFCPropertyGridCtrl _propertyCtrl;

	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnPaint();
	afx_msg LRESULT	OnComponentHighlighted(WPARAM w, LPARAM l);
	DECLARE_MESSAGE_MAP()

private:
	void UpdateCanvas();

private:

	IFCHighlightHandler _highlightHandler;
};

//! [CHPSComponentPropertiesPane::Declaration]
