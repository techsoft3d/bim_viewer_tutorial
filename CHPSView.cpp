#include "stdafx.h"
#include "CHPSApp.h"
#include "CHPSFrame.h"
#include "CHPSDoc.h"
#include "CHPSView.h"
#include "sprk.h"
#include "SandboxHighlightOp.h"
#include "CProgressDialog.h"
#include <WinUser.h>
#include <sstream>
#include <vector>
#include "IFCSampleApp.h"
#include "IFCSampleModel.h"
#include "IFCSamplePlanView.h"
#include "IFCSamplePreview.h"
#include "IFCUtils.h"
 
IFCSamplePlanView gblPlanView;  // temporary, move to better location
IFCSamplePreview  gblPreview;
float             gblPlaneAdjust = 0;


#ifdef USING_PUBLISH
#include "sprk_publish.h"
#endif


using namespace HPS;

IMPLEMENT_DYNCREATE(CHPSView, CView)

BEGIN_MESSAGE_MAP(CHPSView, CView)
	ON_WM_PAINT()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_RBUTTONDBLCLK()
	ON_WM_MBUTTONDOWN()
	ON_WM_MBUTTONUP()
	ON_WM_MBUTTONDBLCLK()
	ON_WM_MOUSEHWHEEL()
	ON_WM_MOUSEWHEEL()
	ON_WM_KEYDOWN()
	ON_WM_KEYUP()
	ON_WM_KILLFOCUS()
	ON_WM_SETFOCUS()
	ON_COMMAND(ID_EDIT_COPY, &CHPSView::OnEditCopy)
	ON_COMMAND(ID_FILE_OPEN, &CHPSView::OnFileOpen)
	ON_COMMAND(ID_FILE_SAVE_AS, &CHPSView::OnFileSaveAs)
	ON_COMMAND(ID_OPERATORS_ORBIT, &CHPSView::OnOperatorsOrbit)
	ON_COMMAND(ID_OPERATORS_PAN, &CHPSView::OnOperatorsPan)
	ON_COMMAND(ID_OPERATORS_ZOOM_AREA, &CHPSView::OnOperatorsZoomArea)
	ON_COMMAND(ID_OPERATORS_FLY, &CHPSView::OnOperatorsFly)
	ON_COMMAND(ID_OPERATORS_HOME, &CHPSView::OnOperatorsHome)
	ON_COMMAND(ID_OPERATORS_ZOOM_FIT, &CHPSView::OnOperatorsZoomFit)
	ON_COMMAND(ID_OPERATORS_SELECT_POINT, &CHPSView::OnOperatorsSelectPoint)
	ON_COMMAND(ID_OPERATORS_SELECT_AREA, &CHPSView::OnOperatorsSelectArea)
	ON_COMMAND(ID_MODES_SIMPLE_SHADOW, &CHPSView::OnModesSimpleShadow)
	ON_COMMAND(ID_MODES_SMOOTH, &CHPSView::OnModesSmooth)
	ON_COMMAND(ID_MODES_HIDDEN_LINE, &CHPSView::OnModesHiddenLine)
	ON_COMMAND(ID_MODES_FRAME_RATE, &CHPSView::OnModesFrameRate)
	ON_COMMAND(ID_USER_CODE_1, &CHPSView::OnUserCode1)
	ON_COMMAND(ID_USER_CODE_2, &CHPSView::OnUserCode2)
	ON_COMMAND(ID_USER_CODE_3, &CHPSView::OnUserCode3)
	ON_COMMAND(ID_USER_CODE_4, &CHPSView::OnUserCode4)
	ON_WM_MOUSELEAVE()
	ON_UPDATE_COMMAND_UI(ID_MODES_SIMPLE_SHADOW, &CHPSView::OnUpdateRibbonBtnSimpleShadow)
	ON_UPDATE_COMMAND_UI(ID_MODES_FRAME_RATE, &CHPSView::OnUpdateRibbonBtnFrameRate)
	ON_UPDATE_COMMAND_UI(ID_MODES_SMOOTH, &CHPSView::OnUpdateRibbonBtnSmooth)
	ON_UPDATE_COMMAND_UI(ID_MODES_HIDDEN_LINE, &CHPSView::OnUpdateRibbonBtnHiddenLine)
	ON_UPDATE_COMMAND_UI(ID_OPERATORS_ORBIT, &CHPSView::OnUpdateRibbonBtnOrbitOp)
	ON_UPDATE_COMMAND_UI(ID_OPERATORS_PAN, &CHPSView::OnUpdateRibbonBtnPanOp)
	ON_UPDATE_COMMAND_UI(ID_OPERATORS_ZOOM_AREA, &CHPSView::OnUpdateRibbonBtnZoomAreaOp)
	ON_UPDATE_COMMAND_UI(ID_OPERATORS_FLY, &CHPSView::OnUpdateRibbonBtnFlyOp)
	ON_UPDATE_COMMAND_UI(ID_OPERATORS_SELECT_POINT, &CHPSView::OnUpdateRibbonBtnPointOp)
	ON_UPDATE_COMMAND_UI(ID_OPERATORS_SELECT_AREA, &CHPSView::OnUpdateRibbonBtnAreaOp)
	ON_COMMAND(ID_COMBO_FLOOR, &CHPSView::OnComboFloor)
	ON_COMMAND(ID_CHECK_COMP_PROPS, &CHPSView::OnCheckCompProps)
	ON_UPDATE_COMMAND_UI(ID_CHECK_COMP_PROPS, &CHPSView::OnUpdateCheckCompProps)
	ON_COMMAND(ID_CHK_COMPONENT_QUANTITIES, &CHPSView::OnChkComponentQuantities)
	ON_UPDATE_COMMAND_UI(ID_CHK_COMPONENT_QUANTITIES, &CHPSView::OnUpdateChkComponentQuantities)
	ON_COMMAND(ID_CHECK_PLAN, &CHPSView::OnCheckPlan)
	ON_UPDATE_COMMAND_UI(ID_CHECK_PLAN, &CHPSView::OnUpdateCheckPlan)
	ON_COMMAND(ID_CHECK_PREVIEW, &CHPSView::OnCheckPreview)
	ON_UPDATE_COMMAND_UI(ID_CHECK_PREVIEW, &CHPSView::OnUpdateCheckPreview)
END_MESSAGE_MAP()

CHPSView::CHPSView()
	: _enableSimpleShadows(false),
	_enableFrameRate(false),
	_displayResourceMonitor(false),
	_capsLockState(false)
{
	_operatorStates[SandboxOperators::OrbitOperator] = true;
	_operatorStates[SandboxOperators::PanOperator] = false;
	_operatorStates[SandboxOperators::ZoomAreaOperator] = false;
	_operatorStates[SandboxOperators::PointOperator] = false;
	_operatorStates[SandboxOperators::AreaOperator] = false;

	_capsLockState = IsCapsLockOn();
}

CHPSView::~CHPSView()
{
	_canvas.Delete();
}

BOOL CHPSView::PreCreateWindow(CREATESTRUCT& cs)
{
	// Setup Window class to work with HPS rendering.  The REDRAW flags prevent
	// flickering when resizing, and OWNDC allocates a single device context to for this window.
	cs.lpszClass = AfxRegisterWndClass(CS_OWNDC|CS_DBLCLKS|CS_HREDRAW|CS_VREDRAW);
	return CView::PreCreateWindow(cs);
}

static bool has_called_initial_update = false;



void CHPSView::OnInitialUpdate()
{
	has_called_initial_update = true;

	// Only perform HPS::Canvas initialization once since we're reusing the same MFC Window each time.
	if (_canvas.Type() == HPS::Type::None)
	{
		// Setup to use the DX11 driver
		HPS::ApplicationWindowOptionsKit		windowOpts;
		windowOpts.SetDriver(HPS::Window::Driver::Default3D);

		// Create our Sprockets Canvas with the specified driver
		_canvas = HPS::Factory::CreateCanvas(reinterpret_cast<HPS::WindowHandle>(m_hWnd), "mfc_sandbox_canvas", windowOpts);

		// Create a new Sprockets View and attach it to our Sprockets.Canvas
		HPS::View view = HPS::Factory::CreateView("mfc_sandbox_view");
		_canvas.AttachViewAsLayout(view);

		// Setup scene startup values
		SetupSceneDefaults();
	}
	
	_canvas.Update(HPS::Window::UpdateType::Refresh);

	HPS::Rendering::Mode mode = GetCanvas().GetFrontView().GetRenderingMode();
	UpdateRenderingMode(mode);

	CHPSFrame * pFrame = (CHPSFrame*)AfxGetMainWnd();
	CMFCRibbonSlider  *pSlider = (CMFCRibbonSlider *)pFrame->m_wndRibbonBar.FindByID(ID_SLIDER2);


}

void CHPSView::OnPaint()
{
	// Update our HPS Canvas.  A refresh is needed as the window size may have changed.
	if (has_called_initial_update)
		_canvas.Update(HPS::Window::UpdateType::Refresh);

	// Invoke BeginPaint/EndPaint.  This must be called when providing OnPaint handler.
	CPaintDC dc(this);
}

void CHPSView::SetMainDistantLight(HPS::Vector const & lightDirection)
{
	HPS::DistantLightKit	light;
	light.SetDirection(lightDirection);
	light.SetCameraRelative(true);
	SetMainDistantLight(light);
}

void CHPSView::SetMainDistantLight(HPS::DistantLightKit const & light)
{
	// Delete previous light before inserting new one
	if (_mainDistantLight.Type() != HPS::Type::None)
		_mainDistantLight.Delete();
	_mainDistantLight = GetCanvas().GetFrontView().GetSegmentKey().InsertDistantLight(light);
}

void CHPSView::SetupSceneDefaults()
{
	// Attach CHPSDoc created model
	GetCanvas().GetFrontView().AttachModel(GetDocument()->GetModel());

	// Set default operators.
	SetupDefaultOperators();

	// Subscribe _errorHandler to handle errors
	HPS::Database::GetEventDispatcher().Subscribe(_errorHandler, HPS::Object::ClassID<HPS::ErrorEvent>());

	// Subscribe _warningHandler to handle warnings
	HPS::Database::GetEventDispatcher().Subscribe(_warningHandler, HPS::Object::ClassID<HPS::WarningEvent>());
}

void CHPSView::SetupDefaultOperators()
{
	// Orbit is on top and will be replaced when the operator is changed
	GetCanvas().GetFrontView().GetOperatorControl()
		.Push(new HPS::MouseWheelOperator(), Operator::Priority::Low)
		.Push(new HPS::ZoomOperator(MouseButtons::ButtonMiddle()))
		.Push(new HPS::PanOperator(MouseButtons::ButtonRight()))
		.Push(new HPS::OrbitOperator(MouseButtons::ButtonLeft()));
}

void CHPSView::ActivateCapture(HPS::Capture & capture)
{
	HPS::View newView = capture.Activate();
	auto newViewSegment = newView.GetSegmentKey();
	HPS::CameraKit newCamera;
	newViewSegment.ShowCamera(newCamera);

	newCamera.UnsetNearLimit();
	auto defaultCameraWithoutNearLimit = HPS::CameraKit::GetDefault().UnsetNearLimit();
	if (newCamera == defaultCameraWithoutNearLimit)
	{
		HPS::View oldView = GetCanvas().GetFrontView();
		HPS::CameraKit oldCamera;
		oldView.GetSegmentKey().ShowCamera(oldCamera);

		newViewSegment.SetCamera(oldCamera);
		newView.FitWorld();
	}

	AttachViewWithSmoothTransition(newView);
}

void CHPSView::AttachView(HPS::View & newView)
{
	HPS::CADModel cadModel = GetCHPSDoc()->GetCADModel();
	if (!cadModel.Empty())
	{
		cadModel.ResetVisibility(_canvas);
		_canvas.GetWindowKey().GetHighlightControl().UnhighlightEverything();
	}

	_preZoomToKeyPathCamera.Reset();
	_zoomToKeyPath.Reset();

	HPS::View oldView = GetCanvas().GetFrontView();

	GetCanvas().AttachViewAsLayout(newView);

	HPS::OperatorPtrArray operators;
	auto oldViewOperatorCtrl = oldView.GetOperatorControl();
	auto newViewOperatorCtrl = newView.GetOperatorControl();
	oldViewOperatorCtrl.Show(HPS::Operator::Priority::Low, operators);
	newViewOperatorCtrl.Set(operators, HPS::Operator::Priority::Low);
	oldViewOperatorCtrl.Show(HPS::Operator::Priority::Default, operators);
	newViewOperatorCtrl.Set(operators, HPS::Operator::Priority::Default);
	oldViewOperatorCtrl.Show(HPS::Operator::Priority::High, operators);
	newViewOperatorCtrl.Set(operators, HPS::Operator::Priority::High);

	SetMainDistantLight();

	oldView.Delete();
}

void CHPSView::AttachViewWithSmoothTransition(HPS::View & newView)
{
	HPS::View oldView = GetCanvas().GetFrontView();
	HPS::CameraKit oldCamera;
	oldView.GetSegmentKey().ShowCamera(oldCamera);

	auto newViewSegment = newView.GetSegmentKey();
	HPS::CameraKit newCamera;
	newViewSegment.ShowCamera(newCamera);

	AttachView(newView);

	newViewSegment.SetCamera(oldCamera);
	newView.SmoothTransition(newCamera);
}

void CHPSView::Update()
{
	_canvas.Update();
}

void CHPSView::Unhighlight()
{
	HPS::HighlightOptionsKit highlightOptions;
	highlightOptions.SetStyleName("highlight_style").SetNotification(true);

	_canvas.GetWindowKey().GetHighlightControl().Unhighlight(highlightOptions);
	HPS::Database::GetEventDispatcher().InjectEvent(HPS::HighlightEvent(HPS::HighlightEvent::Action::Unhighlight, HPS::SelectionResults(), highlightOptions));
	HPS::Database::GetEventDispatcher().InjectEvent(HPS::ComponentHighlightEvent(HPS::ComponentHighlightEvent::Action::Unhighlight, GetCanvas(), 0, HPS::ComponentPath(), highlightOptions));
}

void CHPSView::UnhighlightAndUpdate()
{
	Unhighlight();
	Update();
}

void CHPSView::ZoomToKeyPath(HPS::KeyPath const & keyPath)
{
	HPS::BoundingKit bounding;
	if (keyPath.ShowNetBounding(bounding))
	{
		_zoomToKeyPath = keyPath;

		HPS::View frontView = _canvas.GetFrontView();
		frontView.GetSegmentKey().ShowCamera(_preZoomToKeyPathCamera);

		HPS::CameraKit fittedCamera;
		frontView.ComputeFitWorldCamera(bounding, fittedCamera);
		frontView.SmoothTransition(fittedCamera);
	}
}

void CHPSView::RestoreCamera()
{
	if (!_preZoomToKeyPathCamera.Empty())
	{
		_canvas.GetFrontView().SmoothTransition(_preZoomToKeyPathCamera);
		HPS::Database::Sleep(500);

		InvalidateZoomKeyPath();
		InvalidateSavedCamera();
	}
}

void CHPSView::InvalidateZoomKeyPath()
{
	_zoomToKeyPath.Reset();
}

void CHPSView::InvalidateSavedCamera()
{
	_preZoomToKeyPathCamera.Reset();
}

HPS::ModifierKeys CHPSView::MapModifierKeys(UINT flags)
{
	HPS::ModifierKeys	modifier;

	// Map shift and control modifiers to HPS::InputEvent modifiers
	if ((flags & MK_SHIFT) != 0)
	{
		if (GetAsyncKeyState(VK_LSHIFT) & 0x8000)
			modifier.LeftShift(true);
		else
			modifier.RightShift(true);
	}

	if ((flags & MK_CONTROL) != 0)
	{
		if (GetAsyncKeyState(VK_LCONTROL) & 0x8000)
			modifier.LeftControl(true);
		else
			modifier.RightControl(true);
	}

	if (_capsLockState)
		modifier.CapsLock(true);

	return modifier;
}

HPS::MouseEvent CHPSView::BuildMouseEvent(HPS::MouseEvent::Action action, HPS::MouseButtons buttons, CPoint cpoint, UINT flags, size_t click_count, float scalar)
{
	// Convert location to window space
	HPS::Point				p(static_cast<float>(cpoint.x), static_cast<float>(cpoint.y), 0);
	_canvas.GetWindowKey().ConvertCoordinate(HPS::Coordinate::Space::Pixel, p, HPS::Coordinate::Space::Window, p);

	if(action == HPS::MouseEvent::Action::Scroll)
		return HPS::MouseEvent(action, scalar, p, MapModifierKeys(flags), click_count); 
	else
		return HPS::MouseEvent(action, p, buttons, MapModifierKeys(flags), click_count);
}

HPS::KeyboardEvent CHPSView::BuildKeyboardEvent(HPS::KeyboardEvent::Action action, UINT button)
{
	KeyboardCodeArray codes;
	ModifierKeys modifiers;

	if (GetAsyncKeyState(VK_LCONTROL) & 0x8000)
		modifiers.LeftControl(true);
	if (GetAsyncKeyState(VK_RCONTROL) & 0x8000)
		modifiers.RightControl(true);
	if (GetAsyncKeyState(VK_LSHIFT) & 0x8000)
		modifiers.LeftShift(true);
	if (GetAsyncKeyState(VK_RSHIFT) & 0x8000)
		modifiers.RightShift(true);
	if ((GetKeyState(VK_CAPITAL) & 0x0001) != 0)
		modifiers.CapsLock(true);
	else if (GetKeyState(VK_CAPITAL))
		_capsLockState = false;

	codes.push_back(KeyboardCode(button));
	return HPS::KeyboardEvent(action, codes, modifiers);
}

void CHPSView::OnLButtonDown(UINT nFlags, CPoint point)
{
	SetCapture();
	_canvas.GetWindowKey().GetEventDispatcher().InjectEvent(
		BuildMouseEvent(HPS::MouseEvent::Action::ButtonDown, HPS::MouseButtons::ButtonLeft(), point, nFlags, 1));

	CView::OnLButtonDown(nFlags, point);
}

void CHPSView::OnLButtonUp(UINT nFlags, CPoint point)
{
	_canvas.GetWindowKey().GetEventDispatcher().InjectEvent(
		BuildMouseEvent(HPS::MouseEvent::Action::ButtonUp, HPS::MouseButtons::ButtonLeft(), point, nFlags, 0));

	ReleaseCapture();
	SetCursor(theApp.LoadStandardCursor(IDC_ARROW));

	CView::OnLButtonUp(nFlags, point);
}

void CHPSView::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	_canvas.GetWindowKey().GetEventDispatcher().InjectEvent(
		BuildMouseEvent(HPS::MouseEvent::Action::ButtonDown, HPS::MouseButtons::ButtonLeft(), point, nFlags, 2));

	ReleaseCapture();

	CView::OnLButtonUp(nFlags, point);
}

void CHPSView::OnMouseMove(UINT nFlags, CPoint point)
{
	CRect wndRect;
	GetWindowRect(&wndRect);
	ScreenToClient(&wndRect);

	if(wndRect.PtInRect(point) || (nFlags & MK_LBUTTON) || (nFlags & MK_RBUTTON))
	{
		_canvas.GetWindowKey().GetEventDispatcher().InjectEvent(
			BuildMouseEvent(HPS::MouseEvent::Action::Move, HPS::MouseButtons(), point, nFlags, 0));
	}
	else
	{
		if(!(nFlags & MK_LBUTTON) && GetCapture() != NULL)
			OnLButtonUp(nFlags, point);
		if(!(nFlags & MK_RBUTTON) && GetCapture() != NULL)
			OnRButtonUp(nFlags, point);
	}

	CView::OnMouseMove(nFlags, point);
}

void CHPSView::OnRButtonDown(UINT nFlags, CPoint point)
{
	_canvas.GetWindowKey().GetEventDispatcher().InjectEvent(
		BuildMouseEvent(HPS::MouseEvent::Action::ButtonDown, HPS::MouseButtons::ButtonRight(), point, nFlags, 1));

	CView::OnRButtonDown(nFlags, point);
}

void CHPSView::OnRButtonUp(UINT nFlags, CPoint point)
{
	_canvas.GetWindowKey().GetEventDispatcher().InjectEvent(
		BuildMouseEvent(HPS::MouseEvent::Action::ButtonUp, HPS::MouseButtons::ButtonRight(), point, nFlags, 0));

	SetCursor(theApp.LoadStandardCursor(IDC_ARROW));

	CView::OnRButtonUp(nFlags, point);
}

void CHPSView::OnRButtonDblClk(UINT nFlags, CPoint point)
{
	_canvas.GetWindowKey().GetEventDispatcher().InjectEvent(
		BuildMouseEvent(HPS::MouseEvent::Action::ButtonDown, HPS::MouseButtons::ButtonRight(), point, nFlags, 2));

	ReleaseCapture();

	CView::OnLButtonUp(nFlags, point);
}

void CHPSView::OnMButtonDown(UINT nFlags, CPoint point)
{
	_canvas.GetWindowKey().GetEventDispatcher().InjectEvent(
		BuildMouseEvent(HPS::MouseEvent::Action::ButtonDown, HPS::MouseButtons::ButtonMiddle(), point, nFlags, 1));

	CView::OnMButtonDown(nFlags, point);
}

void CHPSView::OnMButtonUp(UINT nFlags, CPoint point)
{
	_canvas.GetWindowKey().GetEventDispatcher().InjectEvent(
		BuildMouseEvent(HPS::MouseEvent::Action::ButtonUp, HPS::MouseButtons::ButtonMiddle(), point, nFlags, 0));

	SetCursor(theApp.LoadStandardCursor(IDC_ARROW));

	CView::OnMButtonUp(nFlags, point);
}

void CHPSView::OnMButtonDblClk(UINT nFlags, CPoint point)
{
	_canvas.GetWindowKey().GetEventDispatcher().InjectEvent(
		BuildMouseEvent(HPS::MouseEvent::Action::ButtonDown, HPS::MouseButtons::ButtonMiddle(), point, nFlags, 2));

	ReleaseCapture();

	CView::OnLButtonUp(nFlags, point);
}

BOOL CHPSView::OnMouseWheel(UINT nFlags, short zDelta, CPoint point)
{
	HPS::WindowHandle hwnd;
	static_cast<ApplicationWindowKey>(_canvas.GetWindowKey()).GetWindowOptionsControl().ShowWindowHandle(hwnd);
	::ScreenToClient(reinterpret_cast<HWND>(hwnd), &point);
	_canvas.GetWindowKey().GetEventDispatcher().InjectEvent(
		BuildMouseEvent(HPS::MouseEvent::Action::Scroll, HPS::MouseButtons(), point, nFlags, 0, static_cast<float>(zDelta)));

	return CView::OnMouseWheel(nFlags, zDelta, point);
}

void CHPSView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	_canvas.GetWindowKey().GetEventDispatcher().InjectEvent(
		BuildKeyboardEvent(HPS::KeyboardEvent::Action::KeyDown, nChar));

	CView::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CHPSView::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	_canvas.GetWindowKey().GetEventDispatcher().InjectEvent(
		BuildKeyboardEvent(HPS::KeyboardEvent::Action::KeyUp, nChar));

	CView::OnKeyUp(nChar, nRepCnt, nFlags);
}

void CHPSView::UpdateOperatorStates(SandboxOperators op)
{
	for (auto it = _operatorStates.begin(), e = _operatorStates.end(); it != e; ++it)
		(*it).second = false;

	_operatorStates[op] = true;
}

void CHPSView::OnOperatorsOrbit()
{
	GetCanvas().GetFrontView().GetOperatorControl().Pop();
	GetCanvas().GetFrontView().GetOperatorControl().Push(new HPS::OrbitOperator(MouseButtons::ButtonLeft()));
	UpdateOperatorStates(SandboxOperators::OrbitOperator);
}


void CHPSView::OnOperatorsPan()
{
	GetCanvas().GetFrontView().GetOperatorControl().Pop();
	GetCanvas().GetFrontView().GetOperatorControl().Push(new HPS::PanOperator(MouseButtons::ButtonLeft()));
	UpdateOperatorStates(SandboxOperators::PanOperator);
}


void CHPSView::OnOperatorsZoomArea()
{
	GetCanvas().GetFrontView().GetOperatorControl().Pop();
	GetCanvas().GetFrontView().GetOperatorControl().Push(new HPS::ZoomBoxOperator(MouseButtons::ButtonLeft()));
	UpdateOperatorStates(SandboxOperators::ZoomAreaOperator);
}


void CHPSView::OnOperatorsFly()
{
	//GetCanvas().GetFrontView().GetOperatorControl().Pop();
	//GetCanvas().GetFrontView().GetOperatorControl().Push(new HPS::FlyOperator());
	//UpdateOperatorStates(SandboxOperators::FlyOperator);

	GetCanvas().GetFrontView().GetOperatorControl().Pop();
	HPS::SimpleWalkOperator * pWalk = new HPS::SimpleWalkOperator();
	//HPS::WalkOperator * pWalk = new HPS::WalkOperator();

	float height = 0.0;


	IFCSampleModel *pIFCModel = GetCHPSDoc()->GetIFCSampleModel();
	if( pIFCModel->GetCurrentStorey() >= 0 )
		 pIFCModel->GetElevation( pIFCModel->GetCurrentStorey(), height );

	HPS::Plane groundPlane;

	groundPlane.a = 0;
	groundPlane.b = 0;
	groundPlane.c = -1;
	groundPlane.d = -height;

	pWalk->SetGroundPlane(groundPlane);
	pWalk->SetWalkerHeight(1.5);

	GetCanvas().GetFrontView().GetOperatorControl().Push(pWalk);


	UpdateOperatorStates(SandboxOperators::FlyOperator);
}

void CHPSView::OnOperatorsHome()
{
	CHPSDoc * doc = GetCHPSDoc();
	HPS::CADModel cadModel = doc->GetCADModel();
	try
	{
		if (!cadModel.Empty())
			AttachViewWithSmoothTransition(cadModel.ActivateDefaultCapture().FitWorld());
		else
			_canvas.GetFrontView().SmoothTransition(doc->GetDefaultCamera());
	}
	catch(HPS::InvalidSpecificationException const &)
	{
		//SmoothTransition() can throw if there is no model attached
	}
}

void CHPSView::OnOperatorsZoomFit()
{
	HPS::View frontView = GetCanvas().GetFrontView();
	HPS::CameraKit zoomFitCamera;
	if (!_zoomToKeyPath.Empty())
	{
		HPS::BoundingKit bounding;
		_zoomToKeyPath.ShowNetBounding(bounding);
		frontView.ComputeFitWorldCamera(bounding, zoomFitCamera);
	}
	else
		frontView.ComputeFitWorldCamera(zoomFitCamera);

	frontView.SmoothTransition(zoomFitCamera);
}


void CHPSView::OnOperatorsSelectPoint()
{
	GetCanvas().GetFrontView().GetOperatorControl().Pop();

	SandboxHighlightOperator *pSandBoxOp = new SandboxHighlightOperator(this);

	HPS::SelectionOptionsKit sKit = pSandBoxOp->GetSelectionOptions();
	sKit.SetLevel(HPS::Selection::Level::Subentity);
	sKit.SetAlgorithm(HPS::Selection::Algorithm::Visual);
	sKit.SetProximity(0.5);
	sKit.SetRelatedLimit(4);
	pSandBoxOp->SetSelectionOptions(sKit);

	/*HPS::HighlightOptionsKit hKit = pSandBoxOp->GetHighlightOptions();
	hKit.SetSubentityHighlighting(true);
	pOp->SetHighlightOptions(hKit);*/

	GetCanvas().GetFrontView().GetOperatorControl().Push(pSandBoxOp);

	HPS::SelectionOptionsKit sKit2 = pSandBoxOp->GetSelectionOptions();


	UpdateOperatorStates(SandboxOperators::PointOperator);
}


void CHPSView::OnOperatorsSelectArea()
{
	GetCanvas().GetFrontView().GetOperatorControl().Pop();
	GetCanvas().GetFrontView().GetOperatorControl().Push(new HPS::HighlightAreaOperator(MouseButtons::ButtonLeft()));
	UpdateOperatorStates(SandboxOperators::AreaOperator);
}


void CHPSView::OnModesSimpleShadow()
{
	// Toggle state and bail early if we're disabling
	_enableSimpleShadows = !_enableSimpleShadows;
	if (_enableSimpleShadows == false)
	{
		GetCanvas().GetFrontView().GetSegmentKey().GetVisualEffectsControl().SetSimpleShadow(false);
		_canvas.Update();
		return;
	}

	UpdatePlanes();
}


void CHPSView::OnModesSmooth()
{
	if (!_smoothRendering)
	{
		_canvas.GetFrontView().SetRenderingMode(Rendering::Mode::Phong);

		if (GetCHPSDoc()->GetCADModel().Type() == HPS::Type::DWGCADModel)
			_canvas.GetFrontView().GetSegmentKey().GetVisibilityControl().SetLines(true);


		_canvas.Update();
		_smoothRendering = true;
	}
}


void CHPSView::OnModesHiddenLine()
{
	if (_smoothRendering)
	{
		_canvas.GetFrontView().SetRenderingMode(Rendering::Mode::FastHiddenLine);
		_canvas.Update();
		_smoothRendering = false;

		//fixed framerate is not compatible with hidden line mode
		if (_enableFrameRate)
		{
			_canvas.SetFrameRate(0);
			_enableFrameRate = false;
		}
	}
}

void CHPSView::OnModesFrameRate()
{
	const float					frameRate = 20.0f;

	// Toggle frame rate and set.  Note that 0 disables frame rate.
	_enableFrameRate = !_enableFrameRate;
	if (_enableFrameRate)
		_canvas.SetFrameRate(frameRate);
	else
		_canvas.SetFrameRate(0);

	//fixed framerate is not compatible with hidden line mode
	if(!_smoothRendering)
	{
		_canvas.GetFrontView().SetRenderingMode(Rendering::Mode::Phong);
		_smoothRendering = true;
	}

	_canvas.Update();
}



void CHPSView::OnEditCopy()
{
	HPS::Hardcopy::GDI::ExportOptionsKit exportOptionsKit;
	HPS::Hardcopy::GDI::ExportClipboard(GetCanvas().GetWindowKey(), exportOptionsKit);
}


void CHPSView::OnFileOpen()
{
	CString filter = _T("HOOPS Stream File (*.hsf)|*.hsf|StereoLithography File (*.stl)|*.stl|WaveFront File (*.obj)|*.obj|Point Cloud File (*.ptx, *.pts, *.xyz)|*.ptx;*.pts;*.xyz|");
#ifdef USING_EXCHANGE
	CString exchange_filter = 
		_T("All CAD Files (*.3ds, *.3dxml, *.sat, *.sab, *_pd, *.model, *.dlv, *.exp, *.session, *.CATPart, *.CATProduct, *.CATShape, *.CATDrawing")
		_T(", *.cgr, *.dae, *.prt, *.prt.*, *.neu, *.neu.*, *.asm, *.asm.*, *.xas, *.xpr, *.arc, *.unv, *.mf1, *.prt, *.pkg, *.ifc, *.ifczip, *.igs, *.iges, *.ipt, *.iam")
		_T(", *.jt, *.kmz, *.prt, *.pdf, *.prc, *.x_t, *.xmt, *.x_b, *.xmt_txt, *.3dm, *.stp, *.step, *.stpz, *.stp.z, *.stl, *.par, *.asm, *.pwd, *.psm")
		_T(", *.sldprt, *.sldasm, *.sldfpp, *.asm, *.u3d, *.vda, *.wrl, *.vml, *.obj, *.xv3, *.xv0)|")
		_T("*.3ds;*.3dxml;*.sat;*.sab;*_pd;*.model;*.dlv;*.exp;*.session;*.catpart;*.catproduct;*.catshape;*.catdrawing")
		_T(";*.cgr;*.dae;*.prt;*.prt.*;*.neu;*.neu.*;*.asm;*.asm.*;*.xas;*.xpr;*.arc;*.unv;*.mf1;*.prt;*.pkg;*.ifc;*.ifczip;*.igs;*.iges;*.ipt;*.iam")
		_T(";*.jt;*.kmz;*.prt;*.pdf;*.prc;*.x_t;*.xmt;*.x_b;*.xmt_txt;*.3dm;*.stp;*.step;*.stpz;*.stp.z;*.stl;*.par;*.asm;*.pwd;*.psm")
		_T(";*.sldprt;*.sldasm;*.sldfpp;*.asm;*.u3d;*.vda;*.wrl;*.vml;*.obj;*.xv3;*.xv0;*.hsf|")
		_T("3D Studio Files (*.3ds)|*.3ds|")
		_T("3DXML Files (*.3dxml)|*.3dxml|")
		_T("ACIS SAT Files (*.sat, *.sab)|*.sat;*.sab|")
		_T("CADDS Files (*_pd)|*_pd|")
		_T("CATIA V4 Files (*.model, *.dlv, *.exp, *.session)|*.model;*.dlv;*.exp;*.session|")
		_T("CATIA V5 Files (*.CATPart, *.CATProduct, *.CATShape, *.CATDrawing)|*.catpart;*.catproduct;*.catshape;*.catdrawing|")
		_T("CGR Files (*.cgr)|*.cgr|")
		_T("Collada Files (*.dae)|*.dae|")
		_T("Creo (ProE) Files (*.prt, *.prt.*, *.neu, *.neu.*, *.asm, *.asm.*, *.xas, *.xpr)|*.prt;*.prt.*;*.neu;*.neu.*;*.asm;*.asm.*;*.xas;*.xpr|")
		_T("I-DEAS Files (*.arc, *.unv, *.mf1, *.prt, *.pkg)|*.arc;*.unv;*.mf1;*.prt;*.pkg|")
		_T("IFC Files (*.ifc, *.ifczip)|*.ifc;*.ifczip|")
		_T("IGES Files (*.igs, *.iges)|*.igs;*.iges|")
		_T("Inventor Files (*.ipt, *.iam)|*.ipt;*.iam|")
		_T("JT Files (*.jt)|*.jt|")
		_T("KMZ Files (*.kmz)|*.kmz|")
		_T("NX (Unigraphics) Files (*.prt)|*.prt|")
		_T("PDF Files (*.pdf)|*.pdf|")
		_T("PRC Files (*.prc)|*.prc|")
		_T("Parasolid Files (*.x_t, *.xmt, *.x_b, *.xmt_txt)|*.x_t;*.xmt;*.x_b;*.xmt_txt|")
		_T("Rhino Files (*.3dm)|*.3dm|")
		_T("STEP Files (*.stp, *.step, *.stpz, *.stp.z)|*.stp;*.step;*.stpz;*.stp.z|")
		_T("STL Files (*.stl)|*.stl|")
		_T("SolidEdge Files (*.par, *.asm, *.pwd, *.psm)|*.par;*.asm;*.pwd;*.psm|")
		_T("SolidWorks Files (*.sldprt, *.sldasm, *.sldfpp, *.asm)|*.sldprt;*.sldasm;*.sldfpp;*.asm|")
		_T("Universal 3D Files (*.u3d)|*.u3d|")
		_T("VDA Files (*.vda)|*.vda|")
		_T("VRML Files (*.wrl, *.vrml)|*.wrl;*.vrml|")
		_T("XVL Files (*.xv3, *.xv0)|*.xv0;*.xv3|");

	filter.Append(exchange_filter);
#endif

#if defined(USING_PARASOLID)
	CString parasolid_filter = _T("Parasolid File (*.x_t, *.x_b, *.xmt_txt, *.xmt_bin)|*.x_t;*.x_b;*.xmt_txt;*.xmt_bin|");
	filter.Append(parasolid_filter);
#endif

#if defined (USING_DWG)
	CString dwg_filter = _T("DWG File (*.dwg, *.dxf)|*.dwg;*.dxf|");
	filter.Append(dwg_filter);
#endif

	CString all_files = _T("All Files (*.*)|*.*||");
	filter.Append(all_files);
	CFileDialog dlg(TRUE, _T(".hsf"), NULL, OFN_HIDEREADONLY, filter, NULL);

	if (dlg.DoModal() == IDOK)
		GetDocument()->OnOpenDocument(dlg.GetPathName());
}

void CHPSView::OnFileSaveAs()
{
	CString filter = _T("HOOPS Stream File (*.hsf)|*.hsf|PDF (*.pdf)|*.pdf|Postscript File (*.ps)|*.ps|JPEG Image File(*.jpeg)|*.jpeg|PNG Image File (*.png)|*.png||");
	CFileDialog dlg(FALSE, _T(".hsf"), NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOREADONLYRETURN, filter, NULL);

	if (dlg.DoModal())
	{
		CString pathname;
		pathname = dlg.GetPathName();
		CStringA ansiPath(pathname);

		char ext[5];
		ext[0]= '\0';

		char const * tmp = ::strrchr(ansiPath, '.');
		if(!tmp)
		{
			strcpy_s(ext, ansiPath);
			return;
		}

		++tmp;
		size_t len = strlen(tmp);
		for (size_t i = 0; i <= len; ++i){
			ext[i] = static_cast<char>(tolower(tmp[i]));
		}

		if (strcmp(ext, "hsf") == 0)
		{
			HPS::Stream::ExportOptionsKit eok;
			HPS::SegmentKey exportFromHere;

			HPS::Model model = _canvas.GetFrontView().GetAttachedModel();
			if (model.Type() == HPS::Type::None)
				exportFromHere = _canvas.GetFrontView().GetSegmentKey();
			else
				exportFromHere = model.GetSegmentKey();

			HPS::Stream::ExportNotifier notifier;
			HPS::IOResult status = IOResult::Failure;
			try
			{
				notifier = HPS::Stream::File::Export(ansiPath, exportFromHere, eok); 
				CProgressDialog edlg(GetDocument(), notifier, true);
				edlg.DoModal();
				status = notifier.Status();
			}
			catch (HPS::IOException const & e)
			{
				char what[1024];
				sprintf_s(what, 1024, "HPS::Stream::File::Export threw an exception: %s", e.what());
				MessageBoxA(NULL, what, NULL, 0);
			}
			if (status != HPS::IOResult::Success && status != HPS::IOResult::Canceled)
				GetParentFrame()->MessageBox(L"HPS::Stream::File::Export encountered an error", _T("File export error"), MB_ICONERROR | MB_OK);

		}
		else if (strcmp(ext, "pdf") == 0)
		{
			bool export_2d_pdf = true;
#ifdef USING_PUBLISH
			PDFExportDialog pdf_export;
			if (pdf_export.DoModal() == IDOK)
				export_2d_pdf = pdf_export.Export2DPDF();
			else
				return;

			if (!export_2d_pdf)
			{
				try 
				{
					HPS::Publish::ExportOptionsKit export_kit;
					CADModel cad_model = GetDocument()->GetCADModel();
					if (cad_model.Type() == HPS::Type::ExchangeCADModel)
						HPS::Publish::File::ExportPDF(cad_model, ansiPath, export_kit);
					else
					{
						HPS::SprocketPath sprocket_path(GetCanvas(), GetCanvas().GetAttachedLayout(), GetCanvas().GetFrontView(), GetCanvas().GetFrontView().GetAttachedModel());
						HPS::Publish::File::ExportPDF(sprocket_path.GetKeyPath(), ansiPath, export_kit);
					}
				}
				catch (HPS::IOException const & e)
				{
					char what[1024];
					sprintf_s(what, 1024, "HPS::Publish::File::Export threw an exception: %s", e.what());
					MessageBoxA(NULL, what, NULL, 0);
				}
			}
#endif
			if (export_2d_pdf)
			{
				try 
				{
					HPS::Hardcopy::File::Export(ansiPath, HPS::Hardcopy::File::Driver::PDF,
								_canvas.GetWindowKey(), HPS::Hardcopy::File::ExportOptionsKit::GetDefault());
				}
				catch (HPS::IOException const & e)
				{
					char what[1024];
					sprintf_s(what, 1024, "HPS::Hardcopy::File::Export threw an exception: %s", e.what());
					MessageBoxA(NULL, what, NULL, 0);
				}
			}
		}
		else if (strcmp(ext, "ps") == 0)
		{
			try 
			{
				HPS::Hardcopy::File::Export(ansiPath, HPS::Hardcopy::File::Driver::Postscript,
						_canvas.GetWindowKey(), HPS::Hardcopy::File::ExportOptionsKit::GetDefault());
			}
			catch (HPS::IOException const & e)
			{
				char what[1024];
				sprintf_s(what, 1024, "HPS::Hardcopy::File::Export threw an exception: %s", e.what());
				MessageBoxA(NULL, what, NULL, 0);
			}
		}
		else if (strcmp(ext, "jpeg") == 0)
		{
			HPS::Image::ExportOptionsKit eok;
			eok.SetFormat(Image::Format::Jpeg);

			try { HPS::Image::File::Export(ansiPath, _canvas.GetWindowKey(), eok); }
			catch (HPS::IOException const & e)
			{
				char what[1024];
				sprintf_s(what, 1024, "HPS::Image::File::Export threw an exception: %s", e.what());
				MessageBoxA(NULL, what, NULL, 0);
			}
		}
		else if (strcmp(ext, "png") == 0)
		{
			try { HPS::Image::File::Export(ansiPath, _canvas.GetWindowKey(), HPS::Image::ExportOptionsKit::GetDefault()); }
			catch (HPS::IOException const & e)
			{
				char what[1024];
				sprintf_s(what, 1024, "HPS::Image::File::Export threw an exception: %s", e.what());
				MessageBoxA(NULL, what, NULL, 0);
			}
		}
	}
}

void CHPSView::UpdateRenderingMode(HPS::Rendering::Mode mode)
{
	if (mode == HPS::Rendering::Mode::Phong)
		_smoothRendering = true;
	else if (mode == HPS::Rendering::Mode::FastHiddenLine)
		_smoothRendering = false;
}

void CHPSView::UpdatePlanes()
{
	if(_enableSimpleShadows)
	{
		GetCanvas().GetFrontView().SetSimpleShadow(true);
	
		const float					opacity = 0.3f;
		const unsigned int			resolution = 512;
		const unsigned int			blurring = 20;

		HPS::SegmentKey viewSegment = GetCanvas().GetFrontView().GetSegmentKey();

		// Set opacity in simple shadow color
		HPS::RGBAColor color(0.4f, 0.4f, 0.4f, opacity);
		if (viewSegment.GetVisualEffectsControl().ShowSimpleShadowColor(color))
			color.alpha = opacity;

		viewSegment.GetVisualEffectsControl()
			.SetSimpleShadow(_enableSimpleShadows, resolution, blurring)
			.SetSimpleShadowColor(color);
		_canvas.Update();
	}
}

void CHPSView::OnUpdateRibbonBtnSimpleShadow(CCmdUI * pCmdUI)
{
	pCmdUI->SetCheck(_enableSimpleShadows);
}

void CHPSView::OnUpdateRibbonBtnFrameRate(CCmdUI * pCmdUI)
{
	pCmdUI->SetCheck(_enableFrameRate);
}

void CHPSView::OnUpdateRibbonBtnSmooth(CCmdUI * pCmdUI)
{
	pCmdUI->SetCheck(_smoothRendering);
}

void CHPSView::OnUpdateRibbonBtnHiddenLine(CCmdUI * pCmdUI)
{
	pCmdUI->SetCheck(!_smoothRendering);
}

void CHPSView::OnUpdateRibbonBtnOrbitOp(CCmdUI * pCmdUI)
{
	pCmdUI->SetCheck(_operatorStates[SandboxOperators::OrbitOperator]);
}

void CHPSView::OnUpdateRibbonBtnPanOp(CCmdUI * pCmdUI)
{
	pCmdUI->SetCheck(_operatorStates[SandboxOperators::PanOperator]);
}

void CHPSView::OnUpdateRibbonBtnZoomAreaOp(CCmdUI * pCmdUI)
{
	pCmdUI->SetCheck(_operatorStates[SandboxOperators::ZoomAreaOperator]);
}

void CHPSView::OnUpdateRibbonBtnFlyOp(CCmdUI * pCmdUI)
{
	pCmdUI->SetCheck(_operatorStates[SandboxOperators::FlyOperator]);
}

void CHPSView::OnUpdateRibbonBtnPointOp(CCmdUI * pCmdUI)
{
	pCmdUI->SetCheck(_operatorStates[SandboxOperators::PointOperator]);
}

void CHPSView::OnUpdateRibbonBtnAreaOp(CCmdUI * pCmdUI)
{
	pCmdUI->SetCheck(_operatorStates[SandboxOperators::AreaOperator]);
}

void CHPSView::OnKillFocus(CWnd* /*pNewWnd*/)
{
	_canvas.GetWindowKey().GetEventDispatcher().InjectEvent(HPS::FocusLostEvent());
}

void CHPSView::OnSetFocus(CWnd* /*pOldWnd*/)
{
	_capsLockState = IsCapsLockOn();
}

void CHPSView::OnUserCode1()
{
	// Toggle display of resource monitor using the DebuggingControl
	_displayResourceMonitor = !_displayResourceMonitor;
	_canvas.GetWindowKey().GetDebuggingControl().SetResourceMonitor(_displayResourceMonitor);
	
}


void CHPSView::OnUserCode2()
{

	
}


void CHPSView::OnUserCode3()
{

	HPS::View  view = _canvas.GetFrontView();
	HPS::Model model = view.GetAttachedModel();
	HPS::SegmentKey modelKey = model.GetSegmentKey();
	HPS::CADModel cadModel = GetCHPSDoc()->GetCADModel();

	if (!cadModel.Empty())
	{
	   UTF8 unitsString = IFCUtils::GetUnitsString(cadModel);
	 
	   HPS::UTF8  utf8Name = cadModel.GetName();
	   CString cName(utf8Name.GetBytes());

	   MessageBox(CString("Units: ") + unitsString.GetBytes(), CString("Model File:") + cName );
	   //! [show_properties]
	   std::vector < std::pair<UTF8, UTF8>>  wsMetaData;
	   IFCUtils::GetComponentMetaData(cadModel, wsMetaData);

	   UTF8 allProps;

	   for (auto const &wsd : wsMetaData)
	   {
		   allProps += wsd.first + ":" + wsd.second  + "\n";	   
	   }
	   MessageBox(CString( allProps.GetBytes()), CString("Model File") + cName);
	   //! [show_properties]
	}


}

void CHPSView::OnUserCode4()
{

}




IFCSamplePreview * CHPSView::GetPreviewWindow()
{
	return &gblPreview;
}


void ifcPopulateFloorsDropDown(CMFCRibbonComboBox *pCombo, IFCSampleModel *pIFCModel )
{
	
	// The first entry will be to show the entire model

	pCombo->AddItem(CString("All"),NULL);
	pCombo->SelectItem(0);

	// now add an entry for each
	int index = 0;
	for (auto path : pIFCModel->_storeyPaths)
	{
		//lets get the name of the component
		float elevation = pIFCModel->_storeyElevations[index];

		Component front = path.Front();
		

		HPS::UTF8 strFrontName = front.GetName();
		CString csName(strFrontName.GetBytes());

		CString csValue;
		csValue.Format(_T("- %f"),  elevation);
			
		pCombo->AddItem(csName + csValue, index++);

	}
}

void CHPSView::OnLoadedIFCFile( CHPSDoc *pDoc, BOOL mergedFile )
{
	HPS::CADModel cadModel = pDoc->GetCADModel();

	if (cadModel.Empty())
		return;

	//! [on_loaded_ifc_file]

	HPS::Component::ComponentType cType = cadModel.GetComponentType();
	ComponentArray      ancestorcomponents;

	// only update the floors for the first model loaded

	IFCSampleModel * pIFCModel = pDoc->GetIFCSampleModel();
	if (mergedFile)
	{
		pIFCModel->IncrMergeMount();
		if (pIFCModel->GetMergeCount() > 1)
			return;
	}
	else
		pIFCModel->ResetMergeCount();

	pIFCModel->Reset();

	IFCUtils::FindIFCStoreys(cadModel, ancestorcomponents, pIFCModel );

	 IFCUtils::SortIFCStoreysByElevation(pIFCModel);

	//
	// now populate the dropdown
	//
	CHPSFrame * pFrame = (CHPSFrame*)AfxGetMainWnd();
	CMFCRibbonComboBox *pCombo = DYNAMIC_DOWNCAST(CMFCRibbonComboBox, pFrame->m_wndRibbonBar.FindByID(ID_COMBO_FLOOR));

	pCombo->RemoveAllItems();

	ifcPopulateFloorsDropDown(pCombo, pIFCModel);

	//! [on_loaded_ifc_file]

}

//! [on_combo_floor]
void CHPSView::OnComboFloor()
{
	CHPSFrame * pFrame = (CHPSFrame*)AfxGetMainWnd();
	CMFCRibbonComboBox *pCombo = DYNAMIC_DOWNCAST(CMFCRibbonComboBox, pFrame->m_wndRibbonBar.FindByID(ID_COMBO_FLOOR));

	int selectionIndex = pCombo->GetCurSel();

	IFCSampleModel *pIFCModel = GetCHPSDoc()->GetIFCSampleModel();
	int storeyIndex = selectionIndex - 1;
	pIFCModel->SetCurrentStorey(storeyIndex);


	if (storeyIndex < 0 )
	{
		HPS::CADModel cadModel = GetCHPSDoc()->GetCADModel();
		cadModel.ResetVisibility(_canvas);

		HPS::SegmentKey mainCutplaneKey = cadModel.GetModel().GetSegmentKey().Subsegment("Cutplane");
		mainCutplaneKey.Flush();
	}
	else
	{
		// show the selected floor
		//pIFCModel->_storeyPaths[pathIndex].Isolate(_canvas);

		float elevation, height;
		pIFCModel->GetElevationAndHeight(storeyIndex, elevation,height);

		if (height < 0.01)
			height = 0.5;

		HPS::CADModel cadModel = GetCHPSDoc()->GetCADModel();
		cadModel.ResetVisibility(_canvas);

		HPS::SegmentKey mainCutplaneKey = cadModel.GetModel().GetSegmentKey().Subsegment("Cutplane");
		mainCutplaneKey.Flush();

		HPS::CuttingSectionKit cutKit;

		HPS::Plane cutPlane;

		cutPlane.a = 0;
		cutPlane.b = 0;
		cutPlane.c = 1;
		cutPlane.d = -(elevation + height *.75);
		cutKit.SetPlanes(cutPlane);
		cutKit.SetVisualization(HPS::CuttingSection::Mode::None, RGBAColor(0.7f, 0.7f, 0.7f, 0.15f), 1);

		mainCutplaneKey.InsertCuttingSection(cutKit);

	}
	_canvas.Update();
}
//! [on_combo_floor]

void CHPSView::OnCheckCompProps()
{

	CHPSFrame * pFrame = GetCHPSDoc()->GetCHPSFrame();
	bool newState = !pFrame->GetComponentPropertiesPaneVisibility();

	pFrame->SetComponentPropertiesPaneVisibility(newState);
}


void CHPSView::OnUpdateCheckCompProps(CCmdUI *pCmdUI)
{
	CHPSFrame * frame = GetCHPSDoc()->GetCHPSFrame();
	pCmdUI->SetCheck(frame->GetComponentPropertiesPaneVisibility());

}


void CHPSView::OnChkComponentQuantities()
{
	CHPSFrame * pFrame = GetCHPSDoc()->GetCHPSFrame();
	bool newState = !pFrame->GetComponentQuantitiesDialogVisibility();

	pFrame->SetComponentQuantitiesDialogVisibility(newState);
}


void CHPSView::OnUpdateChkComponentQuantities(CCmdUI *pCmdUI)
{
	CHPSFrame * frame = GetCHPSDoc()->GetCHPSFrame();
	pCmdUI->SetCheck(frame->GetComponentQuantitiesDialogVisibility());
}


void CHPSView::OnCheckPlan()
{
	HPS::CADModel cadModel = GetCHPSDoc()->GetCADModel();
	if (cadModel.Empty())
		return;

	IFCSampleApp * pSampleApp = static_cast<CHPSApp *>(AfxGetApp())->GetSampleApp();
	bool newState = !pSampleApp->GetShowPlanView();

    pSampleApp->SetShowPlanView( newState );

	if (newState)
	{
		gblPlanView.CreatePlanView(GetCanvas(), cadModel);
	}
	else
	{
		gblPlanView.DestroyPlanView();
	}
 }


void CHPSView::OnUpdateCheckPlan(CCmdUI *pCmdUI)
{
	CHPSFrame * frame = GetCHPSDoc()->GetCHPSFrame();

	IFCSampleApp * pSampleApp = static_cast<CHPSApp *>(AfxGetApp())->GetSampleApp();

	pCmdUI->SetCheck(pSampleApp->GetShowPlanView());
}


void CHPSView::OnCheckPreview()
{
	HPS::CADModel cadModel = GetCHPSDoc()->GetCADModel();
	if (cadModel.Empty())
		return;

	IFCSampleApp * pSampleApp = static_cast<CHPSApp *>(AfxGetApp())->GetSampleApp();
	bool newState = !pSampleApp->GetShowPreviewView();

	pSampleApp->SetShowPreviewView(newState);

	if (newState)
	{
		gblPreview.CreatePreview(GetCanvas(), cadModel);
	}
	else
	{
		gblPreview.DestroyPreview();
	}
}


void CHPSView::OnUpdateCheckPreview(CCmdUI *pCmdUI)
{
	CHPSFrame * frame = GetCHPSDoc()->GetCHPSFrame();

	IFCSampleApp * pSampleApp = static_cast<CHPSApp *>(AfxGetApp())->GetSampleApp();

	pCmdUI->SetCheck(pSampleApp->GetShowPreviewView());
}

void CHPSView::OnFileOpenAndMerge(HPS::ComponentPath mergePath)
{
	CString filter = _T("");
	CString exchange_filter =
		_T("All CAD Files (*.3ds, *.3dxml, *.sat, *.sab, *_pd, *.model, *.dlv, *.exp, *.session, *.CATPart, *.CATProduct, *.CATShape, *.CATDrawing")
		_T(", *.cgr, *.dae, *.prt, *.prt.*, *.neu, *.neu.*, *.asm, *.asm.*, *.xas, *.xpr, *.arc, *.unv, *.mf1, *.prt, *.pkg, *.ifc, *.ifczip, *.igs, *.iges, *.ipt, *.iam")
		_T(", *.jt, *.kmz, *.prt, *.pdf, *.prc, *.x_t, *.xmt, *.x_b, *.xmt_txt, *.3dm, *.stp, *.step, *.stpz, *.stp.z, *.stl, *.par, *.asm, *.pwd, *.psm")
		_T(", *.sldprt, *.sldasm, *.sldfpp, *.asm, *.u3d, *.vda, *.wrl, *.vml, *.obj, *.xv3, *.xv0)|")
		_T("*.3ds;*.3dxml;*.sat;*.sab;*_pd;*.model;*.dlv;*.exp;*.session;*.catpart;*.catproduct;*.catshape;*.catdrawing")
		_T(";*.cgr;*.dae;*.prt;*.prt.*;*.neu;*.neu.*;*.asm;*.asm.*;*.xas;*.xpr;*.arc;*.unv;*.mf1;*.prt;*.pkg;*.ifc;*.ifczip;*.igs;*.iges;*.ipt;*.iam")
		_T(";*.jt;*.kmz;*.prt;*.pdf;*.prc;*.x_t;*.xmt;*.x_b;*.xmt_txt;*.3dm;*.stp;*.step;*.stpz;*.stp.z;*.stl;*.par;*.asm;*.pwd;*.psm")
		_T(";*.sldprt;*.sldasm;*.sldfpp;*.asm;*.u3d;*.vda;*.wrl;*.vml;*.obj;*.xv3;*.xv0;*.hsf|")
		_T("3D Studio Files (*.3ds)|*.3ds|")
		_T("3DXML Files (*.3dxml)|*.3dxml|")
		_T("ACIS SAT Files (*.sat, *.sab)|*.sat;*.sab|")
		_T("CADDS Files (*_pd)|*_pd|")
		_T("CATIA V4 Files (*.model, *.dlv, *.exp, *.session)|*.model;*.dlv;*.exp;*.session|")
		_T("CATIA V5 Files (*.CATPart, *.CATProduct, *.CATShape, *.CATDrawing)|*.catpart;*.catproduct;*.catshape;*.catdrawing|")
		_T("CGR Files (*.cgr)|*.cgr|")
		_T("Collada Files (*.dae)|*.dae|")
		_T("Creo (ProE) Files (*.prt, *.prt.*, *.neu, *.neu.*, *.asm, *.asm.*, *.xas, *.xpr)|*.prt;*.prt.*;*.neu;*.neu.*;*.asm;*.asm.*;*.xas;*.xpr|")
		_T("I-DEAS Files (*.arc, *.unv, *.mf1, *.prt, *.pkg)|*.arc;*.unv;*.mf1;*.prt;*.pkg|")
		_T("IFC Files (*.ifc, *.ifczip)|*.ifc;*.ifczip|")
		_T("IGES Files (*.igs, *.iges)|*.igs;*.iges|")
		_T("Inventor Files (*.ipt, *.iam)|*.ipt;*.iam|")
		_T("JT Files (*.jt)|*.jt|")
		_T("KMZ Files (*.kmz)|*.kmz|")
		_T("NX (Unigraphics) Files (*.prt)|*.prt|")
		_T("PDF Files (*.pdf)|*.pdf|")
		_T("PRC Files (*.prc)|*.prc|")
		_T("Parasolid Files (*.x_t, *.xmt, *.x_b, *.xmt_txt)|*.x_t;*.xmt;*.x_b;*.xmt_txt|")
		_T("Rhino Files (*.3dm)|*.3dm|")
		_T("STEP Files (*.stp, *.step, *.stpz, *.stp.z)|*.stp;*.step;*.stpz;*.stp.z|")
		_T("STL Files (*.stl)|*.stl|")
		_T("SolidEdge Files (*.par, *.asm, *.pwd, *.psm)|*.par;*.asm;*.pwd;*.psm|")
		_T("SolidWorks Files (*.sldprt, *.sldasm, *.sldfpp, *.asm)|*.sldprt;*.sldasm;*.sldfpp;*.asm|")
		_T("Universal 3D Files (*.u3d)|*.u3d|")
		_T("VDA Files (*.vda)|*.vda|")
		_T("VRML Files (*.wrl, *.vrml)|*.wrl;*.vrml|")
		_T("XVL Files (*.xv3, *.xv0)|*.xv0;*.xv3|");

	filter.Append(exchange_filter);


	CString all_files = _T("All Files (*.*)|*.*||");
	filter.Append(all_files);
	CFileDialog dlg(TRUE, _T(".ifc"), NULL, OFN_HIDEREADONLY, filter, NULL);

	if (dlg.DoModal() == IDOK)
	{
		GetDocument()->OnMergeExchangeModel(dlg.GetPathName(), mergePath );
	}
}
