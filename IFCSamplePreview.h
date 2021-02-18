#pragma once

//
// Simple Class to display Components in a SubWindow
//
struct IFCQuantityRecord;

class IFCSamplePreview
{

	HPS::Canvas     _canvas;
	HPS::CADModel   _cadModel;
	HPS::SegmentKey _previewKey;
	HPS::IncludeKey _includedComponentKey;

public:
	IFCSamplePreview();
	~IFCSamplePreview();

	bool CreatePreview(HPS::Canvas canvas, HPS::CADModel cadModel);
	void DestroyPreview();
	void UpdatePreview( IFCQuantityRecord &qr);
};

