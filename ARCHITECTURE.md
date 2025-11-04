# System Architecture - Test Centre Allotter v2.0

## 📐 High-Level Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│                         USER BROWSER                              │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │                    FRONTEND DASHBOARD                       │  │
│  │                   (HTML + CSS + JavaScript)                 │  │
│  │                                                              │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │  │
│  │  │  Leaflet.js  │  │   UI Layer   │  │ API Client   │     │  │
│  │  │   (Map)      │  │  (Controls)  │  │  (Fetch)     │     │  │
│  │  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘     │  │
│  └─────────┼──────────────────┼──────────────────┼────────────┘  │
└────────────┼──────────────────┼──────────────────┼───────────────┘
             │                  │                  │
             ↓                  ↓                  ↓
    Map Rendering      User Interactions    HTTP Requests (JSON)
             │                  │                  │
             └──────────────────┴──────────────────┘
                                │
                                ↓
                    ┌───────────────────────┐
                    │   Network (HTTP)      │
                    │   Port 8080           │
                    └───────────┬───────────┘
                                ↓
┌──────────────────────────────────────────────────────────────────┐
│                      C++ BACKEND SERVER                          │
│                     (httplib.h Server)                           │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │                    API ENDPOINTS                            │ │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │ │
│  │  │POST /build-  │  │POST /run-    │  │GET /get-path │     │ │
│  │  │     graph    │  │    allotment │  │              │     │ │
│  │  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘     │ │
│  └─────────┼──────────────────┼──────────────────┼────────────┘ │
│            ↓                  ↓                  ↓               │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │              DSA ENGINE (Core Logic)                     │   │
│  │  ┌────────────┐  ┌────────────┐  ┌────────────┐         │   │
│  │  │  Graph     │  │  Dijkstra  │  │    A*      │         │   │
│  │  │  Builder   │  │  Algorithm │  │  Algorithm │         │   │
│  │  └─────┬──────┘  └─────┬──────┘  └─────┬──────┘         │   │
│  │        │               │               │                 │   │
│  │        ↓               ↓               ↓                 │   │
│  │  ┌─────────────────────────────────────────┐             │   │
│  │  │    DATA STRUCTURES & STATE              │             │   │
│  │  │  • graph (adjacency list)               │             │   │
│  │  │  • nodes (hash map)                     │             │   │
│  │  │  • allotment_lookup_map                 │             │   │
│  │  │  • centres, students (vectors)          │             │   │
│  │  │  • final_assignments (hash map)         │             │   │
│  │  └────────────┬────────────────────────────┘             │   │
│  │               │                                           │   │
│  │               ↓                                           │   │
│  │  ┌────────────────────────────────┐                      │   │
│  │  │  Batch Greedy Allotment        │                      │   │
│  │  │  • Priority Queue (min-heap)   │                      │   │
│  │  │  • Constraint Checking         │                      │   │
│  │  │  • Capacity Management         │                      │   │
│  │  └────────────────────────────────┘                      │   │
│  └──────────────────────────────────────────────────────────┘   │
│            ↓                                                     │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │           EXTERNAL API CLIENT (libcurl)                  │   │
│  └─────────────────────┬───────────────────────────────────┘   │
└────────────────────────┼─────────────────────────────────────────┘
                         │
                         ↓
                ┌────────────────────┐
                │  Overpass API      │
                │  (OpenStreetMap)   │
                └────────────────────┘
```

---

## 🔄 Data Flow Diagrams

### 1. Graph Building Flow

```
User clicks "Build Graph"
    │
    ↓
Frontend: Get map bounds
    │
    ↓
POST /build-graph
    │ {min_lat, min_lon, max_lat, max_lon, centres[]}
    ↓
Backend: Construct Overpass Query
    │
    ↓
libcurl → Overpass API
    │
    ↓
Receive OSM JSON Data
    │
    ↓
nlohmann/json Parser
    │
    ├─→ Extract Nodes (id, lat, lon)
    │   └─→ Store in nodes hash map
    │
    └─→ Extract Ways (road segments)
        └─→ Build graph adjacency list
            │ For each consecutive node pair:
            │ • Calculate Haversine distance
            │ • Add bidirectional edge
            └─→ Store in graph
    │
    ↓
For each centre in centres[]:
    │
    ↓
    Find nearest graph node (snap)
    │
    ↓
    Run Dijkstra from centre node
    │ • Priority queue (min-heap)
    │ • Compute distances to all nodes
    │
    ↓
    Store in allotment_lookup_map
    │ [node_id][centre_id] = distance
    │
    ↓
Response: {status: "success", nodes_count, edges_count}
    │
    ↓
Frontend: Enable "Run Allotment" button
```

---

### 2. Allotment Flow

```
User clicks "Run Allotment"
    │
    ↓
POST /run-allotment
    │ {students: [{id, lat, lon, category}, ...]}
    ↓
Backend: Initialize
    │
    ├─→ Clear final_assignments
    ├─→ Create priority queue (empty)
    └─→ Initialize centre loads to 0
    │
    ↓
For each student:
    │
    ↓
    Snap to nearest graph node
    │
    ↓
    For each centre:
        │
        ↓
        Check category constraints
        │ • PwD → wheelchair access?
        │ • Female → female-only or general?
        │
        ↓
        If valid:
            │
            ↓
            Get pre-computed distance
            │ distance = allotment_lookup_map[student_node][centre_id]
            │
            ↓
            Push to priority queue
            │ AssignmentPair{distance, student_id, centre_id}
            │
            ↓
    │
    ↓
Priority Queue Processing:
    │
    ↓
    While queue not empty:
        │
        ↓
        Pop best assignment (minimum distance)
        │
        ↓
        Check if student already assigned
        │ If yes → continue
        │
        ↓
        Check if centre at capacity
        │ If full → continue
        │
        ↓
        Valid assignment!
        │
        ├─→ final_assignments[student_id] = centre_id
        ├─→ Increment centre_loads[centre_id]
        └─→ Mark student as assigned
        │
    ↓
Response: {status: "success", assignments: {...}}
    │
    ↓
Frontend: Color-code students by assignment
```

---

### 3. Path Visualization Flow

```
User clicks assigned student marker
    │
    ↓
Popup shows "Show Path" button
    │
    ↓
User clicks "Show Path"
    │
    ↓
GET /get-path?student_node_id=X&centre_node_id=Y
    │
    ↓
Backend: A* Search
    │
    ├─→ Initialize: g_score, f_score, came_from
    ├─→ Priority queue (f-score based)
    └─→ Heuristic: Haversine distance
    │
    ↓
    Open set: {start_node}
    │
    ↓
    While open set not empty:
        │
        ↓
        Pop node with minimum f_score
        │
        ↓
        If node == goal_node:
            │
            └─→ Reconstruct path from came_from
                └─→ Return [node, node, ..., goal]
        │
        ↓
        For each neighbor:
            │
            ↓
            Calculate tentative_g_score
            │ = current_g_score + edge_weight
            │
            ↓
            If better than previous:
                │
                ├─→ came_from[neighbor] = current
                ├─→ g_score[neighbor] = tentative
                ├─→ f_score[neighbor] = g + heuristic(neighbor, goal)
                └─→ Add to open set
                │
    ↓
Convert node IDs to lat/lon coordinates
    │
    ↓
Response: {status: "success", path: [[lat,lon], ...]}
    │
    ↓
Frontend: Draw polyline on map
```

---

## 🗄️ Data Structure Details

### Graph Representation

```cpp
// Adjacency List
std::unordered_map<
    long,                                    // Node ID
    std::vector<
        std::pair<long, double>              // (Neighbor ID, Distance)
    >
> graph;

// Example:
// graph[123] = [(124, 50.3), (125, 73.2), (126, 42.8)]
//   Node 123 connects to:
//     - Node 124 at 50.3 meters
//     - Node 125 at 73.2 meters
//     - Node 126 at 42.8 meters
```

### Allotment Lookup Map

```cpp
// Pre-computed Distances
std::unordered_map<
    long,                                    // Node ID
    std::unordered_map<
        std::string,                         // Centre ID
        double                               // Distance
    >
> allotment_lookup_map;

// Example:
// allotment_lookup_map[123]["centre_1"] = 1500.5
// allotment_lookup_map[123]["centre_2"] = 2300.7
//   From node 123:
//     - To centre_1: 1500.5 meters
//     - To centre_2: 2300.7 meters
```

### Priority Queue

```cpp
// Min-Heap for Greedy Assignment
struct AssignmentPair {
    double distance;        // Priority key
    std::string student_id;
    std::string centre_id;

    bool operator>(const AssignmentPair& other) const {
        return distance > other.distance;  // Min-heap
    }
};

std::priority_queue<
    AssignmentPair,
    std::vector<AssignmentPair>,
    std::greater<AssignmentPair>      // Custom comparator
> pq;

// Example queue contents (sorted by distance):
// Top → {500.2, "s1", "c2"}
//       {503.5, "s2", "c1"}
//       {510.0, "s3", "c2"}
//       {525.1, "s1", "c1"}
//       ...
```

---

## 🔌 API Contract Specifications

### Endpoint 1: Build Graph

**Request**:

```http
POST /build-graph HTTP/1.1
Content-Type: application/json

{
  "min_lat": 28.5,
  "min_lon": 77.1,
  "max_lat": 28.7,
  "max_lon": 77.3,
  "centres": [
    {
      "centre_id": "centre_1",
      "lat": 28.6139,
      "lon": 77.2090,
      "max_capacity": 50,
      "has_wheelchair_access": true,
      "is_female_only": false
    }
  ]
}
```

**Response**:

```http
HTTP/1.1 200 OK
Content-Type: application/json

{
  "status": "success",
  "nodes_count": 5432,
  "edges_count": 8765
}
```

---

### Endpoint 2: Run Allotment

**Request**:

```http
POST /run-allotment HTTP/1.1
Content-Type: application/json

{
  "students": [
    {
      "student_id": "student_1",
      "lat": 28.614,
      "lon": 77.209,
      "category": "general"
    },
    {
      "student_id": "student_2",
      "lat": 28.615,
      "lon": 77.210,
      "category": "pwd"
    }
  ]
}
```

**Response**:

```http
HTTP/1.1 200 OK
Content-Type: application/json

{
  "status": "success",
  "assignments": {
    "student_1": "centre_1",
    "student_2": "centre_2"
  }
}
```

---

### Endpoint 3: Get Path

**Request**:

```http
GET /get-path?student_node_id=12345&centre_node_id=67890 HTTP/1.1
```

**Response**:

```http
HTTP/1.1 200 OK
Content-Type: application/json

{
  "status": "success",
  "path": [
    [28.6139, 77.2090],
    [28.6145, 77.2095],
    [28.6150, 77.2100],
    [28.6155, 77.2105]
  ]
}
```

---

## 🧮 Algorithm Complexity Analysis

| Operation              | Data Structure | Time Complexity           | Space Complexity |
| ---------------------- | -------------- | ------------------------- | ---------------- |
| **Build Graph**        | Adjacency List | O(V + E)                  | O(V + E)         |
| **Snap to Node**       | Linear Search  | O(V)                      | O(1)             |
| **Dijkstra**           | Min-Heap PQ    | O((V + E) log V)          | O(V)             |
| **Pre-compute All**    | Run M times    | O(M × (V + E) log V)      | O(V × M)         |
| **Build Allotment PQ** | Insert N×M     | O(N × M log(N × M))       | O(N × M)         |
| **Greedy Assign**      | Extract min    | O(N × M log(N × M))       | O(N + M)         |
| **A\* Path**           | Min-Heap PQ    | O(E log V) typical        | O(V)             |
| **Total System**       | Combined       | O(M(V+E)logV + NMlog(NM)) | O(VM + NM)       |

**Where**:

- V = Number of graph nodes (road intersections)
- E = Number of edges (road segments)
- N = Number of students
- M = Number of centres

---

## 🎯 System Performance Profile

### Small Scale (Demo)

```
M = 5 centres
N = 100 students
V = 2,000 nodes
E = 4,000 edges

Graph Build:    ~3 seconds
Dijkstra (5×):  ~2 seconds
Allotment:      ~0.5 seconds
Total:          ~6 seconds
Memory:         ~50 MB
```

### Medium Scale (Typical)

```
M = 10 centres
N = 500 students
V = 5,000 nodes
E = 10,000 edges

Graph Build:    ~8 seconds
Dijkstra (10×): ~5 seconds
Allotment:      ~2 seconds
Total:          ~15 seconds
Memory:         ~200 MB
```

### Large Scale (Stress Test)

```
M = 20 centres
N = 2,000 students
V = 10,000 nodes
E = 20,000 edges

Graph Build:    ~15 seconds
Dijkstra (20×): ~10 seconds
Allotment:      ~8 seconds
Total:          ~33 seconds
Memory:         ~500 MB
```

---

## 🏗️ Technology Stack

### Backend Technologies

```
┌─────────────────────────────────┐
│ C++17 Language                  │
├─────────────────────────────────┤
│ Standard Template Library (STL) │
│ • unordered_map                 │
│ • vector                        │
│ • priority_queue                │
│ • algorithm                     │
├─────────────────────────────────┤
│ httplib.h (HTTP Server)         │
├─────────────────────────────────┤
│ nlohmann/json (JSON Parser)     │
├─────────────────────────────────┤
│ libcurl (HTTP Client)           │
└─────────────────────────────────┘
```

### Frontend Technologies

```
┌─────────────────────────────────┐
│ HTML5                           │
├─────────────────────────────────┤
│ CSS3 (Grid, Flexbox, Gradients) │
├─────────────────────────────────┤
│ JavaScript ES6+                 │
│ • async/await                   │
│ • Fetch API                     │
│ • Arrow functions               │
├─────────────────────────────────┤
│ Leaflet.js 1.9.4                │
│ • Map rendering                 │
│ • Marker management             │
│ • Polyline drawing              │
└─────────────────────────────────┘
```

### External Services

```
┌─────────────────────────────────┐
│ Overpass API                    │
│ • OpenStreetMap data            │
│ • Road network queries          │
│ • Node/way extraction           │
├─────────────────────────────────┤
│ OpenStreetMap Tile Server       │
│ • Map tiles                     │
│ • Zoom levels 1-19              │
└─────────────────────────────────┘
```

---

**This architecture enables scalable, efficient student-to-centre allocation using production-grade DSA techniques! 🚀**
