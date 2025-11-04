# Installing libcurl for Visual Studio 2022

This guide will help you install libcurl using vcpkg to enable real OpenStreetMap integration.

## Step 1: Install vcpkg

Open **x64 Native Tools Command Prompt for VS 2022** and run:

```cmd
cd C:\
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
bootstrap-vcpkg.bat
```

## Step 2: Install libcurl

```cmd
vcpkg install curl:x64-windows
vcpkg integrate install
```

This will:

- Download and compile libcurl (takes 5-10 minutes)
- Install it to `C:\vcpkg\installed\x64-windows\`
- Integrate with Visual Studio

## Step 3: Compile the Backend

Navigate to your project:

```cmd
cd C:\Users\Atharva\OneDrive\Desktop\Programming\dsa-project\backend
```

Compile with the new build command:

```cmd
cl.exe /EHsc /std:c++17 /I.. /IC:\vcpkg\installed\x64-windows\include main.cpp /Fe:server.exe /link /LIBPATH:C:\vcpkg\installed\x64-windows\lib libcurl.lib ws2_32.lib wldap32.lib advapi32.lib crypt32.lib normaliz.lib
```

## Step 4: Run the Server

```cmd
server.exe
```

## Expected Output

You should see:

```
Fetching real road data from Overpass API...
Successfully fetched XXXXX bytes from Overpass API
Extracted XXXX unique nodes from OSM data
Built graph with XXXX nodes and XXXXX directed edges
```

## Troubleshooting

**If vcpkg fails to install:**

- Make sure Git is installed: https://git-scm.com/download/win
- Run the commands as Administrator

**If compilation fails with "cannot find libcurl.lib":**

- Check if the path `C:\vcpkg\installed\x64-windows\lib\libcurl.lib` exists
- Adjust the `/LIBPATH:` in the compile command to match your vcpkg location

**If server crashes when fetching data:**

- Copy `libcurl.dll` from `C:\vcpkg\installed\x64-windows\bin\` to the backend folder
- Or add `C:\vcpkg\installed\x64-windows\bin` to your PATH

## Alternative: Quick Build Script

Save this as `build_with_curl.bat`:

```batch
@echo off
set VCPKG_ROOT=C:\vcpkg
cl.exe /EHsc /std:c++17 /I.. /I%VCPKG_ROOT%\installed\x64-windows\include main.cpp /Fe:server.exe /link /LIBPATH:%VCPKG_ROOT%\installed\x64-windows\lib libcurl.lib ws2_32.lib wldap32.lib advapi32.lib crypt32.lib normaliz.lib
```

Then just run:

```cmd
build_with_curl.bat
```
