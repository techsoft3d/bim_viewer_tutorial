#pragma once
//
// IFC Sample Application Class
//
//  Used primarily for storing application data associated with the 
//  state of the application
//

class IFCSampleApp
{
	bool _showPlanView;    
	bool _showPreviewView;

public:
	IFCSampleApp();
	~IFCSampleApp();

	bool GetShowPlanView() { return _showPlanView; }
	void SetShowPlanView(bool state) { _showPlanView = state;  }

	bool GetShowPreviewView() { return _showPreviewView; }
	void SetShowPreviewView(bool state) { _showPreviewView = state; }




};

