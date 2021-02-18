#include "stdafx.h"
#include "CHPSComponentPropertiesPane.h"
#include "CHPSApp.h"
#include "CHPSDoc.h"
#include "CHPSView.h"
#include "CHPSSegmentBrowserPane.h"
#include "resource.h"
#include "IFCUtils.h"


//! [IFCHighlightHandler::Handle]
HPS::EventHandler::HandleResult IFCHighlightHandler::Handle(HPS::Event const * pInEvent)
{

	HPS::ComponentHighlightEvent * pCHEvent = (HPS::ComponentHighlightEvent *)pInEvent;
	if (!pCHEvent->path.Empty())
	{
		if (NULL != _parentPane)
		{
			_currentComponent = pCHEvent->path.Front();
			_parentPane->PostMessageW(WM_MFC_SANDBOX_COMPONENT_HIGHLIGHT, 0, 0);
		}
	}


	return HandleResult::NotHandled;
}
//! [IFCHighlightHandler::Handle]

//! [CHPSComponentPropertiesPane::Definition]

CHPSComponentPropertiesPane::CHPSComponentPropertiesPane()
{
}


CHPSComponentPropertiesPane::~CHPSComponentPropertiesPane()
{

}


BEGIN_MESSAGE_MAP(CHPSComponentPropertiesPane, CDockablePane)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_PAINT()
	ON_MESSAGE(WM_MFC_SANDBOX_COMPONENT_HIGHLIGHT, &OnComponentHighlighted)
END_MESSAGE_MAP()


int CHPSComponentPropertiesPane::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CDockablePane::OnCreate(lpCreateStruct) == -1)
		return -1;

	CRect rectDummy;
	rectDummy.SetRectEmpty();

	if (!_propertyCtrl.Create(WS_VISIBLE | WS_CHILD, rectDummy, this, ID_COMPONENTPROPS_CTRL))
	{
		TRACE0("Failed to create properties window \n");
		return -1;      // fail to create
	}

	_propertyCtrl.EnableHeaderCtrl(TRUE, _T("Property"), _T("Value"));
	_propertyCtrl.SetVSDotNetLook();



	HPS::EventDispatcher dispatcher = HPS::Database::GetEventDispatcher();
	_highlightHandler._parentPane = this;
	dispatcher.Subscribe(_highlightHandler, HPS::Object::ClassID<HPS::ComponentHighlightEvent>());

	return 0;
}

afx_msg LRESULT	CHPSComponentPropertiesPane::OnComponentHighlighted(WPARAM w, LPARAM l)
{
	SetComponentProperties( _highlightHandler._currentComponent);
	return 0;
}

void CHPSComponentPropertiesPane::OnSize(UINT nType, int cx, int cy)
{
	CDockablePane::OnSize(nType, cx, cy);
	AdjustLayout();
}

void CHPSComponentPropertiesPane::AdjustLayout()
{
	if (GetSafeHwnd() == NULL || (AfxGetMainWnd() != NULL && AfxGetMainWnd()->IsIconic()))
		return;


	CRect rectClient;
	GetClientRect(rectClient);

	_propertyCtrl.SetWindowPos(NULL, rectClient.left + 3, rectClient.top + 2, rectClient.Width() - 4, rectClient.Height()  - 7, SWP_NOACTIVATE | SWP_NOZORDER);
}


void CHPSComponentPropertiesPane::OnPaint()
{
	CPaintDC dc(this); // device context for painting

	CRect paneRect;
	GetWindowRect(paneRect);
	ScreenToClient(paneRect);

	CBrush bg;
	bg.CreateStockObject(WHITE_BRUSH);
	dc.FillRect(&paneRect, &bg);

	CRect rectPropertyCtrl;
	_propertyCtrl.GetWindowRect(rectPropertyCtrl);
	ScreenToClient(rectPropertyCtrl);
	rectPropertyCtrl.InflateRect(1, 1);
	dc.Draw3dRect(rectPropertyCtrl, ::GetSysColor(COLOR_3DSHADOW), ::GetSysColor(COLOR_3DSHADOW));
}

void CHPSComponentPropertiesPane::Flush()
{
	SetWindowText(_T("Component Properties"));
	_propertyCtrl.RemoveAll();
	_propertyCtrl.AdjustLayout();
}

void CHPSComponentPropertiesPane::SetComponentProperties(HPS::Component &component)
{
	_propertyCtrl.RemoveAll();

	std::vector< std::pair<HPS::UTF8, HPS::UTF8>>  properties;
	IFCUtils::GetComponentMetaData(component, properties);

	for (auto const &keyValue : properties)
	{
		CString TextName(keyValue.first);
		CString TextValue(keyValue.second);
		CMFCPropertyGridProperty* pTextVal = new CMFCPropertyGridProperty(TextName, TextValue);
		_propertyCtrl.AddProperty(pTextVal);
	}
	_propertyCtrl.AdjustLayout();
	_propertyCtrl.RedrawWindow();
}

void CHPSComponentPropertiesPane::UpdateCanvas()
{
	CHPSDoc * doc = GetCHPSDoc();
	HPS::Canvas canvas = doc->GetCHPSView()->GetCanvas();
	canvas.Update();
}

//! [CHPSComponentPropertiesPane::Definition]
