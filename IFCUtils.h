#pragma once

class IFCSampleModel;

//
//  Utility functuons for working with IFC data
//
class IFCUtils
{
public:
	IFCUtils();
	~IFCUtils();

	static  HPS::UTF8 GetMetadataValueAsUTF8(HPS::Metadata const & metadata);

	static HPS::UTF8 GetUnitsString(HPS::CADModel cadModel);

	static std::wstring GetMetadataValueAsWString(HPS::Metadata const & metadata);

	static void GetComponentMetaData(HPS::Component component, std::vector< std::pair<HPS::UTF8, HPS::UTF8>>  &properties);

	static bool GetMetaDataType(HPS::Component component, HPS::UTF8 &valueString);

	static void  FindIFCStoreys(HPS::Component component, HPS::ComponentArray &ancestorcomponents, IFCSampleModel *pIFCModel );

	static void  SortIFCStoreysByElevation(IFCSampleModel *pIFCModel);

	static void  FindIFCComponentByTypeAndName(HPS::Component component, HPS::UTF8 &strType,  HPS::UTF8 &strName, HPS::ComponentArray &ancestorcomponents, HPS::ComponentPathArray &componentPaths);

	static  bool FindIFCStoryElevation(HPS::Component component, float &elevation);

	static  bool FindIFCStory(HPS::ComponentPath componentPath, HPS::Component &component);


	static bool MoveCameraToIFCSpace(HPS::ComponentPath componentPath, HPS::View view);
};

