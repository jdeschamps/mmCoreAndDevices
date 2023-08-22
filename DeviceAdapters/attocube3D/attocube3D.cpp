///////////////////////////////////////////////////////////////////////////////
// FILE:          attocube3D.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   
//
// AUTHOR:        
//
// LICENSE:       LGPL
//

#ifdef WIN32
#include <windows.h>
#endif
#include "FixSnprintf.h"

// Note: import the class definitions
#include "attocube3D.h"

#include <string>
#include <math.h>
#include "ModuleInterface.h"
#include <sstream>

// Note: some constants defined here
const char* g_XYStageDeviceName = "attocube 2D";
const char* g_ZStageDeviceName = "attocube 1D";


using namespace std;

///////////////////////////////////////////////////////////////////////////////
// Exported MMDevice API
///////////////////////////////////////////////////////////////////////////////
// Note: function called when starting the Hardware Configuration Wizard, it 
// defines which devices are available 
MODULE_API void InitializeModuleData()
{
	// Note: here we add the devices
	RegisterDevice(g_ZStageDeviceName, MM::StageDevice, "attocube 1D stage");
	RegisterDevice(g_XYStageDeviceName, MM::XYStageDevice, "attocube 2D stage");
}

// Note: this function is called when we have chosen a device and it returns the
// requested device
MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
	if (deviceName == 0)
		return 0;

	// Note: if the device is "attocube 2D"
	if (strcmp(deviceName, g_XYStageDeviceName) == 0)
	{
		// Note: instantiate the device and return it.
		// The class is defined in attocube3D.h
		XYStage* s = new XYStage();
		return s;
	}

	// Note: if the device is "attocube 1D"
	if (strcmp(deviceName, g_ZStageDeviceName) == 0)
	{
		ZStage* s = new ZStage();
		return s;
	}
	return 0;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
	delete pDevice;
}

// TODO: general function that is specific to SmarAct, it will need to be changed
// for attocube specifics error, or removed altogether.
// Note: This function checks if the answer is a SmarAct error message and returns the
// error code (integer)
bool isError(std::string answer, int* error){
	if(answer.compare(":E0") == 0){ // if no error, then return false
		return false;
	}

	if(answer.substr(0,2).compare(":E") == 0){
		std::string s = answer.substr(2); // should check if it is integer (for robustness)
		*error = atoi(s.c_str());
		return true;
	} else {
		*error = 0;
		return false;
	}
}

// TODO: general function that is specific to SmarAct, it will need to be changed
// for attocube specifics error, or removed altogether.
// Note: This function translates the error code (see isError function) into one of the 
// error constants. The error constants are defined in attocube3D.h and the messages
// shown to users in the constructor.
int GetErrorStatus(int error){
	switch(error){
	case 0:
		return DEVICE_OK;
	case 1:
		return ERR_PARSING;
	case 2:
		return ERR_UNKNWON_COMMAND;
	case 3:
		return ERR_INVALID_CHANNEL;
	case 4:
		return ERR_INVALID_MODE;
	case 13:
		return ERR_SYNTAX;
	case 15:
		return ERR_OVERFLOW;
	case 17:
		return ERR_INVALID_PARAMETER;
	case 18:
		return ERR_MISSING_PARAMETER;
	case 19:
		return ERR_NO_SENSOR_PRESENT;
	case 20:
		return ERR_WRONG_SENSOR_TYPE;
	}
	return ERR_UNKNOWN_ERROR;
}


///////////////////////////////////////////////////////////////////////////////
/////////////////////////////// XYStage ///////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Note: XYStage class implementation
XYStage::XYStage() :
	// Note: Here are the default values of the class members
	port_("Undefined"),
	initialized_(false), 
	answerTimeoutMs_(1000),
	reverseX_(1),
	reverseY_(1),
	freqXY_(5000),
	channelX_(0),
	channelY_(1),
	holdtime_(10),
	id_("")
{
	InitializeDefaultErrorMessages();

	// TODO: Adjust the error codes and messages based on what the attocube can return  
	// Note: This defines the message shown to users when one of the error constants
	// are returned by a function
	SetErrorText(ERR_PORT_CHANGE_FORBIDDEN, "Port change forbidden.");
	SetErrorText(ERR_IDENTIFICATION_FAIL, "Fail to communicate with a SmarAct stage.");
	SetErrorText(ERR_PARSING, "The command could not be processed due to a parsing error.");
	SetErrorText(ERR_UNKNWON_COMMAND, "Unknown command.");
	SetErrorText(ERR_INVALID_CHANNEL, "The channel index is invalid and the command could not be processed.");
	SetErrorText(ERR_INVALID_MODE, "The parameter that defines the mode for automatic error reporting is not valid.");
	SetErrorText(ERR_SYNTAX, "The command could not be processed due to a syntax error.");
	SetErrorText(ERR_OVERFLOW, "A number value given was too large to be processed.");
	SetErrorText(ERR_INVALID_PARAMETER, "A parameter that was given with the command was invalid.");
	SetErrorText(ERR_MISSING_PARAMETER, "A parameter was omitted where it was required.");
	SetErrorText(ERR_NO_SENSOR_PRESENT, "Wrong positioner adress: no sensor present.");
	SetErrorText(ERR_WRONG_SENSOR_TYPE, "Wrong sensor required for this command.");
	SetErrorText(ERR_UNKNOWN_ERROR, "Unknown error.");

	// Note: here we create properties that are shown when selecting the device in the
	// Hardware Configuration Wizard

	// TODO: change the pre-init properties if necessary. For instance whatever information
	// the user knows about the stage that cannot be retrieved from communication can be entered
	// here. e.g.: volt_to_um, or maximum_length, etc.


	// Note: name, default value, type, readOnly, reference to an action function, isPreInitProperty
	// here they should all be action_function=0 and isPreInitProperty=true
	CreateProperty("X channel", "0", MM::Integer, false, 0, true);
	// Note: these are allowed values
	// TODO: the attocube probably can be one of 7 channels? So it might need to be adjusted here
	AddAllowedValue("X channel", "0");
	AddAllowedValue("X channel", "1");
	AddAllowedValue("X channel", "2");

	CreateProperty("X direction", "1", MM::Integer, false, 0, true);
	AddAllowedValue("X direction", "-1");
	AddAllowedValue("X direction", "1");

	// TODO: same here for the other axis
	CreateProperty("Y channel", "1", MM::Integer, false, 0, true);
	AddAllowedValue("Y channel", "0");
	AddAllowedValue("Y channel", "1");
	AddAllowedValue("Y channel", "2");

	CreateProperty("Y direction", "1", MM::Integer, false, 0, true);
	AddAllowedValue("Y direction", "-1");
	AddAllowedValue("Y direction", "1");

	// TODO: There might be additional properties that the user needs to set before 
	// starting to initialize the stage

	// Note: properties that are not pre-init and that are constant
	// Name
	CreateProperty(MM::g_Keyword_Name, g_XYStageDeviceName, MM::String, true);

	// Description
	CreateProperty(MM::g_Keyword_Description, "attocube 2D Stage", MM::String, true);

	// Port
	CPropertyAction* pAct = new CPropertyAction (this, &XYStage::OnPort);
	CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);
}

XYStage::~XYStage()
{
	Shutdown();
}

void XYStage::GetName(char* Name) const
{
	CDeviceUtils::CopyLimitedString(Name, g_XYStageDeviceName);
}

// TODO: adapt the properties to the attocube
// Note: This function is called when initializing the device
int XYStage::Initialize()
{
	// TODO: SmarAct specific thing where the stage has to be put in error reporting mode
	int ret = SetErrorReporting(true);
	if (ret != DEVICE_OK)
		return ret;

	//////////////////////////////////////////////////////////////////
	// Define channel and direction
	// Note: this gets the pre-init properties and save them in the class members
	char charbuff[MM::MaxStrLength];
	ret = GetProperty("X direction", charbuff);
	if (ret != DEVICE_OK)
		return ret;
	reverseX_ = atoi(charbuff); 

	ret = GetProperty("X channel", charbuff);
	if (ret != DEVICE_OK)
		return ret;
	channelX_ = atoi(charbuff); 

	ret = GetProperty("Y direction", charbuff);
	if (ret != DEVICE_OK)
		return ret;
	reverseY_ = atoi(charbuff); 

	ret = GetProperty("Y channel", charbuff);
	if (ret != DEVICE_OK)
		return ret;
	channelY_ = atoi(charbuff); 

	//////////////////////////////////////////////////////////////////
	// Hold time property
	// TODO: This might be a smaract specific property
	CPropertyAction* pAct = new CPropertyAction (this, &XYStage::OnHold);
	CreateProperty("Hold time (ms)", "0", MM::Integer, false, pAct);
	// Note: set property limits
	SetPropertyLimits("Hold time (ms)", 1, 60000);

	// Frequency
	// TODO: adapt this to the frequency possibilities of the attocube
	// Note: First we create a property action that is linked to the 
	// OnFrequency function (refered to as "action handlers")
	pAct = new CPropertyAction (this, &XYStage::OnFrequency);
	// Note: Then we create the property and link it to pAct
	// When the property is changed by the user, this will trigger OnFrequency
	CreateProperty("Frequency", "5000", MM::Integer, false, pAct);
	SetPropertyLimits("Frequency", 1, 18500);

	/////////////////////////////////////////////////////////////////
	// Get ID
	// Note: here we call a function to retrieve the ID of the device and
	// store it in a property
	ret = GetID(&id_);
	if (ret != DEVICE_OK)
		return ret;
	CreateProperty("ID", id_.c_str(), MM::String, true);

	initialized_ = true;
	return DEVICE_OK;
}

int XYStage::Shutdown()
{
	if (initialized_)
	{
		initialized_ = false;
	}
	return DEVICE_OK;
}

// TODO: Will need to be adapted
bool XYStage::Busy()
{
	// Note: this functions checks whether the device is busy and is SmarAct specific 
	string answer;

	// Note: example of how to communicate with the device 
	std::stringstream command; // create a command
	command << ":M" << channelX_; // add strings to form the command (here SmarAct specific), this should look like the serial communication on the terminal
	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\n"); // send the serial command								
	ret = GetSerialAnswer(port_.c_str(), "\n", answer); // retrieve an answer 
	// if ret = 0, it means the device is ok, it answered correctly
	if (ret != DEVICE_OK){
		return true;
	}

	// Note: SmarAct specific answer
	if(strcmp(answer.substr(3).c_str(),"S") != 0){
		return true;
	}

	// Note: check the second axis 
	std::stringstream command2;
	command2 << ":M" << channelY_;
	ret = SendSerialCommand(port_.c_str(), command2.str().c_str(), "\n");									
	ret = GetSerialAnswer(port_.c_str(), "\n", answer);
	if (ret != DEVICE_OK){
		return true;
	}

	if(strcmp(answer.substr(3).c_str(),"S") != 0){  
		return true;
	}

	return false;
}

//////////////////////////////////////////////////
/// setters
// TODO: SmarAct specific function
int XYStage::SetErrorReporting(bool reporting){
	string answer;
	std::stringstream command;
	if(reporting){
		command << ":E1";
	} else {
		command << ":E0";
	}

	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\n");	
	ret = GetSerialAnswer(port_.c_str(), "\n", answer);
	if (ret != DEVICE_OK)
		return ret;

	// check for error
	int error; 
	if(isError(answer, &error)){
		return GetErrorStatus(error);
	}

	return DEVICE_OK;
}

// Note: One of the function that needs to be implemented because it is defined in the parent class
// TODO: Needs to be adapted for the attocube commands setting position
int XYStage::SetRelativePositionUm(double x, double y){
	int ret = 0;
	if(x != 0){ // if non null relative position in first channel

		// need to round off to first decimal otherwise the stage 
		// cannot process the position
		double xpos = ceil(x*10)/10;

		// send command
		std::stringstream command;
		command << ":MPR" << channelX_ << "P" << xpos*reverseX_ << "H" << holdtime_;
		ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\n");

		// check for answer
		string answer;
		ret = GetSerialAnswer(port_.c_str(), "\n", answer);
		if (ret != DEVICE_OK)
			return ret;

		// is error?
		int error; 
		if(isError(answer, &error)){
			return GetErrorStatus(error);
		}
	}

	if(y != 0){ // if non null relative position in second channel
		double ypos = ceil(y*10)/10;

		// send command
		std::stringstream command2;
		command2 << ":MPR" << channelY_ << "P" << ypos*reverseY_ << "H" << holdtime_;
		ret = SendSerialCommand(port_.c_str(), command2.str().c_str(), "\n");
		
		// check for answer
		string answer;
		ret = GetSerialAnswer(port_.c_str(), "\n", answer);
		if (ret != DEVICE_OK)
			return ret;

		// check for error
		int error; 
		if(isError(answer, &error)){
			return GetErrorStatus(error);
		}
	}


	return DEVICE_OK;
}

// Note: One of the function that needs to be implemented because it is defined in the parent class
// TODO: Needs to be adapted for the attocube commands setting position
int XYStage::SetPositionUm(double x, double y){
	// round to first decimal, otherwise the stage cannot
	// process the value
	double xpos = ceil(x*10)/10;
	double ypos = ceil(y*10)/10;

	// set position of the first channel
	std::stringstream command;
	command << ":MPA" << channelX_ << "P" << xpos*reverseX_ << "H" << holdtime_;
	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\n");
	if (ret != DEVICE_OK){
		return ret;
	}

	// check for answer
	string answer;
	ret = GetSerialAnswer(port_.c_str(), "\n", answer);
	if (ret != DEVICE_OK)
		return ret;

	// is it an error?
	int error; 
	if(isError(answer, &error)){
		return GetErrorStatus(error);
	}

	// set position of the second channel
	std::stringstream command2;
	command2 << ":MPA" << channelY_ << "P" << ypos*reverseY_ << "H" << holdtime_;
	ret = SendSerialCommand(port_.c_str(), command2.str().c_str(), "\n");
	if (ret != DEVICE_OK){
		return ret;
	}

	// chekc for answer
	ret = GetSerialAnswer(port_.c_str(), "\n", answer);
	if (ret != DEVICE_OK)
		return ret;

	// is it an error?
	if(isError(answer, &error)){
		return GetErrorStatus(error);
	}

	return DEVICE_OK;
}

// Note: Not one of the mandatory (parent class defined) function
// TODO: Needs to be adapted for the attocube commands
int XYStage::SetFrequency(int freq)
{
	// set first channel's frequency
	std::stringstream command;
	command << ":SCLF" << channelX_ << "F" << freq;
	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\n");

	// check for answer
	string answer;
	ret = GetSerialAnswer(port_.c_str(), "\n", answer);
	if (ret != DEVICE_OK)
		return ret;

	// is it an error?
	int error; 
	if(isError(answer, &error)){
		return GetErrorStatus(error);
	}

	// set second channel's frequency
	std::stringstream command2;
	command2 << ":SCLF" << channelY_ << "F" << freq;
	ret = SendSerialCommand(port_.c_str(), command2.str().c_str(), "\n");
	
	// check for answer
	ret = GetSerialAnswer(port_.c_str(), "\n", answer);
	if (ret != DEVICE_OK)
		return ret;

	// is it an error?
	if(isError(answer, &error)){
		return GetErrorStatus(error);
	}

	return DEVICE_OK;
}

// TODO: could be repurposed to set the stage back to an origin position
int XYStage::SetOrigin()											
{
	// set first channel's origin
	std::stringstream command;
	command << ":SZ" << channelX_;
	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\n");
	
	string answer;
	ret = GetSerialAnswer(port_.c_str(), "\n", answer);
	if (ret != DEVICE_OK)
		return ret;

	// check for error
	int error; 
	if(isError(answer, &error)){
		return GetErrorStatus(error);
	}
	
	// set second channel's origin
	std::stringstream command2;
	command2 << ":SZ" << channelY_;
	ret = SendSerialCommand(port_.c_str(), command2.str().c_str(), "\n");
	
	ret = GetSerialAnswer(port_.c_str(), "\n", answer);
	if (ret != DEVICE_OK)
		return ret;
	
	// check for error 
	if(isError(answer, &error)){
		return GetErrorStatus(error);
	}

	return DEVICE_OK;
}

//////////////////////////////////////////////////
/// getters

// Note: Not one of the mandatory (parent class defined) function
// TODO: Needs to be adapted for the attocube commands getting position
int XYStage::GetPositionUm(double& x, double& y)
{	
	string answer;
	std::stringstream command;
	command << ":GP" << channelX_;

	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\n");			
	ret = GetSerialAnswer(port_.c_str(), "\n", answer);
	if (ret != DEVICE_OK){
		return ret;
	}

	// check for error
	int error; 
	if(isError(answer, &error)){
		return GetErrorStatus(error);
	}

	// if not error, then extract position
	curPos_x_=atof(answer.substr(4).c_str())*reverseX_;

	std::stringstream command2;
	command2 << ":GP" << channelY_;
	ret = SendSerialCommand(port_.c_str(), command2.str().c_str(), "\n");	
	ret = GetSerialAnswer(port_.c_str(), "\n", answer);
	if (ret != DEVICE_OK){
		return ret;
	}
	
	// check for error
	if(isError(answer, &error)){
		return GetErrorStatus(error);
	}

	// extract position
	curPos_y_=atof(answer.substr(4).c_str())*reverseY_;

	x =  curPos_x_;
	y =  curPos_y_;

	return DEVICE_OK;
}


// Note: Not one of the mandatory (parent class defined) function
// TODO: Needs to be adapted for the attocube commands setting position
int XYStage::GetID(std::string* id)
{
	string answer;
	std::stringstream command;
	command << ":GID";

	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\n");	
	ret = GetSerialAnswer(port_.c_str(), "\n", answer);
	if (ret != DEVICE_OK)
		return ret;
	
	// check for error
	int error; 
	if(isError(answer, &error)){
		return GetErrorStatus(error);
	}

	// if not, get id
	*id = answer.substr(3);
	
	return DEVICE_OK;
}

/////////////////////////////////////////////////////////////////
/////////////// Unsupported commands ////////////////////////////
// TODO: Unsupported in SmarAct but could be supported with attocube
int XYStage::SetPositionSteps(long /* x */, long /* y */)
{

	return DEVICE_UNSUPPORTED_COMMAND;
}

int XYStage::GetPositionSteps(long& /* x */, long& /* y */)
{

	return DEVICE_UNSUPPORTED_COMMAND;
}

int XYStage::GetLimitsUm(double& /* xMin */, double& /* xMax */, double& /* yMin */, double& /* yMax */)			
{
	return DEVICE_UNSUPPORTED_COMMAND;
}

int XYStage::Home()
{
	return DEVICE_UNSUPPORTED_COMMAND;
}

int XYStage::Stop()
{
	return DEVICE_UNSUPPORTED_COMMAND;
}

int XYStage::GetStepLimits(long& /* xMin */, long& /* xMax */, long& /* yMin */, long& /* yMax */)
{
	return DEVICE_UNSUPPORTED_COMMAND;
}

double XYStage::GetStepSizeXUm()
{
	return DEVICE_UNSUPPORTED_COMMAND;
}

double XYStage::GetStepSizeYUm()
{
	return DEVICE_UNSUPPORTED_COMMAND;
}

///////////////////////////////////////////////////////////////////////////////
// Action handlers
///////////////////////////////////////////////////////////////////////////////
// Note: action handlers are called when a property is changed
 
// Note: There is no handler for set and get position because they are part of the
// parent class, through the set and get position functions.


// Note: "The BeforeGet action occurs before the Device Adapter's client views the property and 
// should query the hardware if necessary, AfterSet occurs after the client has specified a new 
// value of the property and should write the property out to the hardware if necessary."
int XYStage::OnFrequency(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	
	if (eAct == MM::BeforeGet) // called before self-updating the device property browser
	{ 
		// Note: in SmarAct, the stage does not update the frequency, so no need to query the device
		pProp->Set((long)freqXY_);
	}
	else if (eAct == MM::AfterSet) // called after the property has been changed
	{
		// retrieve the new property value
		long pos;
		pProp->Get(pos);

		// store it in the class member
		freqXY_ = pos;

		// update the device value
		SetFrequency(freqXY_);
	}

	return DEVICE_OK;
}

// TODO: SmarAct specific, probably not useful for attocube
int XYStage::OnHold(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set((long)holdtime_);
	}
	else if (eAct == MM::AfterSet)
	{
		long pos;
		pProp->Get(pos);
		holdtime_ = pos;
	}

	return DEVICE_OK;
}

// Note: probably no need to change that, it's pretty standard
int XYStage::OnPort(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(port_.c_str());
	}
	else if (eAct == MM::AfterSet)
	{
		if (initialized_)
		{
			// revert
			pProp->Set(port_.c_str());
			return ERR_PORT_CHANGE_FORBIDDEN;
		}

		pProp->Get(port_);
	}

	return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////
//////////////////////////////// ZStage ///////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

ZStage::ZStage() :
    port_("Undefined"),
	initialized_(false),
	channelZ_(2),
	answerTimeoutMs_(1000),
	reverseZ_(1),
	freqZ_(5000),		  
	holdtime_(10),
	id_(""),
	controller_("")
{
	InitializeDefaultErrorMessages();

	SetErrorText(ERR_PORT_CHANGE_FORBIDDEN, "Port change forbidden.");
	SetErrorText(ERR_IDENTIFICATION_FAIL, "Fail to communicate with a SmarAct stage.");
	SetErrorText(ERR_PARSING, "The command could not be processed due to a parsing error.");
	SetErrorText(ERR_UNKNWON_COMMAND, "Unknown command.");
	SetErrorText(ERR_INVALID_CHANNEL, "The channel index is invalid and the command could not be processed.");
	SetErrorText(ERR_INVALID_MODE, "The parameter that defines the mode for automatic error reporting is not valid.");
	SetErrorText(ERR_SYNTAX, "The command could not be processed due to a syntax error.");
	SetErrorText(ERR_OVERFLOW, "A number value given was too large to be processed.");
	SetErrorText(ERR_INVALID_PARAMETER, "A parameter that was given with the command was invalid.");
	SetErrorText(ERR_MISSING_PARAMETER, "A parameter was omitted where it was required.");
	SetErrorText(ERR_NO_SENSOR_PRESENT, "Wrong positioner adress: no sensor present.");
	SetErrorText(ERR_WRONG_SENSOR_TYPE, "Wrong sensor required for this command.");
	SetErrorText(ERR_UNKNOWN_ERROR, "Unknown error.");

	// create pre-initialization properties
	// ------------------------------------
	CreateProperty("Z channel", "0", MM::Integer, false, 0, true);
	AddAllowedValue("Z channel", "0");
	AddAllowedValue("Z channel", "1");
	AddAllowedValue("Z channel", "2");

	CreateProperty("Z direction", "1", MM::Integer, false, 0, true);
	AddAllowedValue("Z direction", "-1");
	AddAllowedValue("Z direction", "1");

	// Name
	CreateProperty(MM::g_Keyword_Name, g_ZStageDeviceName, MM::String, true);

	// Description
	CreateProperty(MM::g_Keyword_Description, "Smaract 1D Stage", MM::String, true);

	// Port
	CPropertyAction* pAct = new CPropertyAction (this, &ZStage::OnPort);
	CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);
}

ZStage::~ZStage()
{
	Shutdown();
}

void ZStage::GetName(char* Name) const
{
	CDeviceUtils::CopyLimitedString(Name, g_ZStageDeviceName);
}

int ZStage::Initialize()
{	
	// Make sure we are in error reporting mode
	int ret = SetErrorReporting(true);
	if (ret != DEVICE_OK)
		return ret;
	
	//////////////////////////////////////////////////////////////////
	// Define channel and direction
	char charbuff[MM::MaxStrLength];
	ret = GetProperty("Z direction", charbuff);
	if (ret != DEVICE_OK)
		return ret;
	reverseZ_ = atoi(charbuff); 
	ret = GetProperty("Z channel", charbuff);
	if (ret != DEVICE_OK){
		return ret;
	}
	channelZ_ = atoi(charbuff); 

	// Frequency
	CPropertyAction* pAct = new CPropertyAction (this, &ZStage::OnFrequency);
	CreateProperty("Frequency", "5000", MM::Integer, false, pAct);
	SetPropertyLimits("Frequency", 1, 18500);

	// Controller type
	ret = GetController(&controller_);
	if (ret != DEVICE_OK){
		return ret;
	}
	CreateProperty("Controller", controller_.c_str(), MM::String, true);

	// Create ID
	ret = GetID(&id_);
	if (ret != DEVICE_OK){
		return ret;
	}

	CreateProperty("ID", id_.c_str(), MM::String, true);
	
	initialized_ = true;

	return DEVICE_OK;
}

int ZStage::Shutdown()
{
	if (initialized_)
	{
		initialized_ = false;
	}
	return DEVICE_OK;
}

bool ZStage::Busy()
{
	string answer;   
	std::stringstream command;

	command << ":M" << channelZ_;

	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\n");		
	ret = GetSerialAnswer(port_.c_str(), "\n", answer);
	if (ret != DEVICE_OK){
		return true;
	}

	if(strcmp(answer.substr(3).c_str(),"S") != 0){
		return true;
	}

	return false;
}

//////// Setters
int ZStage::SetErrorReporting(bool reporting){
	string answer;
	std::stringstream command;
	if(reporting){
		command << ":E1";
	} else {
		command << ":E0";
	}

	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\n");	
	ret = GetSerialAnswer(port_.c_str(), "\n", answer);
	if (ret != DEVICE_OK)
		return ret;

	// check for error
	int error; 
	if(isError(answer, &error)){
		return GetErrorStatus(error);
	}

	return DEVICE_OK;
}

int ZStage::SetPositionUm(double pos)
{
	// round to first decimal	
	double npos = ceil(pos*10)/10;

	std::stringstream command;
	command << ":MPA" << channelZ_ << "P" << npos*reverseZ_ << "H" << holdtime_;
	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\n");
	if (ret != DEVICE_OK){
		return ret;
	}
	
	// check for answer
	string answer;
	ret = GetSerialAnswer(port_.c_str(), "\n", answer);
	if (ret != DEVICE_OK)
		return ret;

	// is it an error?
	int error; 
	if(isError(answer, &error)){
		return GetErrorStatus(error);
	}

	return DEVICE_OK;
}

int ZStage::SetRelativePositionUm(double pos)
{
	// round to first decimal	
	double npos = ceil(pos*10)/10;

	std::stringstream command;
	command << ":MPR" << channelZ_ << "P" << npos*reverseZ_ << "H" << holdtime_;
	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\n");
	if (ret != DEVICE_OK){
		return ret;
	}

	// check for answer
	string answer;
	ret = GetSerialAnswer(port_.c_str(), "\n", answer);
	if (ret != DEVICE_OK)
		return ret;

	// is it an error?
	int error; 
	if(isError(answer, &error)){
		return GetErrorStatus(error);
	}

	return DEVICE_OK;
}

int ZStage::SetOrigin()
{
	std::stringstream command;
	command << ":SZ" << channelZ_;
	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\n");
	if (ret != DEVICE_OK)
		return ret;
	return DEVICE_OK;
}

int ZStage::SetFrequency(int freq)
{
	std::stringstream command;
	command << ":SCLF" << channelZ_ << "F" << freq;
	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\n");
	if (ret != DEVICE_OK)
		return ret;

	return DEVICE_OK;
}

//////// Getters

int ZStage::GetPositionUm(double& pos)
{
	string answer;
	std::stringstream command;
	command << ":GP" << channelZ_;

	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\n");	
	ret = GetSerialAnswer(port_.c_str(), "\n", answer);
	if (ret != DEVICE_OK){	
		return ret;
	}

	// is the answer an error?
	int error; 
	if(isError(answer, &error)){
		return GetErrorStatus(error);
	}

	// else, extract position
	pos = atof(answer.substr(4).c_str());

	return DEVICE_OK;
}

int ZStage::GetController(std::string* controller)
{
	string answer;
	std::stringstream command;
	command << ":I";

	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\n");	
	ret = GetSerialAnswer(port_.c_str(), "\n", answer);
	if (ret != DEVICE_OK){	
		return ret;
	}

	// is the answer an error?
	int error; 
	if(isError(answer, &error)){
		return GetErrorStatus(error);
	}

	if(answer.find("SmarAct HCU-3D") != std::string::npos){
		*controller = "SmarAct HCU-3D";
	} else if(answer.find("SmarAct CU-3D") != std::string::npos){
		*controller = "SmarAct CU-3D";
	} else if(answer.find("SmarAct SCU-3D") != std::string::npos){
		*controller = "SmarAct SCU-3D";
	} else if(answer.find("SmarAct HCU-1D") != std::string::npos){
		*controller = "SmarAct HCU-1D";
	} else if(answer.find("SmarAct CU-1D") != std::string::npos){
		*controller = "SmarAct CU-1D";
	} else if(answer.find("SmarAct SCU-1D") != std::string::npos){
		*controller = "SmarAct SCU-1D";
	} else {
		return ERR_IDENTIFICATION_FAIL; 
	}

	return DEVICE_OK;
}

int ZStage::GetID(std::string* id)
{
	string answer;
	std::stringstream command;
	command << ":GID";

	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\n");	
	ret = GetSerialAnswer(port_.c_str(), "\n", answer);
	if (ret != DEVICE_OK){	
		return ret;
	}

	// is the answer an error?
	int error; 
	if(isError(answer, &error)){
		return GetErrorStatus(error);
	}

	*id = answer.substr(3);

	return DEVICE_OK;
}

/////////////////////////////////////////////////////////////////
/////////////// Unsupported commands ////////////////////////////

int ZStage::SetPositionSteps(long /* pos */)
{

	return DEVICE_UNSUPPORTED_COMMAND;   
}

int ZStage::GetPositionSteps(long& /* steps */)
{

	return DEVICE_UNSUPPORTED_COMMAND;
}

int ZStage::GetLimits(double& /*min*/, double& /*max*/)
{
	return DEVICE_UNSUPPORTED_COMMAND;
}

///////////////////////////////////////////////////////////////////////////////
// Action handlers
///////////////////////////////////////////////////////////////////////////////
int ZStage::OnFrequency(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{  
		pProp->Set((long)freqZ_);
	}
	else if (eAct == MM::AfterSet)
	{
		long pos;
		pProp->Get(pos);
		freqZ_ = pos;
		SetFrequency(freqZ_);
	}

	return DEVICE_OK;
}

int ZStage::OnPort(MM::PropertyBase* pProp, MM::ActionType eAct)								
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(port_.c_str());
	}
	else if (eAct == MM::AfterSet)
	{
		if (initialized_)
		{
			// revert
			pProp->Set(port_.c_str());
			return ERR_PORT_CHANGE_FORBIDDEN;
		}

		pProp->Get(port_);
	}

	return DEVICE_OK;
}
