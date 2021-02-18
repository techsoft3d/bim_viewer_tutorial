#pragma once

class IFCSamplePlanView;

class IFCCameraEventHandler : public HPS::EventHandler
{
	IFCSamplePlanView * _pPlanView;
public:

	IFCCameraEventHandler() { _pPlanView = 0; }

	void SetPlanView(IFCSamplePlanView * pPlanView) { _pPlanView = pPlanView; }

	~IFCCameraEventHandler() { Shutdown(); }
	EventHandler::HandleResult Handle(HPS::Event const * pInEvent);

};


//
// Utility class to represent a plan view and associated functions
//
class IFCSamplePlanView
{
	HPS::Canvas   _canvas;
	HPS::CADModel _cadModel;
	HPS::SegmentKey _planviewKey;
	HPS::SegmentKey _cutplaneKey;
	HPS::SegmentKey _locationKey;
	HPS::SegmentKey _contentKey;


	IFCCameraEventHandler _cameraHandler;

	void UpdateCameraLocation( HPS::View sourceView );

public:
	IFCSamplePlanView();
	~IFCSamplePlanView();

	bool CreatePlanView( HPS::Canvas canvas, HPS::CADModel cadModel );
	void DestroyPlanView();
	void SetCutPlane(float height, bool removeCutplane = false);

	friend class IFCCameraEventHandler;
};

