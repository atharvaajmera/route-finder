# Testing Guide - Test Centre Allotter v2.0

## 🧪 Test Categories

### 1. Unit Tests (Algorithm Verification)

#### Test 1: Haversine Distance Calculation

**Goal**: Verify distance calculation accuracy

```cpp
// Expected: ~1570 meters between these coordinates
double dist = haversine(28.6139, 77.2090, 28.6250, 77.2100);
assert(dist >= 1500 && dist <= 1600);
```

**Manual Test**:

1. Add centre at [28.6139, 77.2090]
2. Add centre at [28.6250, 77.2100]
3. Check console log for distance

#### Test 2: Graph Construction

**Goal**: Verify graph is built from OSM data

**Test Steps**:

1. Add 1 centre
2. Click "Build Graph"
3. Check response: `nodes_count > 0` and `edges_count > 0`

**Expected**:

- Nodes: 1000-10000 (depends on zoom level)
- Edges: 2000-20000

#### Test 3: Dijkstra Pre-computation

**Goal**: Verify distance lookup table creation

**Test Steps**:

1. Add 3 centres
2. Build graph
3. Check backend logs for "Running Dijkstra from centre_X"

**Expected**: 3 Dijkstra runs, one per centre

#### Test 4: A\* Pathfinding

**Goal**: Verify optimal path finding

**Test Steps**:

1. Complete allotment
2. Click assigned student
3. Click "Show Path"
4. Verify path is drawn (orange dashed line)

**Expected**: Path connects student to assigned centre

### 2. Integration Tests (System Workflow)

#### Test 5: Complete Workflow

**Test Steps**:

1. Add 5 centres (different locations)
2. Build graph (wait for success)
3. Simulate 100 students
4. Run allotment
5. Verify stats: 95-100% assigned

**Expected Results**:

```
Centres: 5
Students: 100
Assigned: 95-100
Graph Nodes: >0
```

#### Test 6: Category Constraints

**Test Steps**:

1. Add centre with wheelchair_access = false
2. Add centre with wheelchair_access = true
3. Build graph
4. Create students with category = "pwd"
5. Run allotment
6. Verify PwD students only go to accessible centre

**Verification**:

- Click PwD students (category visible in popup)
- Check assigned centre has wheelchair icon

#### Test 7: Capacity Limits

**Test Steps**:

1. Add 2 centres with capacity = 10 each
2. Build graph
3. Simulate 50 students
4. Run allotment

**Expected**:

- Max 20 students assigned (10 + 10)
- Remaining 30 unassigned (grey dots)

### 3. Performance Tests

#### Test 8: Scalability - Medium Load

**Configuration**:

- Centres: 10
- Students: 500
- Expected time: <15 seconds total

**Metrics**:

```
Graph Build: <10s
Allotment: <3s
Memory: <200MB
```

#### Test 9: Scalability - High Load

**Configuration**:

- Centres: 20
- Students: 2000
- Expected time: <30 seconds total

**Metrics**:

```
Graph Build: <15s
Allotment: <10s
Memory: <500MB
```

#### Test 10: Stress Test - Priority Queue

**Test Steps**:

1. Add 5 centres with capacity 100 each
2. Simulate 500 students
3. Monitor console for "Priority queue built with X assignments"

**Expected**:

- Queue size: 5 × 500 = 2500 potential assignments
- Processing: <5 seconds
- Assigned: 500 (all students)

### 4. Edge Cases

#### Test 11: Zero Students

**Test Steps**:

1. Add centres
2. Build graph
3. Don't simulate students
4. Click "Run Allotment"

**Expected**: Alert "Please simulate students first!"

#### Test 12: Zero Centres

**Test Steps**:

1. Don't add centres
2. Click "Build Graph"

**Expected**: Alert "Please add at least one test centre first!"

#### Test 13: Disconnected Graph

**Test Steps**:

1. Zoom to remote area (ocean, desert)
2. Add centres
3. Try building graph

**Expected**:

- Graph may have few/no nodes
- Allotment may fail or have low assignment rate

#### Test 14: All Centres Full

**Test Steps**:

1. Add 2 centres with capacity 10 each
2. Build graph
3. Simulate 100 students
4. Run allotment

**Expected**:

- 20 students assigned
- 80 students remain grey (unassigned)

#### Test 15: Category Mismatch

**Test Steps**:

1. Add only female-only centres
2. Simulate students (60% general male)
3. Run allotment

**Expected**: Only female students assigned, males unassigned

### 5. API Tests

#### Test 16: CORS Headers

```bash
curl -H "Origin: http://localhost:3000" \
     -H "Access-Control-Request-Method: POST" \
     -H "Access-Control-Request-Headers: Content-Type" \
     -X OPTIONS http://localhost:8080/build-graph
```

**Expected**: Response includes CORS headers

#### Test 17: POST /build-graph

```bash
curl -X POST http://localhost:8080/build-graph \
  -H "Content-Type: application/json" \
  -d '{
    "min_lat": 28.5, "min_lon": 77.1,
    "max_lat": 28.7, "max_lon": 77.3,
    "centres": [
      {
        "centre_id": "test_centre_1",
        "lat": 28.6139, "lon": 77.2090,
        "max_capacity": 50,
        "has_wheelchair_access": true,
        "is_female_only": false
      }
    ]
  }'
```

**Expected**:

```json
{
  "status": "success",
  "nodes_count": 5000,
  "edges_count": 10000
}
```

#### Test 18: POST /run-allotment

```bash
curl -X POST http://localhost:8080/run-allotment \
  -H "Content-Type: application/json" \
  -d '{
    "students": [
      {
        "student_id": "s1",
        "lat": 28.614, "lon": 77.209,
        "category": "general"
      }
    ]
  }'
```

**Expected**:

```json
{
  "status": "success",
  "assignments": {
    "s1": "test_centre_1"
  }
}
```

### 6. UI/UX Tests

#### Test 19: Map Interaction

**Test Steps**:

1. Click map at various locations
2. Verify centre marker appears at click point
3. Verify popup shows correct info

**Expected**: Red circle marker with capacity info

#### Test 20: Statistics Update

**Test Steps**:

1. Monitor stats panel during workflow
2. Check after each action:
   - Add centre → Centres count increases
   - Simulate students → Students count updates
   - Run allotment → Assigned count appears

**Expected**: Real-time updates

#### Test 21: Loader Display

**Test Steps**:

1. Click "Build Graph"
2. Verify loader appears with spinner
3. Verify "Processing..." message
4. Verify loader disappears after completion

**Expected**: Smooth loading UX

#### Test 22: Color Coding

**Test Steps**:

1. Add 5 centres
2. Run complete workflow
3. Verify students assigned to same centre have same color
4. Verify each centre has unique color

**Expected**: Visual distinction clear

### 7. Error Handling Tests

#### Test 23: Backend Offline

**Test Steps**:

1. Stop backend server
2. Try building graph from frontend

**Expected**: Alert "Failed to connect to backend server..."

#### Test 24: Invalid JSON

**Test Steps**:

1. Send malformed JSON to API

```bash
curl -X POST http://localhost:8080/build-graph \
  -H "Content-Type: application/json" \
  -d '{invalid json}'
```

**Expected**:

```json
{
  "status": "error",
  "message": "JSON parsing error: ..."
}
```

#### Test 25: Overpass API Timeout

**Test Steps**:

1. Select very large map area (zoom out to country level)
2. Add centres
3. Try building graph

**Expected**:

- May timeout after 30s
- Error message in backend console

## 📊 Test Results Template

```markdown
| Test ID | Test Name          | Status  | Time | Notes          |
| ------- | ------------------ | ------- | ---- | -------------- |
| Test 1  | Haversine Distance | ✅ Pass | <1s  | Accurate       |
| Test 5  | Complete Workflow  | ✅ Pass | 8s   | 98% assigned   |
| Test 8  | Medium Load        | ✅ Pass | 12s  | Good perf      |
| Test 13 | Disconnected Graph | ⚠️ Warn | 5s   | Low nodes      |
| Test 23 | Backend Offline    | ✅ Pass | 1s   | Good error msg |
```

## 🎯 Success Criteria

### Minimum Passing Requirements:

- ✅ Tests 1-7: Must pass (core functionality)
- ✅ Tests 8-9: Must pass (performance)
- ✅ Tests 11-12: Must pass (error handling)
- ✅ Test 16-18: Must pass (API correctness)

### Performance Benchmarks:

- Graph build: <15s for 5000 nodes
- Allotment: <5s for 1000 students × 10 centres
- Path finding: <0.5s per path
- Memory: <500MB for largest scenario

### Quality Metrics:

- Assignment rate: >95% when capacity available
- Path accuracy: 100% (must reach destination)
- UI responsiveness: <100ms for interactions
- Error messages: Clear and actionable

## 🔧 Debugging Tips

### Backend Debugging

```cpp
// Add to main.cpp for verbose logging
std::cout << "Debug: Processing student " << student.student_id << std::endl;
```

### Frontend Debugging

```javascript
// In browser console
console.table(centres);
console.table(students);
console.log(assignments);
```

### Performance Profiling

```javascript
// In app.js
console.time("Graph Build");
await buildGraph();
console.timeEnd("Graph Build");
```

## 📝 Manual Test Checklist

Before release:

- [ ] All unit tests pass
- [ ] Complete workflow works end-to-end
- [ ] UI is responsive and clear
- [ ] Error messages are helpful
- [ ] Performance meets benchmarks
- [ ] Category constraints work correctly
- [ ] Capacity limits enforced
- [ ] Paths draw correctly
- [ ] Stats update in real-time
- [ ] Works on different browsers (Chrome, Firefox, Edge)

---

**Remember**: Good testing ensures robust software! 🧪✅
