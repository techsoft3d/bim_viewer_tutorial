#pragma once

//
//  Class to hold application data associated with the IFC Model
//
class IFCSampleModel
{
public:

	HPS::ComponentPathArray _storeyPaths;
	std::vector<float>      _storeyElevations;

	int  _currentStorey;

	int _mergedModelCount;

public:

	IFCSampleModel();
	~IFCSampleModel();

	void Reset();
	
	void IncrMergeMount() { _mergedModelCount++; }
	int  GetMergeCount() { return _mergedModelCount; }
	void ResetMergeCount() { _mergedModelCount = 0; }

	bool GetElevation(int iStorey, float &elevation);

	int GetStoreyCount() { return _storeyElevations.size(); }

	//
	//  Get the elevaton of a storey and estimate its height based on the
	//  storey above. If its the last storey returns 0 for height
	//
	bool GetElevationAndHeight( int iStorey, float &elevation,  float &height );

	int  GetCurrentStorey() { return _currentStorey; }

	bool SetCurrentStorey(int iStorey);


};

