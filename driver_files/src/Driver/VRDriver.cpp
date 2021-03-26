#include "VRDriver.hpp"
#include <Driver/HMDDevice.hpp>
#include <Driver/TrackerDevice.hpp>
#include <Driver/ControllerDevice.hpp>
#include <Driver/TrackingReferenceDevice.hpp>

vr::EVRInitError ExampleDriver::VRDriver::Init(vr::IVRDriverContext* pDriverContext)
{
    // Perform driver context initialisation
    if (vr::EVRInitError init_error = vr::InitServerDriverContext(pDriverContext); init_error != vr::EVRInitError::VRInitError_None) {
        return init_error;
    }

    Log("Activating AprilTag Driver...");

    // Add a HMD
    //this->AddDevice(std::make_shared<HMDDevice>("Example_HMDDevice"));

    // Add a couple controllers
    //this->AddDevice(std::make_shared<ControllerDevice>("Example_ControllerDevice_Left", ControllerDevice::Handedness::LEFT));
    //this->AddDevice(std::make_shared<ControllerDevice>("Example_ControllerDevice_Right", ControllerDevice::Handedness::RIGHT));
    
    std::string hmdPipeName = "\\\\.\\pipe\\HMDPipe";

    //open the pipe
    hmdPipe = CreateFileA(hmdPipeName.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);

    if (hmdPipe == INVALID_HANDLE_VALUE)
    {
        //if connection was unsuccessful, return an error. This means SteamVR will start without this driver running
        return vr::EVRInitError::VRInitError_Driver_Failed;
    }
    //wait for a second to ensure data was sent and next pipe is set up if there is more than one tracker

    Sleep(1000);
    
    // Add a tracker
    char buffer[1024];
    DWORD dwWritten;
    DWORD dwRead;
    
    //on init, we try to connect to our pipes
    for (int i = 0; i < pipeNum; i++)
    {
        //MessageBoxA(NULL, "It works!  " + pipeNum, "Example Driver", MB_OK);
        HANDLE pipe;
        //pipe name, same as in our server program
        std::string pipeName = "\\\\.\\pipe\\TrackPipe" + std::to_string(i);

        //open the pipe
        pipe = CreateFileA(pipeName.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);

        if (pipe == INVALID_HANDLE_VALUE)
        {
            //if connection was unsuccessful, return an error. This means SteamVR will start without this driver running
            return vr::EVRInitError::VRInitError_Driver_Failed;
        }

        //wait for a second to ensure data was sent and next pipe is set up if there is more than one tracker
        Sleep(1000);

        //read the number of pipes and smoothing factor from the pipe
        if (ReadFile(pipe, buffer, sizeof(buffer) - 1, &dwRead, NULL) != FALSE)
        {
            //we receive raw data, so we first add terminating zero and save to a string.
            buffer[dwRead] = '\0'; //add terminating zero
            std::string s = buffer;
            //from a string, we convert to a string stream for easier reading of each sent value
            std::istringstream iss(s);
            //read each value into our variables

            iss >> pipeNum;
            iss >> smoothFactor;
        }
        //save our pipe to global
        this->AddDevice(std::make_shared<TrackerDevice>("AprilTracker"+std::to_string(i),pipe, i));
    }
    
    // Add a couple tracking references
    //this->AddDevice(std::make_shared<TrackingReferenceDevice>("Example_TrackingReference_A"));
    //this->AddDevice(std::make_shared<TrackingReferenceDevice>("Example_TrackingReference_B"));

    Log("AprilTag Driver Loaded Successfully");

	return vr::VRInitError_None;
}

void ExampleDriver::VRDriver::Cleanup()
{
}

void ExampleDriver::VRDriver::RunFrame()
{
    // Collect events
    vr::VREvent_t event;
    std::vector<vr::VREvent_t> events;
    while (vr::VRServerDriverHost()->PollNextEvent(&event, sizeof(event)))
    {
        events.push_back(event);
    }
    this->openvr_events_ = events;

    // Update frame timing
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    this->frame_timing_ = std::chrono::duration_cast<std::chrono::milliseconds>(now - this->last_frame_time_);
    this->last_frame_time_ = now;

    // Update devices
    for (auto& device : this->devices_)
        device->Update();

    vr::TrackedDevicePose_t hmd_pose[10];
    vr::VRServerDriverHost()->GetRawTrackedDevicePoses(0, hmd_pose, 10);

    vr::HmdQuaternion_t q = GetRotation(hmd_pose[0].mDeviceToAbsoluteTracking);
    vr::HmdVector3_t pos = GetPosition(hmd_pose[0].mDeviceToAbsoluteTracking);

    std::string s;
    s = std::to_string(pos.v[0]) +
        " " + std::to_string(pos.v[1]) +
        " " + std::to_string(pos.v[2]) +
        " " + std::to_string(q.w) +
        " " + std::to_string(q.x) +
        " " + std::to_string(q.y) +
        " " + std::to_string(q.z) + "\n";

    DWORD dwWritten;
    WriteFile(hmdPipe,
        s.c_str(),
        (s.length() + 1),   // = length of string + terminating '\0' !!!
        &dwWritten,
        NULL);

}

bool ExampleDriver::VRDriver::ShouldBlockStandbyMode()
{
    return false;
}

void ExampleDriver::VRDriver::EnterStandby()
{
}

void ExampleDriver::VRDriver::LeaveStandby()
{
}

std::vector<std::shared_ptr<ExampleDriver::IVRDevice>> ExampleDriver::VRDriver::GetDevices()
{
    return this->devices_;
}

std::vector<vr::VREvent_t> ExampleDriver::VRDriver::GetOpenVREvents()
{
    return this->openvr_events_;
}

std::chrono::milliseconds ExampleDriver::VRDriver::GetLastFrameTime()
{
    return this->frame_timing_;
}

bool ExampleDriver::VRDriver::AddDevice(std::shared_ptr<IVRDevice> device)
{
    vr::ETrackedDeviceClass openvr_device_class;
    // Remember to update this switch when new device types are added
    switch (device->GetDeviceType()) {
        case DeviceType::CONTROLLER:
            openvr_device_class = vr::ETrackedDeviceClass::TrackedDeviceClass_Controller;
            break;
        case DeviceType::HMD:
            openvr_device_class = vr::ETrackedDeviceClass::TrackedDeviceClass_HMD;
            break;
        case DeviceType::TRACKER:
            openvr_device_class = vr::ETrackedDeviceClass::TrackedDeviceClass_GenericTracker;
            break;
        case DeviceType::TRACKING_REFERENCE:
            openvr_device_class = vr::ETrackedDeviceClass::TrackedDeviceClass_TrackingReference;
            break;
        default:
            return false;
    }
    bool result = vr::VRServerDriverHost()->TrackedDeviceAdded(device->GetSerial().c_str(), openvr_device_class, device.get());
    if(result)
        this->devices_.push_back(device);
    return result;
}

ExampleDriver::SettingsValue ExampleDriver::VRDriver::GetSettingsValue(std::string key)
{
    vr::EVRSettingsError err = vr::EVRSettingsError::VRSettingsError_None;
    int int_value = vr::VRSettings()->GetInt32(settings_key_.c_str(), key.c_str(), &err);
    if (err == vr::EVRSettingsError::VRSettingsError_None) {
        return int_value;
    }
    err = vr::EVRSettingsError::VRSettingsError_None;
    float float_value = vr::VRSettings()->GetFloat(settings_key_.c_str(), key.c_str(), &err);
    if (err == vr::EVRSettingsError::VRSettingsError_None) {
        return float_value;
    }
    err = vr::EVRSettingsError::VRSettingsError_None;
    bool bool_value = vr::VRSettings()->GetBool(settings_key_.c_str(), key.c_str(), &err);
    if (err == vr::EVRSettingsError::VRSettingsError_None) {
        return bool_value;
    }
    std::string str_value;
    str_value.reserve(1024);
    vr::VRSettings()->GetString(settings_key_.c_str(), key.c_str(), str_value.data(), 1024, &err);
    if (err == vr::EVRSettingsError::VRSettingsError_None) {
        return str_value;
    }
    err = vr::EVRSettingsError::VRSettingsError_None;

    return SettingsValue();
}

void ExampleDriver::VRDriver::Log(std::string message)
{
    std::string message_endl = message + "\n";
    vr::VRDriverLog()->Log(message_endl.c_str());
}

vr::IVRDriverInput* ExampleDriver::VRDriver::GetInput()
{
    return vr::VRDriverInput();
}

vr::CVRPropertyHelpers* ExampleDriver::VRDriver::GetProperties()
{
    return vr::VRProperties();
}

vr::IVRServerDriverHost* ExampleDriver::VRDriver::GetDriverHost()
{
    return vr::VRServerDriverHost();
}

//-----------------------------------------------------------------------------
// Purpose: Calculates quaternion (qw,qx,qy,qz) representing the rotation
// from: https://github.com/Omnifinity/OpenVR-Tracking-Example/blob/master/HTC%20Lighthouse%20Tracking%20Example/LighthouseTracking.cpp
//-----------------------------------------------------------------------------

vr::HmdQuaternion_t ExampleDriver::VRDriver::GetRotation(vr::HmdMatrix34_t matrix) {
    vr::HmdQuaternion_t q;

    q.w = sqrt(fmax(0, 1 + matrix.m[0][0] + matrix.m[1][1] + matrix.m[2][2])) / 2;
    q.x = sqrt(fmax(0, 1 + matrix.m[0][0] - matrix.m[1][1] - matrix.m[2][2])) / 2;
    q.y = sqrt(fmax(0, 1 - matrix.m[0][0] + matrix.m[1][1] - matrix.m[2][2])) / 2;
    q.z = sqrt(fmax(0, 1 - matrix.m[0][0] - matrix.m[1][1] + matrix.m[2][2])) / 2;
    q.x = copysign(q.x, matrix.m[2][1] - matrix.m[1][2]);
    q.y = copysign(q.y, matrix.m[0][2] - matrix.m[2][0]);
    q.z = copysign(q.z, matrix.m[1][0] - matrix.m[0][1]);
    return q;
}
//-----------------------------------------------------------------------------
// Purpose: Extracts position (x,y,z).
// from: https://github.com/Omnifinity/OpenVR-Tracking-Example/blob/master/HTC%20Lighthouse%20Tracking%20Example/LighthouseTracking.cpp
//-----------------------------------------------------------------------------

vr::HmdVector3_t ExampleDriver::VRDriver::GetPosition(vr::HmdMatrix34_t matrix) {
    vr::HmdVector3_t vector;

    vector.v[0] = matrix.m[0][3];
    vector.v[1] = matrix.m[1][3];
    vector.v[2] = matrix.m[2][3];

    return vector;
}
