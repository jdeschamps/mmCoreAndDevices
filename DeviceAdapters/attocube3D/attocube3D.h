///////////////////////////////////////////////////////////////////////////////
// FILE:          attocube3D.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   attocube device adapter controlling a 1D and a 2D stage
//
// AUTHOR:        
//
// LICENSE:       LGPL
//


#ifndef _ATTOCUBE_H_
#define _ATTOCUBE_H_

#include "MMDevice.h"
#include "DeviceBase.h"
#include <string>
#include <map>


// TODO: examples of error definitions defined for SmarAct, should be adapted to attocube
// (see .cpp file for more details on how they are used)
#define ERR_PORT_CHANGE_FORBIDDEN 99
#define ERR_IDENTIFICATION_FAIL 1000
#define ERR_PARSING 1001

// TODO:  XYStage and ZStage could be renamed ANPxy and ANPz instead
// Depending on the type of attocube ANP devices, you could consider making
// two 1D classes (ANPx and ANPz) and one 2D class (ANPxy). Indeed it seems
// that ANPx and ANPz might have different properties.

// Note: XYStage is the name of the class, it is the same name than the constructor
// and the destructor (see below). In the .cpp files, all methods from the class
// are implemented as "XYStage::FunctionName". If you change the name of the class
// (e.g. to ANPxy), then you need to change it in all these places.
// Note: CXYStageBase is the parent class, and it goes: "ParentClas<ClassName>"
class XYStage : public CXYStageBase<XYStage>
{
public:
	XYStage(); // constructor
	~XYStage(); // destructor

	// Note: here follows all the function/method declarations. Some are imposed by the 
	// CXYStageBase parent class, some are specific to this adapter.

	// Device API (mandatory)
	// ----------
	int Initialize();
	int Shutdown();

	void GetName(char* pszName) const;
	bool Busy();
	int IsXYStageSequenceable(bool& isSequenceable) const {isSequenceable = false; return DEVICE_OK;}
	bool IsContinuousFocusDrive() const {return false;}
	int Home();
	int Stop();

	// Stage API (not all mandatory)
	// ---------

	// setters
	int SetPositionUm(double x, double y);
	int SetOrigin();
	int SetPositionSteps(long x, long y);
	int SetFrequency(int x);
	int SetRelativePositionUm(double x, double y);

	// getters
	int GetPositionSteps(long& x, long& y);
	int GetLimitsUm(double& xMin, double& xMax, double& yMin, double& yMax);
	int GetPositionUm(double& x, double& y);
	double GetStepSizeXUm();
	double GetStepSizeYUm();

	int GetID(std::string* id);
	int GetStepLimits(long &xMin, long &xMax, long &yMin, long &yMax);

	// action interface
	// ----------------
	int OnFrequency(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnHold(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnPort(MM::PropertyBase* pProp, MM::ActionType eAct);

	// Note: here the members are defined, they can be used to store values 
	// for instance values that can be useful to show in properties or to 
	// use when talking to the stage
private:
	std::string port_;
	bool initialized_;
	double curPos_x_;
	double curPos_y_;
	double answerTimeoutMs_;
	int reverseX_;
	int reverseY_;
	int freqXY_;
	int channelX_;
	int channelY_;
	int holdtime_;
	std::string id_;
};

class ZStage : public CStageBase<ZStage>
{
public:
	ZStage();
	~ZStage();

	// Device API
	// ----------
	int Initialize();
	int Shutdown();

	void GetName(char* pszName) const;
	bool Busy();

	int IsStageSequenceable(bool& isSequenceable) const {isSequenceable = false; return DEVICE_OK;}
	bool IsContinuousFocusDrive() const {return false;}

	// Stage API
	// ---------

	// setters
	int SetPositionUm(double pos);
	int SetRelativePositionUm(double pos);
	int SetPositionSteps(long steps);
	int SetOrigin();
	int SetFrequency(int x);

	// getters
	int GetPositionUm(double& pos);
	int GetPositionSteps(long& steps);
	int GetLimits(double& min, double& max);
	int GetController(std::string* controller);
	int GetID(std::string* id);

	// action interface
	// ----------------
	int OnLimit(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnPort(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnFrequency(MM::PropertyBase* pProp, MM::ActionType eAct);

private:
	std::string port_;
	bool initialized_;
	int channelZ;
	double answerTimeoutMs_;
	int reverseZ_;
	int freqZ_;
	int channelZ_;
	int holdtime_;
	std::string id_;
	std::string controller_;
};

#endif //_Smaract_H_
