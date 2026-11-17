// Cinemacam's camera hand-off API, copied from Cinemacam's api/CinemacamAPI.h (MIT licensed, unlike the rest of Cinemacam)
// SPDX-License-Identifier: MIT
#pragma once
#include <Windows.h>
namespace Cinemacam::API{
inline constexpr int kVersion=1;
using GetApiVersionFn=int(*)();
using RequestCameraFn=bool(*)(const char* modName);
using ReleaseCameraFn=bool(*)(const char* modName);
using IsCameraBorrowedFn=bool(*)();
using IsCameraYoursFn=bool(*)(const char* modName);
struct Functions{
 int version=0;
 RequestCameraFn RequestCamera=nullptr;
 ReleaseCameraFn ReleaseCamera=nullptr;
 IsCameraBorrowedFn IsCameraBorrowed=nullptr;
 IsCameraYoursFn IsCameraYours=nullptr; // optional: null before Cinemacam 2.2.3
 explicit operator bool()const{return version==kVersion&&RequestCamera&&ReleaseCamera&&IsCameraBorrowed;}
};
// Looks the functions up in the loaded Cinemacam.dll. Empty (false) if
// Cinemacam isn't loaded or its API version isn't the one this file is for.
inline Functions Get(){
 Functions f;
 HMODULE dll=GetModuleHandleW(L"Cinemacam.dll");if(!dll)return f;
 auto version=reinterpret_cast<GetApiVersionFn>(GetProcAddress(dll,"Cinemacam_GetApiVersion"));if(!version)return f;
 f.version=version();if(f.version!=kVersion)return f;
 f.RequestCamera=reinterpret_cast<RequestCameraFn>(GetProcAddress(dll,"Cinemacam_RequestCamera"));
 f.ReleaseCamera=reinterpret_cast<ReleaseCameraFn>(GetProcAddress(dll,"Cinemacam_ReleaseCamera"));
 f.IsCameraBorrowed=reinterpret_cast<IsCameraBorrowedFn>(GetProcAddress(dll,"Cinemacam_IsCameraBorrowed"));
 f.IsCameraYours=reinterpret_cast<IsCameraYoursFn>(GetProcAddress(dll,"Cinemacam_IsCameraYours"));
 return f;
}
}
