#pragma once

//
// Sample class to extract cross section and view in HOOPS Visualize
//
class HExCrossSection
{
	A3DAsmModelFile         *_pModelFile;
	A3DPlanarSectionData    _sectionPlane;
	A3DRiSet             ** _sectionElementsArray;
	A3DUns32                _numSections;
	HPS::SegmentKey         _crossSectionSegment;
	bool					 _exchangeInitialized;
	double                   _unitsScale;

	bool CreateVisualizeProfileGeometryFromRISet(HPS::SegmentKey segKey , A3DRiSet *pRISet );
	bool CreateVisualizeProfileGeometryFromRI(HPS::SegmentKey segKey, A3DRiRepresentationItem * pRI);

	bool CreateVisualizeProfileGeometryFromBaseCurve(HPS::SegmentKey segKey, A3DCrvBase* pCurve, double dParamMIn, double dParamMax);
	bool CreateVisualizeProfileGeometryFromRICurve( HPS::SegmentKey segKey, A3DRiCurve * pItem);
	bool CreateVisualizeProfileGeometryFromLineCurve(HPS::SegmentKey segKey, A3DCrvLine* pCurve);
	bool CreateVisualizeProfileGeometryFromCurve(HPS::SegmentKey segKey, A3DCrvBase* pCurve);
	bool CreateVisualizeProfileGeometryFromCompositeCurve(HPS::SegmentKey segKey, A3DCrvComposite* pCurve);

	bool CreateVisualizeProfileGeometryFromTopoWireEdge(HPS::SegmentKey segKey, A3DTopoWireEdge* pWireEdge);


public:
	HExCrossSection();
	~HExCrossSection();

	bool Init();

	void SetSectionPlaneFromPlane(HPS::Plane &plane);
	void SetModelFileFromCADFile(HPS::CADModel &CADModel );
	void SetCrossSectionSegment(HPS::SegmentKey crossSectionSegment);

	bool ComputeCrossSection();

	bool CreateVisualizeGeometry();
};

