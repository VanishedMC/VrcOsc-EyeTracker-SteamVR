// MIT License
//
// Copyright(c) 2025 Matthieu Bucchianeri
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "pch.h"

#include "ShimDriverManager.h"
#include "DetourUtils.h"
#include "Tracing.h"

#include <osc/OscReceivedElements.h>
#include <osc/OscPacketListener.h>
#include <ip/UdpSocket.h>

namespace {
    using namespace driver_shim;

    struct EyeTrackerNotSupportedException : public std::exception {
        const char* what() const throw() {
            return "Eye tracker is not supported";
        }
    };

    // The HmdShimDriver driver wraps another ITrackedDeviceServerDriver instance with the intent to override
    // properties and behaviors.
    struct HmdShimDriver : public vr::ITrackedDeviceServerDriver, osc::OscPacketListener {
        HmdShimDriver(vr::ITrackedDeviceServerDriver* shimmedDevice)
            : m_shimmedDevice(shimmedDevice), m_socket(IpEndpointName(IpEndpointName::ANY_ADDRESS, 9020), this) {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "HmdShimDriver_Ctor");

            // TODO: Add any early initialization here if needed.

            // TODO: Add capabilities detection (if not already done in Driver::Init() earlier) and throw
            // EyeTrackerNotSupportedException to skip shimming when capabilities are not available.

            TraceLoggingWriteStop(local, "HmdShimDriver_Ctor");
        }

        vr::EVRInitError Activate(uint32_t unObjectId) override {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "HmdShimDriver_Activate", TLArg(unObjectId, "ObjectId"));

            // Activate the real device driver.
            m_shimmedDevice->Activate(unObjectId);

            m_deviceIndex = unObjectId;

            const vr::PropertyContainerHandle_t container =
                vr::VRProperties()->TrackedDeviceToPropertyContainer(m_deviceIndex);

            // Advertise supportsEyeGazeInteraction.
            vr::VRProperties()->SetBoolProperty(container, vr::Prop_SupportsXrEyeGazeInteraction_Bool, true);

            // Create the input component for the eye gaze. It must have the path /eyetracking and nothing else!
            vr::VRDriverInput()->CreateEyeTrackingComponent(container, "/eyetracking", &m_eyeTrackingComponent);
            TraceLoggingWriteTagged(
                local, "HmdShimDriver_Activate", TLArg(m_eyeTrackingComponent, "EyeTrackingComponent"));
            DriverLog("Eye Gaze Component: %lld", m_eyeTrackingComponent);

            // Schedule updates in a background thread.
            m_active = true;
            m_listeningThread = std::thread([&]() { m_socket.Run(); });

            TraceLoggingWriteStop(local, "HmdShimDriver_Activate");

            return vr::VRInitError_None;
        }

        void Deactivate() override {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "HmdShimDriver_Deactivate", TLArg(m_deviceIndex, "ObjectId"));

            if (m_active.exchange(false)) {
                m_socket.AsynchronousBreak();
                m_listeningThread.join();
            }

            m_deviceIndex = vr::k_unTrackedDeviceIndexInvalid;

            m_shimmedDevice->Deactivate();

            DriverLog("Deactivated device shimmed with HmdShimDriver");

            TraceLoggingWriteStop(local, "HmdShimDriver_Deactivate");
        }

        void EnterStandby() override {
            m_shimmedDevice->EnterStandby();
        }

        void* GetComponent(const char* pchComponentNameAndVersion) override {
            return m_shimmedDevice->GetComponent(pchComponentNameAndVersion);
        }

        vr::DriverPose_t GetPose() override {
            return m_shimmedDevice->GetPose();
        }

        void DebugRequest(const char* pchRequest, char* pchResponseBuffer, uint32_t unResponseBufferSize) override {
            m_shimmedDevice->DebugRequest(pchRequest, pchResponseBuffer, unResponseBufferSize);
        }

        void ProcessMessage(const osc::ReceivedMessage& m, const IpEndpointName& remoteEndpoint) override {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "HmdShimDriver_OscMessage");
            vr::VREyeTrackingData_t data{};

            try {
                if (std::string_view(m.AddressPattern()) == "/tracking/eye/LeftRightPitchYaw") {
                    osc::ReceivedMessageArgumentStream args = m.ArgumentStream();

                    float leftPitch;
                    float leftYaw;
                    float rightPitch;
                    float rightYaw;
                    args >> leftPitch >> leftYaw >> rightPitch >> rightYaw >> osc::EndMessage;

                    // Convert degrees to radians for trigonometric functions
                    // Need to invert pitch because that's what mbucchia's code wants
                    const float leftPitchRad = leftPitch * M_PI / 180.0f * -1.0f;
                    const float leftYawRad = leftYaw * M_PI / 180.0f;
                    const float rightPitchRad = rightPitch * M_PI / 180.0f * -1.0f;
                    const float rightYawRad = rightYaw * M_PI / 180.0f;

                    // Use polar coordinates to create a unit vector.
                    DirectX::XMStoreFloat3(
                        (DirectX::XMFLOAT3*)&data.vGazeTarget,
                        DirectX::XMVector3Normalize(DirectX::XMVectorSet(
                            (sin(leftYawRad) * cos(leftPitchRad) + sin(rightYawRad) * cos(rightPitchRad)) / 2,
                            (sin(leftPitchRad) + sin(rightPitchRad)) / 2,
                            (-cos(leftYawRad) * cos(leftPitchRad) - cos(rightYawRad) * cos(rightPitchRad)) / 2,
                            1
                        )
                    ));

                    data.bValid = data.bTracked = data.bActive = true;

                    /*TraceLoggingWriteTagged(local,
                                            "HmdShimDriver_OscEyeTrackingInfo",
                                            TLArg(leftPitch, "LeftPitch"),
                                            TLArg(leftYaw, "LeftYaw"),
                                            TLArg(rightPitch, "RightPitch"),
                                            TLArg(rightYaw, "RightYaw"));*/

                    if (!(std::isnan(leftPitch) || std::isnan(leftYaw) || std::isnan(rightPitch) ||
                          std::isnan(rightYaw))) {
                        std::unique_lock lock(m_mutex);
                    }
                }
            } catch (osc::Exception& e) {
                TraceLoggingWriteTagged(local, "VRChatOSCEyeTracker_ProcessMessage", TLArg(e.what(), "Error"));
                //  Fallback to identity.
                DirectX::XMStoreFloat3((DirectX::XMFLOAT3*)&data.vGazeTarget, DirectX::XMVectorSet(0, 0, -1, 1));
                data.bValid = data.bTracked = data.bActive = false;
            }
            vr::VRDriverInput()->UpdateEyeTrackingComponent(m_eyeTrackingComponent, &data, 0.f);
            TraceLoggingWriteStop(local, "HmdShimDriver_OscMessage");
        }

        vr::ITrackedDeviceServerDriver* const m_shimmedDevice;
        std::thread m_listeningThread;
        UdpListeningReceiveSocket m_socket;
        mutable std::mutex m_mutex;

        vr::TrackedDeviceIndex_t m_deviceIndex = vr::k_unTrackedDeviceIndexInvalid;

        std::atomic<bool> m_active = false;
        
        vr::VRInputComponentHandle_t m_eyeTrackingComponent = 0;
    };
} // namespace

namespace driver_shim {

    vr::ITrackedDeviceServerDriver* CreateHmdShimDriver(vr::ITrackedDeviceServerDriver* shimmedDriver) {
        try {
            return new HmdShimDriver(shimmedDriver);
        } catch (EyeTrackerNotSupportedException&) {
            return shimmedDriver;
        }
    }

} // namespace driver_shim
