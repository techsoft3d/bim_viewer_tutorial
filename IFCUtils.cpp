#include "stdafx.h"
#include "hps.h"
#include <sstream>
#include <vector>
#include <algorithm>

using namespace HPS;

#include "IFCUtils.h"
#include "IFCSampleModel.h"


IFCUtils::IFCUtils()
{
}


IFCUtils::~IFCUtils()
{
}

//! [get_units_string]
UTF8 IFCUtils::GetUnitsString(HPS::CADModel cadModel)
{
	if (cadModel.GetComponentType() != HPS::Component::ComponentType::ExchangeModelFile)
		return "";

	HPS::StringMetadata units_metadata = cadModel.GetMetadata("Units");
	if (units_metadata.Empty())
		return "mm";

	HPS::UTF8 full = units_metadata.GetValue();
	if (full == "Millimeter")
		return "mm";
	else if (full == "Centimeter")
		return "cm";
	else if (full == "Meter")
		return "m";
	else if (full == "Kilometer")
		return "km";
	else if (full == "Inch")
		return "in";
	else if (full == "Foot")
		return "ft";
	else if (full == "Yard")
		return "yd";
	else if (full == "Mile")
		return "mi";
	else
	{
		// Just use the full name
		return full;
	}
}
//! [get_units_string]


std::wstring IFCUtils::GetMetadataValueAsWString(HPS::Metadata const & metadata)
{
	std::wstringstream wss;
	switch (metadata.Type())
	{
	case HPS::Type::StringMetadata:
		wss << HPS::StringMetadata(metadata).GetValue();
		break;
	case HPS::Type::UnsignedIntegerMetadata:
		wss << HPS::UnsignedIntegerMetadata(metadata).GetValue();
		break;
	case HPS::Type::IntegerMetadata:
		wss << HPS::IntegerMetadata(metadata).GetValue();
		break;
	case HPS::Type::DoubleMetadata:
		wss << HPS::DoubleMetadata(metadata).GetValue();
		break;
	case HPS::Type::TimeMetadata:
		wss << HPS::TimeMetadata(metadata).GetValue();
		break;
	case HPS::Type::BooleanMetadata:
		wss << HPS::BooleanMetadata(metadata).GetValue();
		break;
	default:
		ASSERT(0);	//Are we missing a metadata type?	
	}
	return wss.str();
}

//! [GetComponentMetaData]
void IFCUtils::GetComponentMetaData(HPS::Component component, std::vector< std::pair<UTF8, UTF8>>  &properties)
{
	HPS::MetadataArray mdArray = component.GetAllMetadata();

	for (auto const &md : mdArray)
	{
		UTF8 utf8Value = GetMetadataValueAsUTF8(md);

		std::pair<UTF8, UTF8> pair(md.GetName(), utf8Value);
		properties.push_back(pair);
	}
}
//! [GetComponentMetaData]


//! [GetMetadataValueAsUTF8]
UTF8 IFCUtils::GetMetadataValueAsUTF8(HPS::Metadata const & metadata)
{
	UTF8  rValue;
	std::stringstream ss;

	switch (metadata.Type())
	{
	case HPS::Type::StringMetadata:
		ss << HPS::StringMetadata(metadata).GetValue();
		rValue = HPS::StringMetadata(metadata).GetValue();
		break;
	case HPS::Type::UnsignedIntegerMetadata:
		ss << HPS::UnsignedIntegerMetadata(metadata).GetValue();
		break;
	case HPS::Type::IntegerMetadata:
		ss << HPS::IntegerMetadata(metadata).GetValue();
		break;
	case HPS::Type::DoubleMetadata:
		ss << HPS::DoubleMetadata(metadata).GetValue();
		break;
	case HPS::Type::TimeMetadata:
		ss << HPS::TimeMetadata(metadata).GetValue();
		break;
	case HPS::Type::BooleanMetadata:
		ss << HPS::BooleanMetadata(metadata).GetValue();
		break;
	default:
		ASSERT(0);	//Are we missing a metadata type?	
	}

	rValue = UTF8(ss.str().c_str());
	return rValue;
}
//! [GetMetadataValueAsUTF8]


bool IFCUtils::GetMetaDataType(HPS::Component component, UTF8 &valueString)
{
	HPS::MetadataArray mdArray = component.GetAllMetadata();

	TRACE("------\n");

	for (auto const &md : mdArray)
	{
		UTF8 name = md.GetName();
		CString MetaDataName(name.GetBytes());
		valueString = HPS::StringMetadata(md).GetValue(); // type value is always a string
		CString MetaDataValue(valueString.GetBytes());

		TRACE(_T("%s: %s\n"), (LPCTSTR)MetaDataName, (LPCTSTR)MetaDataValue);

		if (name == "TYPE")
		{
			valueString = HPS::StringMetadata(md).GetValue(); // type value is always a string
			return true;
		}
	}
	return false;

}


//! [FindIFCStoreys]
void  IFCUtils::FindIFCStoreys(HPS::Component component, HPS::ComponentArray &ancestorcomponents, IFCSampleModel *pIFCModel )
{
	//
	// Get the Exchange Component Type
	// the IFCStory type will always be represented as Product Occurrence. If it not one of those or 
	// the root model file, then do not search any deeper
	//
	HPS::Component::ComponentType cType = component.GetComponentType();


	if (cType != HPS::Component::ComponentType::ExchangeProductOccurrence && cType != HPS::Component::ComponentType::ExchangeModelFile)
		return;

	ancestorcomponents.push_back(component);

	if (cType == HPS::Component::ComponentType::ExchangeProductOccurrence)
	{
		UTF8 strType;
		if (GetMetaDataType(component, strType))
		{

			if (strType == "IFCBUILDINGSTOREY")
			{
				//  Component Path needs to have components ordered leaf -> root
				//  For convenience we build it up the ancestor list vector root->leaf
				//  Create a reversed vector
				ComponentArray reverseComponents(ancestorcomponents);
				std::reverse(reverseComponents.begin(), reverseComponents.end());
				pIFCModel->_storeyPaths.push_back(ComponentPath(reverseComponents));
				ancestorcomponents.pop_back();

				float elevation;
				FindIFCStoryElevation(component, elevation);
				pIFCModel->_storeyElevations.push_back(elevation);

				return;
			}
		}
	}

	HPS::ComponentArray carray = component.GetSubcomponents();
	for (auto comp : carray)
	{
		FindIFCStoreys(comp, ancestorcomponents, pIFCModel);
	}

	ancestorcomponents.pop_back();
}
//! [FindIFCStoreys]


//! [SortIFCStoreysByElevation]
void IFCUtils::SortIFCStoreysByElevation(IFCSampleModel * pIFCModel)
{
	if (NULL == pIFCModel)
		return;

	std::vector<int>  indices(pIFCModel->_storeyElevations.size());

	int index = 0;
	for (auto &a : indices)
		a = index++;

	// sort the indices based on the height;

	std::sort(std::begin(indices), std::end(indices),
		[pIFCModel](int a, int b) { return pIFCModel->_storeyElevations[a] > pIFCModel->_storeyElevations[b]; });

     std::vector<float> tempElevations(  pIFCModel->_storeyElevations);
	 HPS::ComponentPathArray tempPaths(  pIFCModel->_storeyPaths );

	 index = 0;
	 for (auto a : indices)
	 {
		 pIFCModel->_storeyElevations[index] = tempElevations[a];
		 pIFCModel->_storeyPaths[index] = tempPaths[a];
		 index++;
	 }
}
//! [SortIFCStoreysByElevation]


void  IFCUtils::FindIFCComponentByTypeAndName(Component component, UTF8 &strType, UTF8 &strName, HPS::ComponentArray &ancestorcomponents, HPS::ComponentPathArray &componentPaths)
{
	//
	// Get the Exchange Component Type
	// IFCTypes are always repesentted by a Product Occurrence. If it not one of those or 
	// the root model file, then do not search any deeper
	//
	HPS::Component::ComponentType cType = component.GetComponentType();

	if (cType != HPS::Component::ComponentType::ExchangeProductOccurrence && cType != HPS::Component::ComponentType::ExchangeModelFile)
		return;

	ancestorcomponents.push_back(component);

	if (cType == HPS::Component::ComponentType::ExchangeProductOccurrence)
	{
		UTF8 strLocalType;
		if (GetMetaDataType(component, strLocalType))
		{
			bool bMatched = false;
			if (strType == strLocalType)
			{
				if (strName == "*")
					bMatched = true;
				else
				{
					HPS::UTF8 strCompName = component.GetName();
					bMatched = (strCompName == strName);
				}   
				if (bMatched)
				{

					//  Component Path needs to have components ordered leaf -> root
					//  For convenience we build it up the ancestor list vector root->leaf
					//   Create a reversed vector
					ComponentArray reverseComponents(ancestorcomponents);
					std::reverse(reverseComponents.begin(), reverseComponents.end());
					componentPaths.push_back(ComponentPath(reverseComponents));
					ancestorcomponents.pop_back();
					return;
				}
			}
		}
	}

	HPS::ComponentArray carray = component.GetSubcomponents();
	for (auto comp : carray)
		FindIFCComponentByTypeAndName(comp, strType, strName, ancestorcomponents, componentPaths);

	ancestorcomponents.pop_back();
}

bool IFCUtils::FindIFCStoryElevation(HPS::Component component, float &value )
{
	HPS::Metadata md = component.GetMetadata( "IFCBUILDINGSTOREY/Elevation");

	if (md.Empty())
		return false;

	switch (md.Type())
	{
		case HPS::Type::DoubleMetadata:
			value =(float) HPS::DoubleMetadata(md).GetValue();
		break;
		default:
			return false;
	}

	return true;

}

//
// Search a component path for a component which represents a storey
//
bool IFCUtils::FindIFCStory(HPS::ComponentPath componentPath, HPS::Component & component)
{
	for (auto c : componentPath)
	{
		UTF8 strType;
		if (GetMetaDataType(c, strType))
		{
			if (strType == "IFCBUILDINGSTOREY")
			{
				component = c;
				return true;
			}
		}
	}
	return false;
}

bool IFCUtils::MoveCameraToIFCSpace(HPS::ComponentPath componentPath, HPS::View view)
{
	HPS::Component const & component = componentPath.Front();

	//
	//  Checkif this is a space if not return
	//
	HPS::UTF8 valueString;

	if (  !GetMetaDataType(component,valueString) )
		return false;

	if (valueString != "IFCSPACE")
		return false;

	HPS::KeyPathArray keyPaths = componentPath.GetKeyPaths();
	HPS::KeyPath spacePath = keyPaths[0];
	HPS::BoundingKit bound;
	if (!spacePath.ShowNetBounding(bound))
	{
		HPS::Type oType = spacePath.Front().Type();
		return false;
	}

	HPS::SimpleSphere boundingSphere;
	HPS::SimpleCuboid boundingCuboid;

	bound.ShowVolume(boundingSphere, boundingCuboid);

	HPS::CameraKit camKit;

	float height = boundingCuboid.min.z + 1.5;
	HPS::Component storeyComponent;
	if (FindIFCStory(componentPath, storeyComponent))
	{
		float elevation;
		if (IFCUtils::FindIFCStoryElevation(storeyComponent, elevation))
			height = elevation + 1.5;
	}

	HPS::Point spaceCenter(boundingSphere.center.x, boundingSphere.center.y, height);
	HPS::Point camPos, camTarget;
	float w, h;

	view.GetSegmentKey().ShowCamera(camKit);
	camKit.ShowPosition(camPos);
	camKit.ShowTarget(camTarget);
	camKit.ShowField(w, h);
	HPS::Vector deltaCam = spaceCenter - camPos;
	camTarget = camTarget + deltaCam;
	camTarget.z = spaceCenter.z;
	camKit.SetPosition(spaceCenter);
	camKit.SetTarget(camTarget);
	camKit.SetUpVector(HPS::Vector(0, 0, 1));

	//view.GetSegmentKey().SetCamera(camKit);
	view.SmoothTransition(camKit, 0.5f);
	//view.GetAttachedModel().GetSegmentKey().Subsegment("spaceballs").InsertSphere(spaceCenter, boundingSphere.radius);
	//view.GetAttachedModel().GetSegmentKey().Subsegment("spaceballs").GetMaterialMappingControl().SetFaceColor(RGBAColor(1, 0, 1, 0.5f));
	

	return true;
}
