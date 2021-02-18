#include "stdafx.h"
#include "IFCSampleModel.h"


IFCSampleModel::IFCSampleModel()
{
	_currentStorey = -1;
	_mergedModelCount = 0;
}


IFCSampleModel::~IFCSampleModel()
{
}

void IFCSampleModel::Reset()
{
	_storeyPaths.clear();
	_storeyElevations.clear();
	_currentStorey = -1;

}

bool IFCSampleModel::GetElevation(int iStorey, float & elevation)
{

	if (iStorey < 0 || iStorey >= _storeyElevations.size())
		return false;

	elevation = _storeyElevations[iStorey];

	return true;
}

bool IFCSampleModel::GetElevationAndHeight(int iStorey, float &elevation, float &height)
{
	height = 0;

	if (!GetElevation(iStorey, elevation))
		return false;

	float nextElevation = elevation;

	if (GetElevation(iStorey - 1, nextElevation))
		height = nextElevation - elevation;

	return true;
}

bool IFCSampleModel::SetCurrentStorey(int iStorey)
{
	if ( iStorey < -1  || iStorey >= GetStoreyCount() )
		return false;

	_currentStorey = iStorey;

	return true;
}
