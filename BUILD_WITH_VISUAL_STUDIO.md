# Build Instructions with Visual Studio

## ✅ After Installing Visual Studio Community 2022

### Step 1: Open the Correct Command Prompt

**Important**: You MUST use the Visual Studio command prompt, not regular PowerShell!

Find and open one of these:

- **"Developer Command Prompt for VS 2022"**
- **"x64 Native Tools Command Prompt for VS 2022"** (Preferred)

You can find them by:

1. Press Windows key
2. Type "x64 native"
3. Click on "x64 Native Tools Command Prompt for VS 2022"

### Step 2: Navigate to Project

```cmd
cd C:\Users\Atharva\OneDrive\Desktop\Programming\dsa-project\backend
```

### Step 3: Compile the Backend

```cmd
cl /EHsc /std:c++17 /Fe:server.exe main.cpp /I.. ws2_32.lib
```

**Explanation of flags**:

- `/EHsc` - Enable C++ exception handling
- `/std:c++17` - Use C++17 standard
- `/Fe:server.exe` - Output file name
- `/I..` - Include parent directory (for httplib.h and json_single.hpp)
- `ws2_32.lib` - Windows socket library

### Step 4: Run the Server

```cmd
server.exe
```

You should see: `Server starting on http://localhost:8080`

### Step 5: Open Frontend

1. Open a new browser tab
2. Navigate to: `file:///C:/Users/Atharva/OneDrive/Desktop/Programming/dsa-project/frontend/index.html`
3. Or run a local server:
   ```cmd
   cd ..\frontend
   python -m http.server 3000
   ```
   Then open: `http://localhost:3000`

---

## 🔧 Alternative: Build Script for Visual Studio

Create a file `build_vs.bat` in the backend folder:

```batch
@echo off
echo Building Test Centre Allotter with MSVC...
cl /EHsc /std:c++17 /Fe:server.exe main.cpp /I.. ws2_32.lib
if %errorlevel% equ 0 (
    echo Build successful!
    echo Run with: server.exe
) else (
    echo Build failed!
)
```

Then just run:

```cmd
build_vs.bat
```

---

## ⚠️ Common Issues

### Issue: "cl is not recognized"

**Solution**: You're not in the Visual Studio command prompt. See Step 1 above.

### Issue: "Cannot open include file: 'httplib.h'"

**Solution**: Make sure you're in the `backend` folder and the `/I..` flag is included.

### Issue: "unresolved external symbol"

**Solution**: Make sure `ws2_32.lib` is at the end of the compile command.

---

## 🎯 Quick Test

After compiling, test the server:

```cmd
server.exe
```

Then in another terminal:

```powershell
curl http://localhost:8080/
```

You should get a response (even if it's an error - it means the server is running!).

---

## 📊 Expected Output

When compilation succeeds, you'll see something like:

```
Microsoft (R) C/C++ Optimizing Compiler Version 19.XX.XXXXX for x64
Copyright (C) Microsoft Corporation.  All rights reserved.

main.cpp
Microsoft (R) Incremental Linker Version 14.XX.XXXXX.X
Copyright (C) Microsoft Corporation.  All rights reserved.

/out:server.exe
main.obj
ws2_32.lib
```

File created: `server.exe` (around 500KB - 2MB)

---

## 🚀 You're Ready!

Once you see "Server starting on http://localhost:8080", your backend is running!

Open `frontend/index.html` in your browser and start using the application!
