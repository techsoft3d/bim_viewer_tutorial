#pragma once
#include <vector>
#include "hps.h"
using namespace HPS;

//
// Utility class to store quantity information 
//
struct IFCQuantityRecord
{
	Component *_pcomponent;
	UTF8      _componentName;
	UTF8      _componentType;
	ComponentPathArray _componentGroup; // used for selection
	float     _unitCost;  
	float     _totalCost;

	IFCQuantityRecord(Component *pcomp, UTF8 &name, UTF8 &type,  ComponentPathArray &group)
	{
		_pcomponent = pcomp;
		_componentName = name;
		_componentType = type;
		_componentGroup = group;
		_unitCost = 0;
		_totalCost = 0;

	}

	void SetUnitCost(float cost) { _unitCost = cost; }
	void SetTotalCost(float cost) { _totalCost = cost; }
	int  GetComponentQuantity() { return (int) _componentGroup.size();  }

};

class IFCQuantity
{
	std::vector<IFCQuantityRecord> _componentQuantities;

public:

	 IFCQuantity();
	~IFCQuantity();

	std::vector<IFCQuantityRecord> &GetQuantities();

	IFCQuantityRecord &  AddQuantityRecord(Component *pcomp, UTF8 &name, UTF8 &type,  ComponentPathArray &group);

	void  ClearAll();
};

