# Scalable Test Centre Allotter v2.0

A high-performance C++/JavaScript application to efficiently assign students to test centres based on location, capacity constraints, and category filters using advanced DSA algorithms.

## 🎯 Features

- **Real-time Map Integration**: Uses OpenStreetMap data via Overpass API
- **Advanced Pathfinding**: Dijkstra's algorithm for pre-computation, A\* for visualization
- **Smart Allotment**: Batch Greedy algorithm with priority queue
- **Category Constraints**: Supports PwD (wheelchair access) and female-only centres
- **Interactive Dashboard**: Leaflet.js-based UI with live visualization
- **High Performance**: Handles large N (students) and M (centres) efficiently

## 🏗️ Architecture

### Backend (C++)

- **Language**: C++17
- **Server**: httplib.h (lightweight HTTP server)
- **Graph Data**: Adjacency list using STL containers
- **APIs Used**: Overpass API for OpenStreetMap data
- **Libraries**:
  - `httplib.h`: HTTP server
  - `nlohmann/json`: JSON parsing
  - `libcurl`: HTTP requests

### Frontend (JavaScript)

- **Language**: Vanilla JavaScript + HTML/CSS
- **Map Library**: Leaflet.js
- **UI**: Responsive design with modern gradient aesthetics

## 📋 Prerequisites

### Windows (MinGW)

1. **MinGW-w64** or **MSYS2** installed
2. **libcurl** development libraries
3. **C++17 compatible compiler** (g++ 7.0+)

### Linux/macOS

1. **g++** or **clang++** with C++17 support
2. **libcurl** development package
3. **make** (optional)

## 🔧 Installation

### Step 1: Install Dependencies

#### Windows (MSYS2)

```bash
# Install MSYS2 from https://www.msys2.org/

# In MSYS2 terminal:
pacman -S mingw-w64-x86_64-gcc
pacman -S mingw-w64-x86_64-curl
```

#### Ubuntu/Debian

```bash
sudo apt-get update
sudo apt-get install g++ libcurl4-openssl-dev
```

#### macOS

```bash
brew install curl
```

### Step 2: Download Required Headers

The project requires two header-only libraries that are already in the `dsa-project` directory:

1. **httplib.h** - Already present in project root
2. **json.hpp** - Already present in project root

## 🚀 Building the Application

### Windows (PowerShell/CMD)

```powershell
cd backend
g++ -std=c++17 main.cpp -o server.exe -I.. -lcurl -lws2_32 -DCPPHTTPLIB_OPENSSL_SUPPORT
```

### Linux/macOS

```bash
cd backend
g++ -std=c++17 main.cpp -o server -I.. -lcurl -lpthread
```

## ▶️ Running the Application

### Step 1: Start the Backend Server

```bash
cd backend
./server        # Linux/macOS
./server.exe    # Windows
```

The server will start on `http://localhost:8080`

### Step 2: Open the Frontend Dashboard

Open `frontend/index.html` in a web browser:

- **Firefox/Chrome**: Simply double-click the file
- **Or use a local server** (recommended):
  ```bash
  cd frontend
  python -m http.server 3000
  ```
  Then navigate to `http://localhost:3000`

## 📖 Usage Guide

### 1. Add Test Centres

- Click anywhere on the map to add test centres
- Configure capacity (default: 50 students)
- Set wheelchair access for PwD students
- Mark as female-only if needed

### 2. Build Allotment Zones

- Click **"Build Allotment Zones"** button
- Backend fetches road network from OpenStreetMap
- Builds graph and runs Dijkstra from each centre
- Creates pre-computed distance lookup table

### 3. Simulate Students

- Enter number of students (e.g., 100, 1000)
- Click **"Simulate Students"**
- Random students appear on map with categories:
  - General (60%)
  - PwD (20%)
  - Female (20%)

### 4. Run Allotment

- Click **"RUN ALLOTMENT"** button
- Batch Greedy algorithm assigns students
- Color-coded visualization shows assignments
- Each centre gets a unique color

### 5. View Paths

- Click on any assigned student marker
- Click **"Show Path"** in popup
- A\* algorithm finds optimal route
- Animated path drawn on map

## 🧮 Core Algorithms

### 1. Graph Representation

```cpp
std::unordered_map<long, std::vector<std::pair<long, double>>> graph;
```

- Adjacency list: NodeID → [(NeighborID, Distance)]
- Edge weights calculated using Haversine formula

### 2. Pre-computation (Dijkstra)

- **Runs**: M times (once per centre)
- **Data Structure**: Min-heap priority queue
- **Output**: `allotment_lookup_map[node_id][centre_id] = distance`
- **Time Complexity**: O(M × (V + E) log V)

### 3. Batch Greedy Allotment

```cpp
std::priority_queue<AssignmentPair, vector<AssignmentPair>, greater<>> pq;
```

- Creates N × M potential assignments
- Filters by category constraints
- Pops best (minimum distance) assignments
- Respects capacity limits
- **Time Complexity**: O(N × M log(N × M))

### 4. Path Visualization (A\*)

- **Heuristic**: Haversine distance (admissible)
- **Trigger**: User clicks "Show Path"
- **Time Complexity**: O(E log V) worst case

## 🗂️ Project Structure

```
dsa-project/
├── backend/
│   ├── main.cpp              # C++ server with all DSA logic
│   └── server.exe            # Compiled binary (after build)
├── frontend/
│   ├── index.html            # Dashboard UI
│   └── app.js                # Frontend logic & API calls
├── httplib.h                 # HTTP server library
├── json.hpp                  # JSON parser library
└── README.md                 # This file
```

## 🔌 API Endpoints

### POST `/build-graph`

**Request**:

```json
{
  "min_lat": 28.5,
  "min_lon": 77.1,
  "max_lat": 28.7,
  "max_lon": 77.3,
  "centres": [...]
}
```

**Response**:

```json
{
  "status": "success",
  "nodes_count": 5432,
  "edges_count": 8765
}
```

### POST `/run-allotment`

**Request**:

```json
{
  "students": [...]
}
```

**Response**:

```json
{
  "status": "success",
  "assignments": {
    "student_1": "centre_2",
    "student_2": "centre_1",
    ...
  }
}
```

### GET `/get-path`

**Query**: `?student_node_id=123&centre_node_id=456`
**Response**:

```json
{
  "status": "success",
  "path": [[28.6139, 77.2090], [28.6145, 77.2095], ...]
}
```

## ⚠️ Troubleshooting

### "Cannot connect to backend"

- Ensure backend server is running on port 8080
- Check firewall settings
- Verify `API_BASE_URL` in `app.js`

### "Graph building takes too long"

- Overpass API may be slow or rate-limited
- Try smaller map area (zoom in)
- Check internet connection

### "Students not assigned"

- Ensure graph is built first
- Check category constraints
- Verify centre capacities

### Compilation errors

- Install libcurl development package
- Use C++17 or later: `-std=c++17`
- On Windows, add `-lws2_32` flag

## 🎓 Educational Value

This project demonstrates:

- **Graph Algorithms**: Dijkstra, A\*, Graph Traversal
- **Data Structures**: Priority Queue, Hash Maps, Adjacency List
- **System Design**: Client-Server Architecture, REST APIs
- **Real-world DSA**: Map data processing, optimization problems
- **Full-stack Development**: C++ backend, JavaScript frontend

## 📊 Performance

- **Pre-computation**: O(M × (V + E) log V)
- **Allotment**: O(N × M log(N × M))
- **Pathfinding**: O(E log V) per path
- **Memory**: O(V × M) for lookup table

**Example**:

- 10 centres, 1000 students, 5000 graph nodes
- Graph build: ~2-5 seconds
- Allotment: <1 second
- Path finding: <0.1 seconds

## 🤝 Contributing

This is an educational project. Feel free to:

- Optimize algorithms
- Add new features (traffic simulation, real-time updates)
- Improve UI/UX
- Add test cases

## 📝 License

Educational use only. OpenStreetMap data is © OpenStreetMap contributors.

## 👨‍💻 Author

Created as a demonstration of advanced DSA concepts in C++ with modern web technologies.

---

**Happy Coding! 🚀**
