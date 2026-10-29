/*
 * Copyright (C) 2021 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "SamsungCameraProvider@2.5"

#include "SamsungCameraProvider.h"

#include <algorithm>
#include <cinttypes>
#include <climits>
#include <cstdlib>

#include <cutils/properties.h>

using ::android::NO_ERROR;
using ::android::OK;

using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Void;

const int kMaxCameraIdLen = 16;

/*
 * DERP: runtime camera-ID probe for the aux rear sensors.
 *
 * The A30s registers imx576/hi1631/4ha/gc5035 in the DT (dtbo/exynos7904-a30s_camera.dtsi)
 * but the shipped HAL publishes only one rear and one front camera, so the provider's
 * extra-ID list is the only userspace lever to ask the HAL for more IDs. Instead of
 * recompiling to try candidates, read them from a property:
 *
 *   setprop persist.vendor.camera.derp_extra_ids 1,2,3,4,10,11
 *
 * Every ID is accepted only if mModule->getCameraInfo(id) succeeds, exactly like the
 * compile-time EXTRA_IDS list, so an ID the HAL does not know is simply skipped. When
 * the property is absent nothing is added and the default camera set is unchanged.
 */
static const char kPropDerpExtraIds[] = "persist.vendor.camera.derp_extra_ids";

static std::vector<int> parseIdList(const char *list) {
    std::vector<int> ids;
    const char *p = list;
    while (p != nullptr && *p != '\0') {
        char *end = nullptr;
        long value = strtol(p, &end, 10);
        if (end != p && value >= 0 && value <= INT32_MAX) {
            ids.push_back(static_cast<int>(value));
        }
        p = end;
        while (*p == ',' || *p == ' ' || *p == '\t' || *p == '\n') {
            p++;
        }
    }
    return ids;
}

SamsungCameraProvider::SamsungCameraProvider()
    : LegacyCameraProviderImpl_2_5() {

    mExtraIDs = {
#ifdef EXTRA_IDS
        EXTRA_IDS
#endif
    };

  mExtraIDs.push_back(23);
  mExtraIDs.push_back(52);

  char derpExtraIds[PROPERTY_VALUE_MAX] = {0};
  if (property_get(kPropDerpExtraIds, derpExtraIds, nullptr) > 0) {
    for (int id : parseIdList(derpExtraIds)) {
      if (std::find(mExtraIDs.begin(), mExtraIDs.end(), id) == mExtraIDs.end()) {
        mExtraIDs.push_back(id);
      }
    }
  }

  if (!mInitFailed) {
    for (int i : mExtraIDs) {
      struct camera_info info;
      auto rc = mModule->getCameraInfo(i, &info);

      if (rc != NO_ERROR) {
        continue;
      }

      if (checkCameraVersion(i, info) != OK) {
        ALOGE("Camera version check failed!");
        mModule.clear();
        mInitFailed = true;
        return;
      }

#ifdef SAMSUNG_CAMERA_DEBUG
      ALOGI("ID=%d is at index %d", i, mNumberOfLegacyCameras);
#endif

      char cameraId[kMaxCameraIdLen];
      snprintf(cameraId, sizeof(cameraId), "%d", i);
      std::string cameraIdStr(cameraId);
      mCameraStatusMap[cameraIdStr] = CAMERA_DEVICE_STATUS_PRESENT;

      addDeviceNames(i);
      mNumberOfLegacyCameras++;
    }
  }
}
