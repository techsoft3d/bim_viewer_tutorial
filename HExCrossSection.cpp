#define INITIALIZE_A3D_API


#include "A3DSDKIncludes.h"
#include "hps.h"
#include "sprk_exchange.h"
#include <vector>
#include "sprk.h"
#include <math.h>
#include "HExCrossSection.h"


static void stVectoriel(const A3DVector3dData* X, const A3DVector3dData* Y, A3DVector3dData* Z)
{
	Z->m_dX = X->m_dY*Y->m_dZ - X->m_dZ*Y->m_dY;
	Z->m_dY = X->m_dZ*Y->m_dX - X->m_dX*Y->m_dZ;
	Z->m_dZ = X->m_dX*Y->m_dY - X->m_dY*Y->m_dX;
}

static  bool stIsIdentity( A3DMiscGeneralTransformationData &sData )
{
	for (int i = 0; i < 16; i++)
	{
		switch (i) {
		case 0:
		case 5:
		case 10:
		case 15:
			if (sData.m_adCoeff[i] != 1.0)
				return false;
			break;
		default:
			if (sData.m_adCoeff[i] != 0.0)
				return false;
		}
	}
	return true;
}


static A3DStatus stGetTransformation(const A3DMiscTransformation* pTransfo3d, bool &isIdentity, float hpsMtx[] )
{
	if (pTransfo3d == NULL)
		return A3D_SUCCESS;

	isIdentity = true;

	A3DStatus iRet = A3D_SUCCESS;

	A3DEEntityType eType = kA3DTypeUnknown;
	A3DEntityGetType(pTransfo3d, &eType);

	switch (eType)
	{
	case kA3DTypeMiscCartesianTransformation:
	{
		A3DMiscCartesianTransformationData sData;
		A3D_INITIALIZE_DATA(A3DMiscCartesianTransformationData, sData);

		A3DMiscCartesianTransformationGet(pTransfo3d, &sData);

		double adMatrix[16];
		double dMirror = (sData.m_ucBehaviour & kA3DTransformationMirror) ? -1. : 1.;

		A3DVector3dData sZVector;
		memset(adMatrix, 0, 16 * sizeof(double));
		stVectoriel(&sData.m_sXVector, &sData.m_sYVector, &sZVector);

		adMatrix[12] = sData.m_sOrigin.m_dX;
		adMatrix[13] = sData.m_sOrigin.m_dY;
		adMatrix[14] = sData.m_sOrigin.m_dZ;

		adMatrix[0] = sData.m_sXVector.m_dX*sData.m_sScale.m_dX;
		adMatrix[1] = sData.m_sXVector.m_dY*sData.m_sScale.m_dX;
		adMatrix[2] = sData.m_sXVector.m_dZ*sData.m_sScale.m_dX;

		adMatrix[4] = sData.m_sYVector.m_dX*sData.m_sScale.m_dY;
		adMatrix[5] = sData.m_sYVector.m_dY*sData.m_sScale.m_dY;
		adMatrix[6] = sData.m_sYVector.m_dZ*sData.m_sScale.m_dY;

		adMatrix[8] = dMirror * sZVector.m_dX*sData.m_sScale.m_dZ;
		adMatrix[9] = dMirror * sZVector.m_dY*sData.m_sScale.m_dZ;
		adMatrix[10] = dMirror * sZVector.m_dZ*sData.m_sScale.m_dZ;

		adMatrix[15] = 1.;

		A3DMiscCartesianTransformationGet(NULL, &sData);
	}
	break;

	case kA3DTypeMiscGeneralTransformation:
	{
		A3DMiscGeneralTransformationData sData;
		A3D_INITIALIZE_DATA(A3DMiscGeneralTransformationData, sData);

		A3DMiscGeneralTransformationGet(pTransfo3d, &sData); {

			if ( !stIsIdentity(sData))
			{
				isIdentity = false;
				for (int i = 0; i < 16; i++)
					hpsMtx[i] = sData.m_adCoeff[i];
			}

		} A3DMiscGeneralTransformationGet(NULL, &sData);
	}
	break;
	default:
		break;
	}

	return iRet;
}
HExCrossSection::HExCrossSection()
{
	_pModelFile = 0;
	_numSections = 0;
	_unitsScale = 1;
	_exchangeInitialized = false;
}


HExCrossSection::~HExCrossSection()
{
}


bool HExCrossSection::Init()
{

	if (_exchangeInitialized)
		return true;

	if (!A3DSDKLoadLibrary(NULL))
		return false;

	_exchangeInitialized = true;
	return true;
}


bool HExCrossSection::CreateVisualizeProfileGeometryFromBaseCurve(HPS::SegmentKey segKey, A3DCrvBase* pCurve,	double dParamMin, double dParamMax)
{
	double scale = _unitsScale;
	auto EvaluateA3DPoint = [scale](A3DCrvLine* pCurve, double param)
	{
		A3DVector3dData dPoint;
		A3D_INITIALIZE_DATA(A3DVector3dData, dPoint);
		A3DCrvEvaluate(pCurve, param, 0, &dPoint);
		return HPS::Point((float)dPoint.m_dX*scale, (float)dPoint.m_dY*scale, (float)dPoint.m_dZ*scale);
	};

	const int nSegs = 12;
	const int nPts = nSegs + 1;
	double interval = ( dParamMax - dParamMin)/nSegs;
	HPS::Point Pts[nPts];

	double param = dParamMin;

	for (int i = 0; i < nSegs + 1; i++)
	{
		Pts[i] = EvaluateA3DPoint(pCurve, param);
		param += interval;
	}

	segKey.InsertLine(nPts, Pts);

	return true;
}

bool HExCrossSection::CreateVisualizeProfileGeometryFromLineCurve(HPS::SegmentKey segKey, A3DCrvLine* pCurve )
{
	A3DCrvLineData lineData;
	A3D_INITIALIZE_DATA(A3DCrvLineData, lineData);

	A3DCrvLineGet(pCurve, &lineData); {

		double scale = _unitsScale;
	
		auto EvaluateA3DPoint = [scale](A3DCrvLine* pCurve, double param)
			{
				A3DVector3dData dPoint;
				A3D_INITIALIZE_DATA(A3DVector3dData, dPoint);
				A3DCrvEvaluate(pCurve,param, 0, &dPoint);
				return HPS::Point((float)dPoint.m_dX*scale, (float)dPoint.m_dY*scale, (float)dPoint.m_dZ*scale);
			};

		double dMin = lineData.m_sParam.m_sInterval.m_dMin;
		double dMax = lineData.m_sParam.m_sInterval.m_dMax;

		HPS::Point p1 = EvaluateA3DPoint(pCurve, dMin);
		HPS::Point p2 = EvaluateA3DPoint(pCurve, dMax);

		segKey.InsertLine(p1,p2);

	}A3DCrvLineGet(0, &lineData);

	return true;
}

bool HExCrossSection::CreateVisualizeProfileGeometryFromCompositeCurve(HPS::SegmentKey segKey, A3DCrvComposite* pCurve)
{
	A3DCrvCompositeData ccData;
	A3D_INITIALIZE_DATA(A3DCrvCompositeData, ccData);

	A3DCrvCompositeGet(pCurve, &ccData); {
		for (A3DUns32 i = 0; i < ccData.m_uiSize; i++)
		{
			CreateVisualizeProfileGeometryFromCurve( segKey, ccData.m_ppCurves[i]);
		}
	}A3DCrvCompositeGet(0, &ccData);

	return true;
}

bool HExCrossSection::CreateVisualizeProfileGeometryFromCurve(HPS::SegmentKey segKey, A3DCrvBase* pCurve)
{
	A3DEEntityType  ecType;

	A3DEntityGetType(pCurve, &ecType);
	switch (ecType)
	{
	case kA3DTypeCrvCircle:
	case kA3DTypeCrvNurbs:
	case kA3DTypeCrvEllipse:
	case kA3DTypeCrvHyperbola:
		A3DIntervalData iData;
		A3D_INITIALIZE_DATA(A3DIntervalData, iData);
		A3DCrvGetInterval(pCurve, &iData);
		CreateVisualizeProfileGeometryFromBaseCurve(segKey, pCurve, iData.m_dMin, iData.m_dMax);
		break;
	case kA3DTypeCrvLine:
		CreateVisualizeProfileGeometryFromLineCurve(segKey, pCurve);
		break;
	case kA3DTypeCrvComposite:
		CreateVisualizeProfileGeometryFromCompositeCurve(segKey, pCurve);
		break;
	default:
		int i = 1;
	}

	return true;
}
bool HExCrossSection::CreateVisualizeProfileGeometryFromTopoWireEdge(HPS::SegmentKey segKey, A3DTopoWireEdge * pWireEdge)
{
	A3DTopoWireEdgeData wireEdgeData;
	A3D_INITIALIZE_DATA(A3DTopoWireEdgeData, wireEdgeData);

	A3DTopoWireEdgeGet(pWireEdge, &wireEdgeData); {
		if (wireEdgeData.m_bHasTrimDomain)
			int i = 1;
		CreateVisualizeProfileGeometryFromCurve(segKey, wireEdgeData.m_p3dCurve);
	}A3DTopoWireEdgeGet(0, &wireEdgeData);

	return true;
}

bool HExCrossSection::CreateVisualizeProfileGeometryFromRICurve(HPS::SegmentKey segKey, A3DRiCurve * pRiCurve)
{
	A3DRiCurveData riCurveData;

	A3DRiRepresentationItemData riData;
	A3D_INITIALIZE_DATA(A3DRiRepresentationItemData, riData);

	HPS::SegmentKey curveKey = segKey.Subsegment();

	A3DRiRepresentationItemGet(pRiCurve, &riData); {

		A3DRiCoordinateSystemData coordData;
		A3D_INITIALIZE_DATA(A3DRiCoordinateSystemData, coordData);

		A3DRiCoordinateSystemGet(riData.m_pCoordinateSystem, &coordData); {

			bool bIsIdentity = true;
			float hpsMtx[16];
			stGetTransformation( coordData.m_pTransformation, bIsIdentity, hpsMtx);

			if (!bIsIdentity)
			{
				curveKey.GetModellingMatrixControl().SetElements(16, hpsMtx);
				curveKey.GetMaterialMappingControl().SetLineColor(HPS::RGBAColor(1, 1, 0));
			}

		} A3DRiCoordinateSystemGet(0, &coordData);

	
	}A3DRiRepresentationItemGet(0, &riData);


	A3D_INITIALIZE_DATA(A3DRiCurveData, riCurveData);

	A3DRiCurveGet(pRiCurve, &riCurveData); {

		A3DTopoSingleWireBodyData wireBodyData;
		A3D_INITIALIZE_DATA(A3DTopoSingleWireBodyData, wireBodyData);

		A3DTopoBodyData bodyData;
		A3D_INITIALIZE_DATA(A3DTopoBodyData, bodyData);

		A3DTopoBodyGet(riCurveData.m_pBody, &bodyData); {

			A3DTopoContextData contextData;
			A3D_INITIALIZE_DATA(A3DTopoContextData, contextData);

			A3DTopoContextGet(bodyData.m_pContext, &contextData); {

				if (contextData.m_bHaveScale)
				{
					int i = 1;
				}


				A3DTopoSingleWireBodyGet(riCurveData.m_pBody, &wireBodyData); {

					CreateVisualizeProfileGeometryFromTopoWireEdge(curveKey, wireBodyData.m_pWireEdge);

				}A3DTopoSingleWireBodyGet(0, &wireBodyData);

			}A3DTopoContextGet(0, &contextData); 
		}A3DTopoBodyGet(0, &bodyData);

	}A3DRiCurveGet(0, &riCurveData);

	return true;
}



bool HExCrossSection::CreateVisualizeProfileGeometryFromRI(HPS::SegmentKey segKey, A3DRiRepresentationItem * pItem)
{
	bool status = true;

	A3DEEntityType  eType;

	A3DEntityGetType(pItem, &eType);

	switch (eType)
	{
	case kA3DTypeRiSet:
		status = CreateVisualizeProfileGeometryFromRISet(segKey, pItem);
		break;
	case kA3DTypeRiPointSet:
		break;
	case kA3DTypeRiDirection:
		break;
	case kA3DTypeRiCurve:
		CreateVisualizeProfileGeometryFromRICurve(segKey, pItem);
		break;
	case kA3DTypeRiCoordinateSystem:
		break;
	case kA3DTypeRiPlane:
		break;
	case kA3DTypeRiBrepModel:
		break;
	case kA3DTypeRiPolyBrepModel:
	default:
		status = false;
		break;
	}

	return status;
}

bool HExCrossSection::CreateVisualizeProfileGeometryFromRISet(HPS::SegmentKey segKey, A3DRiSet * pRISet)
{
	A3DRiSetData riSetData;
	bool status = true;

	A3D_INITIALIZE_DATA(A3DRiSetData, riSetData);
	A3DRiSetGet(pRISet, &riSetData);

	for (A3DUns32 uiSet = 0; uiSet < riSetData.m_uiRepItemsSize; uiSet++)
	{
		CreateVisualizeProfileGeometryFromRI(segKey, riSetData.m_ppRepItems[uiSet]);
	}

	A3DRiSetGet(0, &riSetData);


	return status;
}



void HExCrossSection::SetSectionPlaneFromPlane(HPS::Plane &plane)
{
	double a = plane.a;
	double b = plane.b;
	double c = plane.c;
	double d = -plane.d;
	double mag = std::sqrt( a* a + b*b + c*c );

	A3D_INITIALIZE_DATA(A3DPlanarSectionData, _sectionPlane);

	A3DVector3dData direction;
	A3D_INITIALIZE_DATA(A3DVector3dData, direction);
	direction.m_dX = a/mag;
	direction.m_dY = b/mag;
	direction.m_dZ = c/mag;
	_sectionPlane.m_sDirection = direction;

	// plane origin point
	A3DVector3dData origin;

	A3D_INITIALIZE_DATA(A3DVector3dData, origin);
	origin.m_dX = direction.m_dX*d;
	origin.m_dY = direction.m_dY*d;
	origin.m_dZ = direction.m_dZ*d;
	_sectionPlane.m_sOrigin = origin;

}

void HExCrossSection::SetModelFileFromCADFile(HPS::CADModel & cadModel)
{
	HPS::Exchange::CADModel ex_cad_model(cadModel);

	_pModelFile = ex_cad_model.GetExchangeEntity();

	A3DDouble  modelUnit = 1;
	//A3DStatus eStatus = A3DAsmModelFileGetUnit(_pModelFile, &modelUnit);

	//if ( A3D_SUCCESS == eStatus )
	//	_unitsScale = 1 / modelUnit;
}

void HExCrossSection::SetCrossSectionSegment(HPS::SegmentKey crossSectionSegment)
{
	_crossSectionSegment = crossSectionSegment;
}


bool HExCrossSection::ComputeCrossSection()
{


	A3DStatus stat = A3DComputePlanarSectionOnModelFile(_pModelFile, &_sectionPlane, &_numSections, &_sectionElementsArray);

	return (A3D_SUCCESS == stat );
}

bool HExCrossSection::CreateVisualizeGeometry()
{
	if ( 0 == _numSections )
		return false;

	if (_crossSectionSegment.Empty())
		return false;

	for (A3DUns32 iSect = 0; iSect < _numSections; iSect++)
	{
		HPS::SegmentKey subSeg = _crossSectionSegment.Subsegment();
		CreateVisualizeProfileGeometryFromRISet(subSeg, _sectionElementsArray[iSect]);
	}

	return true;
}
