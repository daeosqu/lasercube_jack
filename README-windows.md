# build for windows

```
PS C:\opt\el\laserdock_jack> mkdir build
PS C:\opt\el\laserdock_jack> cd build
PS C:\opt\el\laserdock_jack\build> cmake .. -G "Visual Studio 17 2022" -A Win32 -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:/Users/daisuke/scoop/apps/vcpkg/current/scripts/buildsystems/vcpkg.cmake
-- Selecting Windows SDK version 10.0.22621.0 to target Windows 10.0.22631.
-- The C compiler identification is MSVC 19.39.33523.0
-- The CXX compiler identification is MSVC 19.39.33523.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.39.33519/bin/Hostx64/x86/cl.exe - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.39.33519/bin/Hostx64/x86/cl.exe - skipped
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
-- Found Libusb: C:/opt/el/laserdock_jack/build/3thparty/libusb-1.0.22/MS32/dll/libusb-1.0.lib
-- Found JACK: C:/Program Files/JACK2/lib32/libjack.lib
LIBUSB_DLL=C:/opt/el/laserdock_jack/build/3thparty/libusb-1.0.22/MS32/dll/libusb-1.0.dll
LIBUSB_DLL=C:/opt/el/laserdock_jack/build/3thparty/libusb-1.0.22/MS32/dll/libusb-1.0.lib
-- Configuring done (8.6s)
-- Generating done (0.1s)
-- Build files have been written to: C:/opt/el/laserdock_jack/build

PS C:\opt\el\laserdock_jack\build> gsudo cmake --build . --config Release --target install
MSBuild のバージョン 17.9.8+b34f75857 (.NET Framework)

  laserdock_jack.vcxproj -> C:\opt\el\laserdock_jack\build\Release\laserdock_jack.exe
  lasershark_stdin_circlemaker.vcxproj -> C:\opt\el\laserdock_jack\build\Release\lasershark_stdin_circl
  emaker.exe
  1>
  -- Install configuration: "Release"
  -- Installing: C:/Program Files (x86)/lasershark_hostapp/bin/laserdock_jack.exe
  -- Up-to-date: C:/Program Files (x86)/lasershark_hostapp/bin/libusb-1.0.dll
PS C:\opt\el\laserdock_jack\build>
```
