# Quick Start Guide - Test Centre Allotter v2.0

## ⚡ Fast Setup (5 Minutes)

### Prerequisites

- **Windows**: MinGW or MSYS2 with g++ and libcurl
- **Linux/macOS**: g++ and libcurl installed

### Step 1: Build the Backend

```powershell
# Windows
.\build.ps1

# Linux/macOS
chmod +x build.sh
./build.sh
```

### Step 2: Start the Server

```bash
cd backend
./server      # Linux/macOS
./server.exe  # Windows
```

You should see: `Server starting on http://localhost:8080`

### Step 3: Open the Dashboard

- Open `frontend/index.html` in your web browser
- Or run: `python -m http.server 3000` in the `frontend` directory

## 🎮 Using the Application

### Phase 1: Setup (30 seconds)

1. **Add Centres**: Click 3-5 times on the map to place test centres
   - Each click adds a centre with default capacity of 50
   - You can adjust wheelchair access and female-only settings

### Phase 2: Build Graph (10-30 seconds)

2. **Build Allotment Zones**: Click the button
   - Backend fetches road network from OpenStreetMap
   - Runs Dijkstra algorithm from each centre
   - Shows success message when complete

### Phase 3: Simulate & Allot (5 seconds)

3. **Simulate Students**:

   - Enter number (e.g., 100)
   - Click "Simulate Students"
   - Random colored dots appear on map

4. **Run Allotment**:
   - Click the big green "RUN ALLOTMENT" button
   - Students change color based on assigned centre
   - Check statistics panel for results

### Phase 4: Visualize (Interactive)

5. **View Paths**:
   - Click any assigned student (colored dot)
   - Click "Show Path" in the popup
   - Watch A\* draw the optimal route

## 📊 What to Expect

### Performance Metrics

| Students | Centres | Graph Build | Allotment | Total |
| -------- | ------- | ----------- | --------- | ----- |
| 100      | 5       | ~5s         | <1s       | ~6s   |
| 500      | 10      | ~8s         | ~2s       | ~10s  |
| 1000     | 15      | ~12s        | ~5s       | ~17s  |

### Visual Feedback

- **Red circles** = Test centres
- **Blue dots** = Unassigned students
- **Colored dots** = Assigned students (color matches centre)
- **Orange dashed line** = A\* path

## 🐛 Common Issues

### "Cannot connect to backend"

**Fix**: Make sure the backend server is running

```bash
cd backend
./server.exe  # Should show "Server starting on..."
```

### "Graph building failed"

**Fix**:

- Check internet connection (needs Overpass API)
- Zoom in to smaller area on map
- Wait 10 seconds and try again (API rate limit)

### Compilation errors (Windows)

**Fix**:

```powershell
# Install MSYS2, then:
pacman -S mingw-w64-x86_64-gcc
pacman -S mingw-w64-x86_64-curl
```

### Compilation errors (Linux)

**Fix**:

```bash
sudo apt-get install g++ libcurl4-openssl-dev
```

## 🎯 Demo Scenario

Try this complete workflow:

1. **Zoom** to your city (or use default Delhi location)
2. **Click 5 times** on different areas to add centres
3. **Build graph** (wait for success message)
4. **Simulate 200 students**
5. **Run allotment**
6. **Click several students** to see different paths
7. **Check statistics** panel - should show ~200 assigned

## 🔧 Advanced Usage

### Custom Centre Settings

Before clicking to add a centre:

- Change "Centre Capacity" to 20-100
- Check "Wheelchair Access" for PwD students
- Check "Female Only" for restricted centres

### Large Scale Testing

```javascript
// In browser console (frontend):
document.getElementById("studentCount").value = 1000;
simulateStudents();
```

### API Testing

```bash
# Test if server is running
curl http://localhost:8080/

# Won't work without POST data, but shows server is alive
```

## 📈 Understanding Results

### Good Allotment

- 95-100% students assigned
- Similar load across centres
- Short average distances

### Poor Allotment

- <80% students assigned
- Some centres full, others empty
- **Fix**: Add more centres or increase capacity

### Check Centre Loads

Click each red centre marker to see:

- Current load vs. max capacity
- Wheelchair access status
- Female-only flag

## 🎓 Learn the Algorithms

### Watch in Action

1. Open browser DevTools (F12)
2. Go to Console tab
3. You'll see:
   ```
   Running Dijkstra from centre: centre_1
   Running Dijkstra from centre: centre_2
   Priority queue built with 1500 potential assignments
   Allotment complete! Assigned 200 students
   ```

### Understand the Flow

```
User clicks map
  ↓
Add centre marker
  ↓
User clicks "Build Graph"
  ↓
Backend: Fetch OSM data → Build graph → Run Dijkstra (M times)
  ↓
User simulates students
  ↓
User clicks "Run Allotment"
  ↓
Backend: Create N×M pairs → Priority queue → Greedy assign
  ↓
Frontend: Color students by assignment
  ↓
User clicks student → Backend: Run A* → Frontend: Draw path
```

## ✅ Success Checklist

- [ ] Backend compiles without errors
- [ ] Server starts on port 8080
- [ ] Frontend opens in browser
- [ ] Can add centres by clicking map
- [ ] Graph builds successfully
- [ ] Students appear on map
- [ ] Allotment assigns students (check stats)
- [ ] Paths draw when clicking students
- [ ] No console errors in browser

## 🚀 Next Steps

After getting it working:

1. Try different map locations (zoom to your city)
2. Experiment with 1000+ students
3. Test category constraints (PwD, female-only)
4. Modify capacities and see effect on assignments
5. Read the main README.md for algorithmic details

---

**Need Help?** Check the console logs in both terminal (backend) and browser (frontend) for detailed debugging info.

**Enjoy exploring the power of DSA! 🎓**
