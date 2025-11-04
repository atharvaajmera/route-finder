# How to Get Real Road Network Data

## Current Situation

Your application works but uses a **simulated grid** instead of real roads. This is why paths show as straight lines instead of following actual streets.

## Why This Happened

We removed the Overpass API integration because it requires `libcurl` which wasn't installed on your system.

## Solution: Install vcpkg and libcurl

### Step 1: Install vcpkg (Microsoft's C++ Package Manager)

Open **x64 Native Tools Command Prompt for VS 2022** and run:

```cmd
cd C:\
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
bootstrap-vcpkg.bat
```

### Step 2: Install libcurl

```cmd
vcpkg install curl:x64-windows
vcpkg integrate install
```

This will take 5-10 minutes.

### Step 3: Restore Real Map Data Code

I'll need to restore the original Overpass API code in `main.cpp`. The changes needed:

1. Add back `#include <curl/curl.h>`
2. Replace `generate_simulated_graph()` with `build_graph_from_overpass()`
3. Restore the CURL callback function

### Step 4: Recompile with libcurl

```cmd
cd C:\Users\Atharva\OneDrive\Desktop\Programming\dsa-project\backend
cl /EHsc /std:c++17 /Fe:server.exe main.cpp /I.. /IC:\vcpkg\installed\x64-windows\include ws2_32.lib C:\vcpkg\installed\x64-windows\lib\libcurl.lib
```

### Step 5: Run and Test

```cmd
server.exe
```

Now when you click "Build Graph", it will:

- ✅ Fetch real road data from OpenStreetMap
- ✅ Build graph from actual streets/highways
- ✅ Paths will follow real roads!

---

## Alternative: Keep the Grid (Easier)

The grid-based system is **perfectly valid** for:

- ✅ Learning and demonstrating algorithms
- ✅ Portfolio projects (explain it's simulated data)
- ✅ Testing performance and scalability
- ✅ Understanding DSA concepts

Just add a note in your UI: _"Using simulated road network for demonstration"_

---

## My Recommendation

For now, **keep the grid**. It works great for demonstrating your DSA knowledge!

If you need real roads later (for production or specific demo), follow the vcpkg installation steps above.

The **algorithms are identical** - the only difference is the input graph structure.

---

## Quick Improvement: Make Grid Denser

Want better-looking paths without real data? Increase the grid density:

In `main.cpp`, line ~136, change:

```cpp
const int GRID_SIZE = 50;  // Current
```

To:

```cpp
const int GRID_SIZE = 100; // Denser grid = smoother paths
```

Recompile and paths will look smoother (but still follow grid pattern).

**Trade-off**: More nodes = slower Dijkstra pre-computation.
