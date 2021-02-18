#pragma once
#include "A3DSDKIncludes.h"
#include <vector>
using namespace std;


struct HexPoint
{
	double _x;
	double _y;
	double _z;
	HexPoint(double x, double y, double z) :_x(x), _y(y), _z(z) {};
	HexPoint() :_x(0), _y(0), _z(0) {};
};

class HExInterface
{
	static bool _exchangeInitialized;

public:
	HExInterface();
	~HExInterface();

	static vector< HexPoint> _pointsBuffer;

	static bool Init();

	static void  GetSurfaceInformation(const A3DSurfBase* pSrf);

	static void  GetFaceInformation(A3DEntity* pEnt);

	static void  GetCoEdgeInformation(A3DEntity * pEnt);

	static void  GetEdgeInformation(A3DEntity* pEnt);

	static void  GetModelFaces(A3DEntity* pEnt);

	static bool FindCoEdgesFromEdge(A3DEntity * pRI, A3DEntity * pEdgeEntity, A3DEntity ** pCoEdgeEntity0, A3DEntity ** pCoEdgeEntity1);
	

};

