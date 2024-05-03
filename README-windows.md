# build for windows

## Requirements

- Windows 11
- Powershell 7
- Visual Studio 2022 Community
- scoop
  - cmake 3.29.2 64bit
  - gsudo
- Git for Windows 2.44.0.1
- [JACK 1.9.22 win64](https://github.com/jackaudio/jack2-releases/releases/download/v1.9.22/jack2-win64-v1.9.22.exe) JACK Audio Connection Kit for Windows 64bit

## Prepare

```powershell
scoop install gsudo
```

Create working directory anywhere.

```powershell
mkdir C:\opt\el
cd C:\opt\el
```

Clone this repository and enter the directory.

```powershell
git clone --recursive https://github.com/daeosqu/laserdock_jack.git
cd C:\opt\el\laserdock_jack
```

## Run cmake generator

Create build directory and run cmake generator.

```powershell
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="$env:USERPROFILE/scoop/apps/vcpkg/current/scripts/buildsystems/vcpkg.cmake"
```

## Build

Build laserdock_jack.

```powershell
cmake --build . --config Release --target clean
cmake --build . --config Release
```

## Install

Install program.

```powershell
gsudo cmake --build . --config Release --target install
```

## Run

Connect your LaserCube and run jack host application.

```powershell
. "C:/Program Files/lasershark_hostapp/bin/laserdock_jack.exe"
```

## Build log

```powershell
PS C:\opt\el\laserdock_jack> mkdir build
PS C:\opt\el\laserdock_jack> cd build


PS C:\opt\el\laserdock_jack\build> cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="$env:USERPROFILE/scoop/apps/vcpkg/current/scripts/buildsystems/vcpkg.cmake"
-- Selecting Windows SDK version 10.0.22621.0 to target Windows 10.0.22631.
-- The C compiler identification is MSVC 19.39.33523.0
-- The CXX compiler identification is MSVC 19.39.33523.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.39.33519/bin/Hostx64/x64/cl.exe - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.39.33519/bin/Hostx64/x64/cl.exe - skipped
-- Detecting CXX compile features
-- Detecting CXX compile features - done
Build type: Release
-- Could NOT find Libusb (missing: LIBUSB_LIBRARY LIBUSB_INCLUDE_DIR)
-- downloading libusb 1.0.22

7-Zip 23.01 (x64) : Copyright (c) 1999-2023 Igor Pavlov : 2023-06-20

Scanning the drive for archives:
1 file, 980895 bytes (958 KiB)

Extracting archive: C:\opt\el\laserdock_jack\build\libusb-1.0.22.7z
--
Path = C:\opt\el\laserdock_jack\build\libusb-1.0.22.7z
Type = 7z
Physical Size = 980895
Headers Size = 682
Method = LZMA2:23 BCJ
Solid = +
Blocks = 2

Everything is Ok

Folders: 18
Files: 32
Size:       6294517
Compressed: 980895
-- Found Libusb: C:/opt/el/laserdock_jack/build/3thparty/libusb-1.0.22/MS64/dll/libusb-1.0.lib
-- Found JACK: C:/Program Files/JACK2/lib/libjack64.lib
LIBUSB_DLL=C:/opt/el/laserdock_jack/build/3thparty/libusb-1.0.22/MS64/dll/libusb-1.0.dll
LIBUSB_DLL=C:/opt/el/laserdock_jack/build/3thparty/libusb-1.0.22/MS64/dll/libusb-1.0.lib
-- Configuring done (9.7s)
-- Generating done (0.0s)
-- Build files have been written to: C:/opt/el/laserdock_jack/build


PS C:\opt\el\laserdock_jack\build> cmake --build . --config Release --target clean
MSBuild のバージョン 17.9.8+b34f75857 (.NET Framework)


PS C:\opt\el\laserdock_jack\build> cmake --build . --config Release
MSBuild のバージョン 17.9.8+b34f75857 (.NET Framework)

  1>Checking Build System
  Building Custom Rule C:/opt/el/laserdock_jack/CMakeLists.txt
  laserdock_jack.c

C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\um\winioctl.h(10853,22): warning C4668: '_WIN32_WINNT_WIN10_RS1' is not defined as a preprocessor macro, replacing with '0' for '#if/#elif' [C:\opt\el\laserdock_jack\build\laserdock_jack.vcxproj]  (compiling source file '../laserdock_jack.c')

...


PS C:\opt\el\laserdock_jack\build> gsudo cmake --build . --config Release --target install
MSBuild のバージョン 17.9.8+b34f75857 (.NET Framework)

  laserdock_jack.vcxproj -> C:\opt\el\laserdock_jack\build\Release\laserdock_jack.exe
  lasershark_stdin_circlemaker.vcxproj -> C:\opt\el\laserdock_jack\build\Release\lasershark_stdin_circlemaker.exe
  1>
  -- Install configuration: "Release"
  -- Installing: C:/Program Files/lasershark_hostapp/bin/laserdock_jack.exe
  -- Installing: C:/Program Files/lasershark_hostapp/bin/libusb-1.0.dll
```
