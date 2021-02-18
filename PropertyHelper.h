#pragma once
#include "stdafx.h"
#include "CommonProperty.h"
#include <set>

#if defined(PARASOLID_DEMO_VIEWER)
#include "parasolid_kernel.h"
#endif

class SimplePropertyHelper
{
public:
	SimplePropertyHelper(
		size_t number_of_selected_items,
		HPS::CADModel const & cad_model,
		HPS::Component const & selected_component,
		HPS::Component const & ifc_component = HPS::Component());

	void SetComponentPath(HPS::ComponentArray const & in_path);

	void GetProperties(bool recalculate_triangle_count = false);

private:
	void		GetPMIProperties(HPS::Exchange::Component const & pmi_component);
	void		GetInheritanceProperties(HPS::Exchange::Component const & exchange_component);

	std::wstring GetUnitsString();

	// Exchange component only properties
	void		GetExchangePhysicalProperties(HPS::Exchange::Component const & exchange_component);
	void		GetExchangeEdgeLength(HPS::Exchange::Component const & edge_component, std::wstring const & units);
	void		GetExchangeComponentProperties(HPS::Exchange::Component const & exchange_component, double & volume, double & area, A3DVector3dData & center_of_gravity);
	void		GetExchangeScale(HPS::Exchange::Component const & in_component, double * out_pocc_scale, double * out_ri_scale, double * out_context_scale);

	// Generic component properties
	void		GetSelectionTypeProperties(HPS::Component const & component);
	HPS::UTF8	ToString(HPS::Exchange::Component::ComponentType component_type);
	void		GetMetadata(HPS::Component const & component, CommonProperty::PropertyType property_type);
	_variant_t	GetMetadataValue(HPS::Metadata const & metadata);
	void		GetMaterialProperties(HPS::Component const & component);
	void		GetTransformationProperties(HPS::Component const & component);
	void		GetComponentCount(HPS::Component const & component, int & number_of_triangles, int & number_of_faces, int & number_of_edges);
	void		RecursiveComponentCount(HPS::Component const & component, int & number_of_triangles, int & number_of_faces, int & number_of_edges);
	void		GetPolyBRepCount(HPS::Component const & component, int & out_tris, int & out_faces, int & out_edges);

#if defined(PARASOLID_DEMO_VIEWER)
	// Parasolid component only properties
	void		GetParasolidPhysicalProperties(HPS::Parasolid::Component const & parasolid_component);
	void		GetParasolidEdgeLength(HPS::Parasolid::Component const & parasolid_component, std::wstring const & units, double scale);
	void		GetParasolidComponentProperties(HPS::Parasolid::Component const & component, double & volume, double & area, double & length, HPS::Vector & center_of_gravity);
	void		GetParasolidBodyProperties(PK_BODY_t body, double & volume, double & area, double & length, HPS::Vector & center_of_gravity);
	PK_ERROR_code_t GetMassProperties(const PK_TOPOL_t topol, double accuracy, const PK_TOPOL_eval_mass_props_o_t * options, double & volume, double & surface, double & mass, double c_of_g[3]);
#endif

	HWND						m_main_frame_handle;
	size_t						m_number_of_selected_items;
	HPS::CADModel				m_cad_model;
	HPS::Component				m_selected_component;
	HPS::Component				m_ifc_component;
	HPS::KeyPath				m_key_path;
	HPS::ComponentArray			m_component_path;

#if defined(EXCHANGE_DEMO_VIEWER)
	std::set<A3DEntity *>		m_unique_edges;
#elif defined(PARASOLID_DEMO_VIEWER)
	std::set<PK_ENTITY_t>		m_unique_edges;
#endif

	//IFC metadata are nested. For example a metadata value would be foo/bar and its value would be 'hello'
	//when this happens the an entry is added to the multimap, with key foo and value of (bar, 'hello')
	//At the end of the import a property category is created for each key in the map, and each value
	//associated with the key becomes a sub-property
	std::set<std::wstring>												m_ifc_properties_keys;
	std::multimap<std::wstring, std::pair<std::wstring, _variant_t>>	m_ifc_properties;
};