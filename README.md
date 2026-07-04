# FUCK.dll
https://www.nexusmods.com/skyrimspecialedition/mods/181603

## Requirements
* [CMake](https://cmake.org/)
	* Add this to your `PATH`
* [PowerShell](https://github.com/PowerShell/PowerShell/releases/latest)
* [Vcpkg](https://github.com/microsoft/vcpkg)
	* Add the environment variable `VCPKG_ROOT` with the value as the path to the folder containing vcpkg
* [Visual Studio Community 2022](https://visualstudio.microsoft.com/)
	* Desktop development with C++
* [CommonLibVR](https://github.com/alandtse/CommonLibVR)
	* The default (`ng`, CommonLibSSE-NG based) branch; initialize its `extern/openvr` submodule
	* Add this as as an environment variable `CommonLibVRPath`

## Register Visual Studio as a Generator
* Open `x64 Native Tools Command Prompt`
* Run `cmake`
* Close the cmd window

## Building
```
git clone https://github.com/Fuzzlesz/FUCK.git
cd FUCK
# pull commonlib /extern to override the path settings
git submodule init
# to update submodules to checked in build
git submodule update
```

### SSE / AE / VR (one DLL)
```
cmake --preset vs2022-windows-vcpkg
cmake --build build --config Release
```
The DLL targets all runtimes and picks addresses at load time via the address library.

In Skyrim VR the menu needs [ImGuiVRHelper](https://github.com/alandtse/imgui-vr-helper) installed in-game; it is mirrored into the helper's in-scene panel and driven with the wand. Without the helper the menu draws to the flat mirror window only.
## License
[MIT](LICENSE)
