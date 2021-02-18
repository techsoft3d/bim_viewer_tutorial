#pragma once
#include "hps.h"
#include <comutil.h>

#define WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY						(WM_USER + 100)
#define WM_HOOPS_DEMO_VIEWER_REMOVE_ALL_PROPERTIES				(WM_USER + 101)
#define WM_HOOPS_DEMO_VIEWER_INITIALIZE_BROWSER					(WM_USER + 102)
#define WM_HOOPS_DEMO_VIEWER_ADD_SEGMENT_BROWSER_PROPERTY		(WM_USER + 103)
#define WM_HOOPS_DEMO_VIEWER_FLUSH_SEGMENT_BROWSER_PROPERTIES	(WM_USER + 104)
#define WM_HOOPS_DEMO_VIEWER_REMOVE_CP_INFO_PANEL				(WM_USER + 105)
#define WM_HOOPS_DEMO_VIEWER_UNSET_SEGMENT_BROWSER_ATTRIBUTE	(WM_USER + 106)
#define WM_HOOPS_DEMO_VIEWER_REEXPAND_MODEL_BROWSER				(WM_USER + 107)

class CommonProperty
{
public:
	enum class PropertyType
	{
		None,
		File, 
		General,
		Metadata,
		PMI,
		Time,
		Feature,
	};

	enum class SelectionMode
	{
		Faces,
		Edges,
		FacesAndEdges,
	};

	class Property
	{
	public:

		Property();
		Property(HPS::UTF8 name, _variant_t value, PropertyType type, bool sub_grid = false, bool extra_data = false);
		~Property();

		enum class VectorLabel
		{
			Label_XYZ,
			Label_012
		};

		static Property *	AddVector(HPS::Vector & vector, HPS::UTF8 const & property_name, PropertyType property_type, VectorLabel label = VectorLabel::Label_XYZ);
		static bool			ToBool(signed char in_bool);

		//When a property has sub properties do the following:
		//1. specify a property name
		//2. set add_sub_grid to true
		//3. add the sub properties to the sub_properties vector
		HPS::UTF8					property_name;
		_variant_t					property_value;		//only valid if entry_type is set to GridEntry
		COLORREF					color;				//only valid if entry_type is set to ColorEntry
		PropertyType				property_type;
		PropertyType				property_subtype;
		bool						add_sub_grid;
		std::vector<Property *>		sub_properties;

		enum class EntryType
		{
			GridEntry,
			ColorEntry,
			ButtonEntry
		};

		EntryType					entry_type;

		enum class Action
		{
			PopulateBlend03Elements,	//grab data on the elements making up a blend03 surface
			PopulateLoops,				//get loops data
			PopulateMatrixTable,		//populates the matrix dialog from m_matrices
			PopulateLogDialog,			//populates the import log dialog
		};

		class ExtraData
		{
		public:
			ExtraData(void * in_component, Action in_action, bool in_expanded = true);
			void *		component;	//component on which the action will be performed
			Action		action;		//action to be performed on the component
			bool		expanded;	//true if the element is expanded

			std::vector<HPS::Vector>	m_matrices;			//HPS::Vectors represent rx, ry, rz, t and s in this order

			std::wstring				m_string;		
		};

		bool						has_extra_data;
		ExtraData *					extra_data;
	};

	static _variant_t GetMetadataValue(HPS::Metadata const & metadata);
	static void GetMaterialProperties(HWND main_frame_handle, HPS::KeyPath const & in_key_path);
	static void GetTransformationProperties(HWND main_frame_handle, HPS::KeyPath const & in_key_path);
	static bool ReallyGetTransformationProperties(CommonProperty::Property * transformation_property, HPS::MatrixKit const & matrix);

private:
	static void ReallyGetMaterialProperties(CommonProperty::Property * material_property, HPS::MaterialKit const & material, HPS::Material::Type material_type);
	

};