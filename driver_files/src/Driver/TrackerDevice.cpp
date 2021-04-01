#include "TrackerDevice.hpp"
#include <Windows.h>

ExampleDriver::TrackerDevice::TrackerDevice(std::string serial, HANDLE pipe, int deviceId):
    serial_(serial), hpipe(pipe)
{
	this->deviceId = deviceId;
    this->last_pose_ = MakeDefaultPose();
    this->isSetup = false;
}

std::string ExampleDriver::TrackerDevice::GetSerial()
{
    return this->serial_;
}

void ExampleDriver::TrackerDevice::Update()
{
    if (this->device_index_ == vr::k_unTrackedDeviceIndexInvalid)
        return;

    // Check if this device was asked to be identified
    auto events = GetDriver()->GetOpenVREvents();
    for (auto event : events) {
        // Note here, event.trackedDeviceIndex does not necissarily equal this->device_index_, not sure why, but the component handle will match so we can just use that instead
        //if (event.trackedDeviceIndex == this->device_index_) {
        if (event.eventType == vr::EVREventType::VREvent_Input_HapticVibration) {
            if (event.data.hapticVibration.componentHandle == this->haptic_component_) {
                this->did_vibrate_ = true;
            }
        }
        //}
    }

    // Check if we need to keep vibrating
    if (this->did_vibrate_) {
        this->vibrate_anim_state_ += (GetDriver()->GetLastFrameTime().count()/1000.f);
        if (this->vibrate_anim_state_ > 1.0f) {
            this->did_vibrate_ = false;
            this->vibrate_anim_state_ = 0.0f;
        }
    }

    // Setup pose for this frame
    auto pose = this->last_pose_;
    
    if (PeekNamedPipe(hpipe, NULL, 0, NULL, &dwRead, NULL) != FALSE)
    {
        //if data is ready,
        if (dwRead > 0)
        {
            //we go and read it into our buffer
            if (ReadFile(hpipe, buffer, sizeof(buffer) - 1, &dwRead, NULL) != FALSE)
            {

                buffer[dwRead] = '\0'; //add terminating zero
                //convert our buffer to string
                std::string s = buffer;

                //first three variables are a position vector
                double a;
                double b;
                double c;

                //second four are rotation quaternion
                double qw;
                double qx;
                double qy;
                double qz;

                //convert to string stream
                std::istringstream iss(s);

                //read to our variables
                iss >> a;
                iss >> b;
                iss >> c;
                iss >> qw;
                iss >> qx;
                iss >> qy;
                iss >> qz;

                //send the new position and rotation from the pipe to the tracker object
                pose.vecPosition[0] = a;
                pose.vecPosition[1] = b;
                pose.vecPosition[2] = c;

                pose.qRotation.w = qw;
                pose.qRotation.x = qx;
                pose.qRotation.y = qy;
                pose.qRotation.z = qz;

                // Post pose
                GetDriver()->GetDriverHost()->TrackedDevicePoseUpdated(this->device_index_, pose, sizeof(vr::DriverPose_t));
                this->last_pose_ = pose;
            }
        }
    } 
    

}

DeviceType ExampleDriver::TrackerDevice::GetDeviceType()
{
    return DeviceType::TRACKER;
}

vr::TrackedDeviceIndex_t ExampleDriver::TrackerDevice::GetDeviceIndex()
{
    return this->device_index_;
}

vr::EVRInitError ExampleDriver::TrackerDevice::Activate(uint32_t unObjectId)
{
    this->device_index_ = unObjectId;

    GetDriver()->Log("Activating tracker " + this->serial_);

    // Get the properties handle
    auto props = GetDriver()->GetProperties()->TrackedDeviceToPropertyContainer(this->device_index_);

    // Setup inputs and outputs
    GetDriver()->GetInput()->CreateHapticComponent(props, "/output/haptic", &this->haptic_component_);

    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/system/click", &this->system_click_component_);
    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/system/touch", &this->system_touch_component_);

    // Set some universe ID (Must be 2 or higher)
    GetDriver()->GetProperties()->SetUint64Property(props, vr::Prop_CurrentUniverseId_Uint64, 2);
    
    // Set up a model "number" (not needed but good to have)
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_ModelNumber_String, "Vive Tracker Pro MV");

    // Opt out of hand selection
	GetDriver()->GetProperties()->SetInt32Property(props, vr::Prop_ControllerRoleHint_Int32, vr::ETrackedControllerRole::TrackedControllerRole_OptOut);

    // Set up a render model path
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_RenderModelName_String, "vr_controller_05_wireless_b");

    // Set controller profile
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_InputProfilePath_String, "{example}/input/example_tracker_bindings.json");

    // Set the icon
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceReady_String, "{example}/icons/tracker_ready.png");

    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceOff_String, "{example}/icons/tracker_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceSearching_String, "{example}/icons/tracker_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceSearchingAlert_String, "{example}/icons/tracker_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceReadyAlert_String, "{example}/icons/tracker_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceNotReady_String, "{example}/icons/tracker_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceStandby_String, "{example}/icons/tracker_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceAlertLow_String, "{example}/icons/tracker_not_ready.png");

	switch (deviceId)
	{
	case 0:
		GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_ControllerType_String, "vive_tracker_waist");
		break;
	case 1:
		GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_ControllerType_String, "vive_tracker_left_foot");
		break;
	case 2:
		GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_ControllerType_String, "vive_tracker_right_foot");
		break;
	}

    return vr::EVRInitError::VRInitError_None;
}

void ExampleDriver::TrackerDevice::Deactivate()
{
    this->device_index_ = vr::k_unTrackedDeviceIndexInvalid;
}

void ExampleDriver::TrackerDevice::EnterStandby()
{
}

void* ExampleDriver::TrackerDevice::GetComponent(const char* pchComponentNameAndVersion)
{
    return nullptr;
}

void ExampleDriver::TrackerDevice::DebugRequest(const char* pchRequest, char* pchResponseBuffer, uint32_t unResponseBufferSize)
{
    if (unResponseBufferSize >= 1)
        pchResponseBuffer[0] = 0;
}

vr::DriverPose_t ExampleDriver::TrackerDevice::GetPose()
{
    return last_pose_;
}

