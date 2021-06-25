#include "stdafx.h"
#include "CHPSApp.h"
#include "CHPSDoc.h"
#include "CHPSView.h"
#include "SandboxHighlightOp.h"

HPS::Selection::Level SandboxHighlightOperator::SelectionLevel = HPS::Selection::Level::Entity;

SandboxHighlightOperator::SandboxHighlightOperator(
	CHPSView * in_cview)
	: HPS::SelectOperator(HPS::MouseButtons::ButtonLeft(), HPS::ModifierKeys()), cview(in_cview) {}

SandboxHighlightOperator::~SandboxHighlightOperator() {} 

bool SandboxHighlightOperator::OnMouseDown(
	HPS::MouseState const & in_state)
{
	auto sel_opts = GetSelectionOptions();

	HPS::Selection::Level level = SandboxHighlightOperator::SelectionLevel;
	level = HPS::Selection::Level::Subentity;

	sel_opts.SetLevel(level);
	SetSelectionOptions(sel_opts);

	HPS::WindowPoint wpoint = in_state.GetLocation();
	HPS::WindowKey wkey = in_state.GetEventSource();

	unsigned int wx, wy;

	wkey.GetWindowInfoControl().ShowWindowPixels(wx, wy);
	float halfX = (float)wx / 2;
	float halfY = (float)wy / 2;


	POINT pt;
	pt.x = (int) (( wpoint.x * halfX )  + halfX  );
	pt.y = (int) (( wpoint.y * halfY ) +  halfY );
	WPARAM wParam = 0;
	LPARAM lParam = MAKELPARAM(pt.x, pt.y);
	
	//PostMessage(cview->m_hWnd, WM_SANDBOX_OP, wParam, lParam); //only used for diagnostics

	if (IsMouseTriggered(in_state) && HPS::SelectOperator::OnMouseDown(in_state))
	{
		HighlightCommon();
		return true;
	}
	return false;
}

bool SandboxHighlightOperator::OnTouchDown(
	HPS::TouchState const & in_state)
{
	auto sel_opts = GetSelectionOptions();
	sel_opts.SetLevel(SandboxHighlightOperator::SelectionLevel);
	SetSelectionOptions(sel_opts);

	if (HPS::SelectOperator::OnTouchDown(in_state))
	{
		HighlightCommon();
		return true;
	}
	return false;
}

void SandboxHighlightOperator::HighlightCommon()
{
	cview->Unhighlight();

	HPS::SelectionResults selection_results = GetActiveSelection();
	size_t selected_count = selection_results.GetCount();
	if (selected_count > 0)
	{
		HPS::CADModel cad_model = cview->GetDocument()->GetCADModel();

		HPS::HighlightOptionsKit highlight_options(HPS::HighlightOptionsKit::GetDefault());
		highlight_options.SetStyleName("highlight_style");

		highlight_options.SetSubentityHighlighting(SelectionLevel == HPS::Selection::Level::Subentity);
		highlight_options.SetOverlay(HPS::Drawing::Overlay::InPlace);

		if (!cad_model.Empty())
		{
			// since we have a CADModel, we want to highlight the components, not just the Visualize geometry
			HPS::SelectionResultsIterator it = selection_results.GetIterator();
			HPS::Canvas canvas = cview->GetCanvas();
			while (it.IsValid())
			{
				HPS::ComponentPath component_path = cad_model.GetComponentPath(it.GetItem());
				if (!component_path.Empty())
				{
					// Make the selected component get highlighted in the model browser
					highlight_options.SetNotification(true); 
					component_path.Highlight(canvas, highlight_options);

					// if we selected PMI, highlight the associated components (if any)
					HPS::Component const & leaf_component = component_path.Front();

		

					if (leaf_component.HasComponentType(HPS::Component::ComponentType::ExchangePMIMask))
					{
						// Only highlight the Visualize geometry for the associated components, don't highlight the associated components in the model browser
						highlight_options.SetNotification(false);
						for (auto const & reference : leaf_component.GetReferences())
							HPS::ComponentPath(1, &reference).Highlight(canvas, highlight_options);
					}
					else if (leaf_component.HasComponentType(HPS::Component::ComponentType::ExchangeTopoFace))
					{
						HPS::Exchange::Component hExComp = (HPS::Exchange::Component)leaf_component;
						A3DEntity * pFaceEntity = hExComp.GetExchangeEntity();
					}
					else if (leaf_component.HasComponentType(HPS::Component::ComponentType::ExchangeTopoCoEdge))
					{
						HPS::Exchange::Component hExComp = (HPS::Exchange::Component)leaf_component;
						A3DEntity * pCoEdgeEntity = hExComp.GetExchangeEntity();
					}
					else if (leaf_component.HasComponentType(HPS::Component::ComponentType::ExchangeTopoEdge))
					{
						HPS::Exchange::Component hExComp = (HPS::Exchange::Component)leaf_component;
						A3DEntity * pEdgeEntity = hExComp.GetExchangeEntity();
		
						size_t csize = component_path.Size();

						//
						// Get the owning RI, then search for the owning coedges
						//
					
						A3DEntity * pRIEntity = 0;
						for (int i = 0; i < csize; i++)
						{
							HPS::Component const & currentComp = component_path.At(i);
							HPS::Exchange::Component hExComp2 = (HPS::Exchange::Component)currentComp;
							HPS::Component::ComponentType cType = hExComp2.GetComponentType();
							if (cType == HPS::Component::ComponentType::ExchangeRIBRepModel)
							{
								pRIEntity = hExComp2.GetExchangeEntity();
								break;
							}

						}
						if (0 != pRIEntity)
						{
						}

					}

				}
				it.Next();
			}
		}
		else
		{
			HPS::SelectionResultsIterator it = selection_results.GetIterator();
			HPS::Canvas canvas = cview->GetCanvas();
			while (it.IsValid())
			{
				HPS::Key key;
				HPS::SelectionItem selectionItem = it.GetItem();
				selectionItem.ShowSelectedItem(key); // 'key' is the HPS::Key of the selected item

				if (key.Type() == HPS::Type::ShellKey)
				{

					HPS::SizeTArray out_faces, out_vertices, edges1, edges2;

					// out_faces is a list of faces
					selectionItem.ShowFaces(out_faces);
					// out_vertices is list of vertices 
					selectionItem.ShowVertices(out_vertices);

					// edges1 and edges2 are parallel 
					selectionItem.ShowEdges(edges1, edges2);
				}
				it.Next();
			}

			HPS::HighlightOptionsKit highlight_options(HPS::HighlightOptionsKit::GetDefault());
			highlight_options.SetSubentityHighlighting(true);
		
		
			// since there is no CADModel, just highlight the Visualize geometry
			cview->GetCanvas().GetWindowKey().GetHighlightControl().Highlight(selection_results, highlight_options);
			
			HPS::Database::GetEventDispatcher().InjectEvent(HPS::HighlightEvent(HPS::HighlightEvent::Action::Highlight, selection_results, highlight_options));
		}
	}

	cview->Update();
}
