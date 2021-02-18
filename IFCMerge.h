#pragma once
class IFCMerge
{
	std::vector< HPS::UTF8 >_filePaths;
	
public:

	IFCMerge();
	~IFCMerge();

	bool ImportIFCModel(HPS::UTF8 & path );

};

