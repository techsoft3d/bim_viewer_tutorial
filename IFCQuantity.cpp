#include "stdafx.h"
#include "IFCQuantity.h"


IFCQuantity::IFCQuantity()
{
}


IFCQuantity::~IFCQuantity()
{
}

std::vector<IFCQuantityRecord>& IFCQuantity::GetQuantities()
{
	return _componentQuantities;
}

IFCQuantityRecord & IFCQuantity::AddQuantityRecord(Component *pcomp, UTF8 & name, UTF8 & type, ComponentPathArray & group)
{
	_componentQuantities.push_back(IFCQuantityRecord( pcomp, name, type,  group ));
	return _componentQuantities.back();
}

void IFCQuantity::ClearAll()
{
	_componentQuantities.clear();
}
