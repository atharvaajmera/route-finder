# 📚 Project Index - Test Centre Allotter v2.0

## 🎯 Start Here

**New to this project?** Read these files in order:
1. **PROJECT_SUMMARY.md** - Overview of what you built (5 min read)
2. **QUICKSTART.md** - Get it running in 5 minutes
3. **README.md** - Complete documentation (15 min read)

**Ready to dive deeper?**
4. **ARCHITECTURE.md** - System design and data flow
5. **TESTING.md** - 25+ test scenarios to validate

---

## 📁 Complete File Structure

```
dsa-project/
│
├── 📘 Documentation Files
│   ├── PROJECT_SUMMARY.md          ⭐ START HERE - Project overview
│   ├── QUICKSTART.md               🚀 5-minute setup guide
│   ├── README.md                   📖 Complete documentation
│   ├── ARCHITECTURE.md             🏗️ System design & data flow
│   ├── TESTING.md                  🧪 Test scenarios (25+)
│   └── INDEX.md                    📚 This file - navigation guide
│
├── 💻 Source Code
│   ├── backend/
│   │   └── main.cpp                🔥 C++ server (700+ lines)
│   │                               • All DSA algorithms
│   │                               • HTTP API endpoints
│   │                               • Graph processing
│   │
│   └── frontend/
│       ├── index.html              🎨 Dashboard UI
│       └── app.js                  ⚡ Frontend logic & API client
│
├── 📦 Dependencies (Header-Only)
│   ├── httplib.h                   🌐 HTTP server library
│   └── json.hpp                    📄 JSON parser (nlohmann)
│
├── 🔧 Build & Config
│   ├── build.ps1                   🪟 Windows build script
│   ├── build.sh                    🐧 Linux/macOS build script
│   ├── config.json                 ⚙️ Configuration presets
│   └── .gitignore                  🚫 Git ignore rules
│
└── 🎓 Learning Materials
    └── (Documentation files above)
```

---

## 🗺️ Navigation Guide

### 👨‍💻 For Developers

| I want to... | Read this file | Time |
|-------------|---------------|------|
| Understand the project | PROJECT_SUMMARY.md | 5 min |
| Get it running ASAP | QUICKSTART.md | 5 min |
| Learn the complete system | README.md | 15 min |
| Understand architecture | ARCHITECTURE.md | 10 min |
| Test the application | TESTING.md | 30 min |
| Modify the backend | backend/main.cpp | - |
| Modify the frontend | frontend/app.js, index.html | - |

### 🎓 For Learning

| Topic | File | Section |
|-------|------|---------|
| **Graph Algorithms** | backend/main.cpp | Lines 140-200 (Dijkstra) |
| **A* Search** | backend/main.cpp | Lines 202-270 |
| **Greedy Algorithm** | backend/main.cpp | Lines 400-500 |
| **Priority Queues** | backend/main.cpp | Lines 380-420 |
| **REST APIs** | backend/main.cpp | Lines 550-700 |
| **Map Integration** | frontend/app.js | Lines 1-100 |
| **Data Flow** | ARCHITECTURE.md | Section 2 |
| **Performance** | ARCHITECTURE.md | Section "Algorithm Complexity" |

### 📊 For Presentations

| Audience | Show these | Duration |
|----------|-----------|----------|
| **Technical Interview** | PROJECT_SUMMARY.md + backend/main.cpp | 10 min |
| **Demo** | QUICKSTART.md + Live demo | 5 min |
| **Deep Dive** | ARCHITECTURE.md + Code walkthrough | 20 min |
| **Portfolio** | README.md + Screenshots | 5 min |

---

## 🔍 Code Navigation

### Backend (main.cpp)

| Lines | Component | Description |
|-------|-----------|-------------|
| 1-30 | Imports & Setup | Libraries, namespaces, using declarations |
| 32-60 | Data Structures | Student, Centre, AssignmentPair, Node |
| 62-90 | Global State | Graph, nodes, lookup map, assignments |
| 92-120 | Utility Functions | Haversine, nearest node, heuristic |
| 122-140 | CURL Callback | HTTP response handler |
| 142-200 | **Dijkstra Algorithm** | Single-source shortest path |
| 202-270 | **A* Algorithm** | Heuristic pathfinding |
| 272-350 | Graph Building | Overpass API integration |
| 352-380 | Pre-computation | Build allotment lookup map |
| 382-420 | **Greedy Allotment** | Main assignment algorithm |
| 422-700 | HTTP Server | API endpoints, request handling |

### Frontend (app.js)

| Lines | Component | Description |
|-------|-----------|-------------|
| 1-20 | Global State | centres, students, assignments, map |
| 22-40 | Map Init | Leaflet setup, tile layer |
| 42-70 | Centre Management | Add, clear, marker creation |
| 72-120 | Graph Building | API call, error handling |
| 122-170 | Student Simulation | Random generation, markers |
| 172-230 | **Allotment Logic** | API call, assignment processing |
| 232-280 | Visualization | Color coding, path drawing |
| 282-320 | **A* Path Display** | Fetch and render paths |
| 322-360 | UI Helpers | Stats, loader, etc. |

---

## 📖 Documentation Deep Dive

### README.md Structure
1. **Features** - What the system does
2. **Architecture** - High-level design
3. **Prerequisites** - System requirements
4. **Installation** - Step-by-step setup
5. **Building** - Compilation instructions
6. **Running** - Execution guide
7. **Usage** - How to use the UI
8. **Algorithms** - DSA explanations
9. **API Endpoints** - REST API docs
10. **Troubleshooting** - Common issues
11. **Performance** - Benchmarks

### ARCHITECTURE.md Structure
1. **High-Level Diagram** - System overview
2. **Data Flow** - Graph building, allotment, pathfinding
3. **Data Structures** - Implementation details
4. **API Contracts** - Request/response specs
5. **Complexity Analysis** - Big-O notation
6. **Performance Profile** - Real-world metrics
7. **Tech Stack** - All technologies used

### TESTING.md Structure
1. **Unit Tests** - Algorithm verification (5 tests)
2. **Integration Tests** - System workflow (3 tests)
3. **Performance Tests** - Scalability (3 tests)
4. **Edge Cases** - Boundary conditions (5 tests)
5. **API Tests** - Endpoint validation (3 tests)
6. **UI Tests** - User experience (4 tests)
7. **Error Handling** - Failure modes (3 tests)

---

## 🎯 Quick Reference

### Build Commands
```powershell
# Windows
.\build.ps1

# Linux/macOS
chmod +x build.sh
./build.sh
```

### Run Commands
```bash
# Start backend
cd backend
./server.exe    # Windows
./server        # Linux/macOS

# Open frontend
# Simply open frontend/index.html in browser
```

### Test Commands
```bash
# Check server health
curl http://localhost:8080/

# Build graph (requires JSON body)
curl -X POST http://localhost:8080/build-graph ...

# See TESTING.md for complete API test commands
```

---

## 🔗 File Dependencies

```
main.cpp
├── Requires: httplib.h, json.hpp
├── External: libcurl
└── Compiles to: server.exe / server

app.js
├── Requires: index.html
├── External: Leaflet.js CDN
└── Calls: backend API endpoints

index.html
├── Includes: app.js
└── External: Leaflet CSS/JS CDN

build.ps1 / build.sh
├── Compiles: main.cpp
├── Links: libcurl, ws2_32 (Windows)
└── Output: backend/server.exe
```

---

## 📊 Complexity Reference Card

| Algorithm | Time | Space | Use Case |
|-----------|------|-------|----------|
| **Dijkstra** | O((V+E) log V) | O(V) | Pre-compute all distances |
| **Greedy Allotment** | O(NM log(NM)) | O(NM) | Assign students to centres |
| **A* Search** | O(E log V) | O(V) | Single path visualization |
| **Haversine** | O(1) | O(1) | Distance calculation |
| **Graph Build** | O(V + E) | O(V + E) | Parse OSM data |

**Legend**: V=nodes, E=edges, N=students, M=centres

---

## 🎓 Learning Path

### Beginner (1-2 hours)
1. Read PROJECT_SUMMARY.md
2. Follow QUICKSTART.md to run the app
3. Play with the UI (add centres, simulate students)
4. Read "Usage Guide" in README.md

### Intermediate (3-5 hours)
5. Read ARCHITECTURE.md data flow diagrams
6. Study backend/main.cpp Dijkstra implementation
7. Study frontend/app.js API integration
8. Run tests from TESTING.md (Test 1-7)

### Advanced (6+ hours)
9. Deep dive into A* algorithm code
10. Understand priority queue mechanics
11. Optimize code (improve performance)
12. Add new features (see PROJECT_SUMMARY.md)
13. Run full test suite (all 25 tests)

---

## 🚀 Next Actions

### Just Starting?
→ Read **PROJECT_SUMMARY.md** (5 minutes)
→ Follow **QUICKSTART.md** (5 minutes)
→ You're ready to demo!

### Want to Understand?
→ Read **ARCHITECTURE.md** (10 minutes)
→ Study **backend/main.cpp** (30 minutes)
→ You understand the system!

### Want to Master?
→ Complete **TESTING.md** tests (1 hour)
→ Modify and extend features (2+ hours)
→ You own this project!

---

## 📞 Help & Support

### Issue: Can't compile
→ See README.md "Installation" section
→ Check QUICKSTART.md "Troubleshooting"

### Issue: Server won't start
→ Check if port 8080 is available
→ See TESTING.md "Error Handling Tests"

### Issue: Graph build fails
→ Check internet connection (Overpass API)
→ Try smaller map area (zoom in)

### Issue: Low assignment rate
→ Check centre capacities
→ Review category constraints
→ See TESTING.md "Edge Cases"

---

## 📈 Project Metrics

- **Total Files**: 13
- **Lines of Code**: 700+ (C++) + 500+ (JavaScript)
- **Documentation**: 2000+ lines
- **Algorithms**: 4 major (Dijkstra, A*, Greedy, Haversine)
- **APIs**: 3 endpoints
- **Test Cases**: 25+
- **Build Time**: <10 seconds
- **Setup Time**: 5 minutes

---

## 🏆 Achievement Checklist

- [ ] Read PROJECT_SUMMARY.md
- [ ] Successfully built backend (no errors)
- [ ] Started server on port 8080
- [ ] Opened frontend in browser
- [ ] Added centres to map
- [ ] Built graph successfully
- [ ] Simulated students
- [ ] Ran allotment (>95% assigned)
- [ ] Viewed A* path visualization
- [ ] Read ARCHITECTURE.md
- [ ] Ran at least 5 tests from TESTING.md
- [ ] Modified code and recompiled
- [ ] Understand all 4 algorithms
- [ ] Can explain to someone else

---

## 📝 Contributing Ideas

Want to extend this project?

**Easy Additions**:
- [ ] Add more centre categories (metro accessible, parking, etc.)
- [ ] Different color schemes for UI
- [ ] Save/load configurations
- [ ] Export assignments to CSV

**Medium Additions**:
- [ ] Database integration (SQLite, PostgreSQL)
- [ ] User authentication
- [ ] Multiple allotment strategies comparison
- [ ] Performance analytics dashboard

**Advanced Additions**:
- [ ] Real-time traffic data integration
- [ ] Multi-objective optimization (distance + fairness)
- [ ] Machine learning for demand prediction
- [ ] Mobile app (React Native, Flutter)

---

## 📚 Learning Resources

**C++ & STL**:
- cppreference.com - STL documentation
- learncpp.com - C++ fundamentals

**Algorithms**:
- Introduction to Algorithms (CLRS)
- Competitive Programmer's Handbook

**System Design**:
- Designing Data-Intensive Applications
- System Design Primer (GitHub)

**Web Development**:
- MDN Web Docs - JavaScript/HTML/CSS
- Leaflet.js documentation

---

**Happy exploring! 🚀 This index should help you navigate the entire project efficiently.**

---

*Last updated: Project completion*
*Total documentation: ~2500 lines across 6 files*
