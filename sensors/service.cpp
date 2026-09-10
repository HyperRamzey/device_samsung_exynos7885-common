#define LOG_TAG "android.hardware.sensors@1.0-service_32"

#include <android/hardware/sensors/1.0/ISensors.h>
#include <hidl/LegacySupport.h>
#include <log/log.h>

using android::hardware::sensors::V1_0::ISensors;

int main() {
    /* Framework needs the sensors HAL to stay alive: SystemServer blocks on
     * waitForSensorService() and A16 init escalates UPDATABLE_CRASHING reboots
     * when this process exits repeatedly (observed exit-255 every ~7s).
     * defaultPassthroughServiceImplementation exits -1 when the underlying
     * hw module init fails - log the reason and park instead of exiting. */
    int ret = android::hardware::defaultPassthroughServiceImplementation<ISensors>(2);
    ALOGE("sensors passthrough service returned %d - parking (no exit)", ret);

    /* Keep the process alive so init does not crash-loop the boot; the HAL
     * interface is simply absent and frameworks handle that gracefully. */
    for (;;) pause();
}
