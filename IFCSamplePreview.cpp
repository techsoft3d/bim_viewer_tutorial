#include "stdafx.h"
#include "IFCSamplePreview.h"
#include "IFCQuantity.h"


IFCSamplePreview::IFCSamplePreview()
{
}


IFCSamplePreview::~IFCSamplePreview()
{
}

bool IFCSamplePreview::CreatePreview(HPS::Canvas canvas, HPS::CADModel cadModel)
{
	_canvas = canvas;
	_cadModel = cadModel;

	// show a plan view window
	HPS::SegmentKey viewKey = _canvas.GetFrontView().GetSegmentKey();

	_previewKey = viewKey.Subsegment("previewView", true);

	HPS::SubwindowKit subwindowKit;
	subwindowKit.SetSubwindow(HPS::Rectangle(0.49f, 0.99f, -0.99f, -0.49), HPS::Subwindow::Type::Standard);
	subwindowKit.SetBorder(HPS::Subwindow::Border::InsetBold);
	subwindowKit.SetBackground(HPS::Subwindow::Background::SolidColor);

	_previewKey.SetSubwindow(subwindowKit);
	_previewKey.InsertDistantLight(HPS::Vector(0.5f, 0.5f, 0.5f));

	
	return true;
}

void IFCSamplePreview::DestroyPreview()
{
	_previewKey.Delete();
	_canvas.GetFrontView().Update();

}

void IFCSamplePreview::UpdatePreview(IFCQuantityRecord &qr )
{
	
	if (_previewKey.Empty())
		return;

	HPS::ComponentPath cPath = qr._componentGroup[0]; // lets get one instance of a component
	HPS::KeyPathArray keyPaths = cPath.GetKeyPaths();
	HPS::KeyPath keyPath = keyPaths[0];
	HPS::SegmentKey segKey(keyPath.Front());

	if (!_includedComponentKey.Empty())
		_includedComponentKey.Delete();

	_includedComponentKey = _previewKey.IncludeSegment(segKey); // add it to this sub- window

	//
	// now get a bound on the contents
	//
	HPS::KeyPath newkeyPath;
	newkeyPath.PushBack(segKey);
	newkeyPath.PushBack(_previewKey);
	newkeyPath.PushBack(_canvas.GetFrontView().GetSegmentKey());

	HPS::BoundingKit bound;
	newkeyPath.ShowNetBounding(bound);

	HPS::SimpleSphere boundingSphere;
	HPS::SimpleCuboid boundingCuboid;

	bound.ShowVolume(boundingSphere, boundingCuboid);

	float scale = 1.75;
	
	float delta = boundingSphere.radius  * scale;
	
	HPS::Point camCenter(boundingSphere.center);
	HPS::Point camPos(camCenter.x + 1, camCenter.y + 1, camCenter.z + 1);

	HPS::CameraKit camKit;
	camKit.SetTarget(camCenter);
	camKit.SetPosition(camPos);
	camKit.SetField(delta, delta);
	camKit.SetUpVector(HPS::Vector(0, 0, 1));
	camKit.SetProjection(HPS::Camera::Projection::Orthographic);
	_previewKey.SetCamera(camKit);

	_canvas.GetFrontView().Update();


}
