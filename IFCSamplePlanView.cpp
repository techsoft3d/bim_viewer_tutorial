#include "stdafx.h"
#include "hps.h"
#include "IFCSamplePlanView.h"



IFCSamplePlanView::IFCSamplePlanView()
{
}


IFCSamplePlanView::~IFCSamplePlanView()
{
}

bool IFCSamplePlanView::CreatePlanView(HPS::Canvas canvas, HPS::CADModel cadModel)
{
	_canvas = canvas;
	_cadModel = cadModel;


	// show a plan view window
	HPS::SegmentKey viewKey = _canvas.GetFrontView().GetSegmentKey();
	HPS::SegmentKey modelKey = cadModel.GetModel().GetSegmentKey();

	//! [subwindow]
	_planviewKey = viewKey.Subsegment("planView", true);
	_locationKey = _planviewKey.Subsegment("cameraLocation", true);
 
	 _contentKey = _planviewKey.Subsegment("content", true);
	_contentKey.IncludeSegment(modelKey);
 
	HPS::SubwindowKit subwindowKit;
	subwindowKit.SetSubwindow(HPS::Rectangle(-1, -0.5f, -1, -0.25), HPS::Subwindow::Type::Standard);
	subwindowKit.SetBorder(HPS::Subwindow::Border::None);
	subwindowKit.SetBackground(HPS::Subwindow::Background::Transparent);
	_planviewKey.SetSubwindow(subwindowKit);
	_planviewKey.InsertDistantLight(HPS::Vector(0.5f, 0.5f, 0.5f));
	//! [subwindow]

	//! [bounding]
	HPS::KeyPath keyPath;
	keyPath.PushBack(modelKey);
	HPS::BoundingKit bound;
	keyPath.ShowNetBounding(bound);
	HPS::SimpleSphere boundingSphere;
	HPS::SimpleCuboid boundingCuboid;
 
	bound.ShowVolume(boundingSphere, boundingCuboid);
 
	float cx = (boundingCuboid.min.x + boundingCuboid.max.x) / 2;
	float cy = (boundingCuboid.min.y + boundingCuboid.max.y) / 2;
	float cz = (boundingCuboid.min.z + boundingCuboid.max.z) / 2;
 
	float scale = 1.05;
	float dx = (boundingCuboid.max.x - boundingCuboid.min.x)  * scale;
	float dy = (boundingCuboid.max.y - boundingCuboid.min.y) *  scale;
	float dz = (boundingCuboid.max.z - boundingCuboid.min.z) *  scale;
 
	float delta = fmax(dx, dy); // we want a square 
 
	HPS::Point camCenter(cx, cy, cz);
	HPS::Point camPos(cx, cy, cz + 1);
 
	HPS::CameraKit camKit;
	camKit.SetTarget(camCenter);
	camKit.SetPosition(camPos);
	camKit.SetField(delta, delta);
	camKit.SetUpVector(HPS::Vector(0, 1, 0));
	camKit.SetProjection(HPS::Camera::Projection::Orthographic);
	_planviewKey.SetCamera(camKit);
	//! [bounding]


	_locationKey.InsertSphere(HPS::Point(0, 0, boundingCuboid.max.z), boundingSphere.radius / 10);
	_locationKey.InsertLine(HPS::Point(0, 0, boundingCuboid.max.z), HPS::Point(boundingSphere.radius / 5, 0, boundingCuboid.max.z));

	_locationKey.GetDrawingAttributeControl().SetOverlay(HPS::Drawing::Overlay::Default);
	_locationKey.GetMaterialMappingControl().SetFaceColor(HPS::RGBAColor(1, 1, 0)).SetLineColor(HPS::RGBAColor(1, 0, 0));
	_locationKey.GetVisibilityControl().SetLines(true);

	UpdateCameraLocation(_canvas.GetFrontView());

	_canvas.GetFrontView().Update();

	_cameraHandler.SetPlanView(this);
	HPS::EventDispatcher dispatcher = HPS::Database::GetEventDispatcher();
	dispatcher.Subscribe(_cameraHandler, HPS::Object::ClassID<HPS::CameraChangedEvent>());

	return true;
}

void IFCSamplePlanView::DestroyPlanView()
{
	_cameraHandler.SetPlanView(0);
	_planviewKey.Delete();
	_canvas.GetFrontView().Update();
}

void IFCSamplePlanView::UpdateCameraLocation(HPS::View sourceView)
{
	HPS::CameraKit mainCamera;

	sourceView.GetSegmentKey().ShowCamera(mainCamera);

	HPS::Point position, target;
	mainCamera.ShowPosition(position);
	mainCamera.ShowTarget(target);

	HPS::Vector dirVec = target - position;
	dirVec.Normalize();
	double radZ = atan2(dirVec[1], dirVec[0]);
	double degZ = radZ * (180.0 / 3.141592653589793238463);

	//TRACE("Camera Rotation %f\n", degZ);

	HPS::MatrixKit mKit0;
	mKit0.Rotate(0, 0,(float)degZ);
	mKit0.Translate(position.x, position.y, 0);

	_locationKey.GetModellingMatrixControl().Set(mKit0);

	//TRACE("Camera Pos %f, %f\n", position.x, position.y);
	//TRACE("Matrix Pos %f, %f, %f\n", mKit1.data[12], mKit1.data[13], mKit1.data[14]);

	_canvas.GetFrontView().Update();

	

}


//
// Local Cutplane
//
// Only needed if you want to set the plan view cutplane separately from the main 
// biew
void IFCSamplePlanView::SetCutPlane(float height, bool removeCutplane)
{

	if ( _planviewKey.Empty() )
		return;
	if ( !_cutplaneKey.Empty())
		_cutplaneKey.Delete();

	if (removeCutplane)
		return;

	HPS::CuttingSectionKit cutKit;
	HPS::Plane cutPlane;

	cutPlane.a = 0;
	cutPlane.b = 0;
	cutPlane.c = 1;
	cutPlane.d = -height;
	cutKit.SetPlanes(cutPlane);
	cutKit.SetVisualization(HPS::CuttingSection::Mode::None, HPS::RGBAColor(0.7f, 0.7f, 0.7f, 0.15f), 1);

	_cutplaneKey = _contentKey.Subsegment("cutplane");
	_cutplaneKey.InsertCuttingSection(cutKit);

	_contentKey.GetVisibilityControl().SetLines(true).SetCutFaces(true).SetCutEdges(true);
	_locationKey.GetVisibilityControl().SetCuttingSections(false);

}


HPS::EventHandler::HandleResult IFCCameraEventHandler::Handle(HPS::Event const * pInEvent)
{
	if (NULL == _pPlanView)
		return HandleResult::NotHandled;

	HPS::CameraChangedEvent * pCCEvent = (HPS::CameraChangedEvent *)pInEvent;
	_pPlanView->UpdateCameraLocation(pCCEvent->view);

	return HandleResult::NotHandled;
}
