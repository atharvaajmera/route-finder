// ==================== GLOBAL STATE ====================

const API_BASE_URL = "http://localhost:8080";

let map;
let centres = [];
let students = [];
let assignments = {};
let graphBuilt = false;

let centreMarkers = [];
let studentMarkers = [];
let pathLayers = [];

const centreColors = [
  "#ef4444",
  "#f59e0b",
  "#10b981",
  "#3b82f6",
  "#8b5cf6",
  "#ec4899",
  "#14b8a6",
  "#f97316",
];

// ==================== INITIALIZE MAP ====================

function initMap() {
  // Initialize map centered on a default location (can be changed)
  map = L.map("map").setView([30.726352078816053, 76.77585936519962], 12); // Jodhpur, India

  L.tileLayer("https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png", {
    attribution: "© OpenStreetMap contributors",
    maxZoom: 19,
  }).addTo(map);

  // Add click event to add centres
  map.on("click", function (e) {
    addCentre(e.latlng.lat, e.latlng.lng);
  });

  console.log("Map initialized");
}

// ==================== CENTRE MANAGEMENT ====================

function addCentre(lat, lon) {
  const centreId = `centre_${centres.length + 1}`;
  const capacity = parseInt(document.getElementById("centreCapacity").value);
  const wheelchairAccess = document.getElementById("wheelchairAccess").checked;
  const femaleOnly = document.getElementById("femaleOnly").checked;

  const centre = {
    centre_id: centreId,
    lat: lat,
    lon: lon,
    max_capacity: capacity,
    has_wheelchair_access: wheelchairAccess,
    is_female_only: femaleOnly,
  };

  centres.push(centre);

  // Add marker to map
  const color = centreColors[centres.length % centreColors.length];
  const marker = L.circleMarker([lat, lon], {
    radius: 10,
    fillColor: "#ef4444",
    color: "#fff",
    weight: 3,
    opacity: 1,
    fillOpacity: 0.9,
  }).addTo(map);

  marker.bindPopup(`
        <strong>${centreId}</strong><br>
        Capacity: ${capacity}<br>
        ${wheelchairAccess ? "♿ Wheelchair Access<br>" : ""}
        ${femaleOnly ? "👩 Female Only<br>" : ""}
    `);

  centreMarkers.push(marker);

  updateStats();
  console.log(`Added ${centreId} at [${lat}, ${lon}]`);
}

function clearCentres() {
  centres = [];
  centreMarkers.forEach((marker) => map.removeLayer(marker));
  centreMarkers = [];
  updateStats();
  console.log("Cleared all centres");
}

// ==================== GRAPH BUILDING ====================

async function buildGraph() {
  if (centres.length === 0) {
    alert("Please add at least one test centre first!");
    return;
  }

  showLoader("Building graph from OpenStreetMap...");

  try {
    // Get map bounds
    const bounds = map.getBounds();
    const payload = {
      min_lat: bounds.getSouth(),
      min_lon: bounds.getWest(),
      max_lat: bounds.getNorth(),
      max_lon: bounds.getEast(),
      centres: centres,
    };

    console.log("Sending build-graph request:", payload);

    const response = await fetch(`${API_BASE_URL}/build-graph`, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify(payload),
    });

    const data = await response.json();

    if (data.status === "success") {
      graphBuilt = true;
      document.getElementById("runAllotmentBtn").disabled = false;
      document.getElementById("statNodes").textContent = data.nodes_count;

      hideLoader();
      alert(
        `Graph built successfully!\nNodes: ${data.nodes_count}\nEdges: ${data.edges_count}`
      );
      console.log("Graph built:", data);
    } else {
      hideLoader();
      alert("Error building graph: " + data.message);
    }
  } catch (error) {
    hideLoader();
    alert(
      "Failed to connect to backend server. Make sure it is running on port 8080."
    );
    console.error("Error:", error);
  }
}

// ==================== STUDENT SIMULATION ====================

function simulateStudents() {
  const count = parseInt(document.getElementById("studentCount").value);

  if (count <= 0) {
    alert("Please enter a valid number of students!");
    return;
  }

  // Clear existing students
  students = [];
  studentMarkers.forEach((marker) => map.removeLayer(marker));
  studentMarkers = [];

  // Get map bounds
  const bounds = map.getBounds();
  const minLat = bounds.getSouth();
  const maxLat = bounds.getNorth();
  const minLon = bounds.getWest();
  const maxLon = bounds.getEast();

  // Generate random students
  const categories = ["general", "general", "general", "pwd", "female"];

  for (let i = 0; i < count; i++) {
    const lat = minLat + Math.random() * (maxLat - minLat);
    const lon = minLon + Math.random() * (maxLon - minLon);
    const category = categories[Math.floor(Math.random() * categories.length)];

    const student = {
      student_id: `student_${i + 1}`,
      lat: lat,
      lon: lon,
      category: category,
    };

    students.push(student);

    // Add marker
    const marker = L.circleMarker([lat, lon], {
      radius: 4,
      fillColor: "#3b82f6",
      color: "#fff",
      weight: 1,
      opacity: 1,
      fillOpacity: 0.7,
    }).addTo(map);

    marker.bindPopup(`
            <strong>${student.student_id}</strong><br>
            Category: ${category}
        `);

    studentMarkers.push(marker);
  }

  updateStats();
  console.log(`Simulated ${count} students`);
}

// ==================== ALLOTMENT ====================

async function runAllotment() {
  if (!graphBuilt) {
    alert("Please build the graph first!");
    return;
  }

  if (students.length === 0) {
    alert("Please simulate students first!");
    return;
  }

  showLoader("Running batch greedy allotment...");

  try {
    const payload = {
      students: students,
    };

    console.log("Sending run-allotment request");

    const response = await fetch(`${API_BASE_URL}/run-allotment`, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify(payload),
    });

    const data = await response.json();

    if (data.status === "success") {
      assignments = data.assignments;
      visualizeAssignments();
      hideLoader();

      const assignedCount = Object.keys(assignments).length;
      alert(
        `Allotment complete!\n${assignedCount} students assigned to centres.`
      );
      console.log("Allotment complete:", assignments);
    } else {
      hideLoader();
      alert("Error running allotment: " + data.message);
    }
  } catch (error) {
    hideLoader();
    alert("Failed to run allotment. Check backend server.");
    console.error("Error:", error);
  }
}

// ==================== VISUALIZATION ====================

function visualizeAssignments() {
  // Create a map of centre_id to color
  const centreColorMap = {};
  centres.forEach((centre, index) => {
    centreColorMap[centre.centre_id] =
      centreColors[index % centreColors.length];
  });

  // Update student markers based on assignments
  students.forEach((student, index) => {
    const assignedCentre = assignments[student.student_id];

    if (assignedCentre) {
      const color = centreColorMap[assignedCentre];

      // Update marker color
      studentMarkers[index].setStyle({
        fillColor: color,
        fillOpacity: 0.9,
        radius: 5,
      });

      // Update popup
      studentMarkers[index].bindPopup(`
                <strong>${student.student_id}</strong><br>
                Category: ${student.category}<br>
                Assigned to: <strong>${assignedCentre}</strong><br>
                <button onclick="showPath('${student.student_id}', '${assignedCentre}')">Show Path</button>
            `);
    } else {
      // Student not assigned - keep blue
      studentMarkers[index].setStyle({
        fillColor: "#6b7280",
        fillOpacity: 0.5,
      });
    }
  });

  updateStats();
}

async function showPath(studentId, centreId) {
  // Find student and centre
  const student = students.find((s) => s.student_id === studentId);
  const centre = centres.find((c) => c.centre_id === centreId);

  if (!student || !centre) {
    alert("Student or centre not found!");
    return;
  }

  showLoader("Finding optimal path...");

  try {
    // We need to pass the snapped node IDs - for simplicity, we'll use a workaround
    // In production, you'd store these after the allotment response
    const response = await fetch(
      `${API_BASE_URL}/get-path?student_lat=${student.lat}&student_lon=${student.lon}&centre_lat=${centre.lat}&centre_lon=${centre.lon}`
    );

    const data = await response.json();

    console.log("Path response:", data);
    console.log("Path length:", data.path ? data.path.length : 0);
    console.log("Path data:", data.path);

    if (data.status === "success" && data.path && data.path.length > 0) {
      // Clear previous paths
      pathLayers.forEach((layer) => map.removeLayer(layer));
      pathLayers = [];

      console.log("Drawing path with", data.path.length, "points");

      // Draw path
      const pathLine = L.polyline(data.path, {
        color: "#f59e0b",
        weight: 4,
        opacity: 0.8,
        dashArray: "10, 10",
      }).addTo(map);

      pathLayers.push(pathLine);

      // Fit map to path
      map.fitBounds(pathLine.getBounds());

      hideLoader();
      alert(`Path found with ${data.path.length} points!`);
      console.log("Path drawn successfully");
    } else {
      hideLoader();
      console.error("No path found. Response:", data);
      alert(
        "Could not find path between student and centre. Check console for details."
      );
    }
  } catch (error) {
    hideLoader();
    alert("Failed to get path. Check backend server.");
    console.error("Error:", error);
  }
}

// ==================== UI HELPERS ====================

function updateStats() {
  document.getElementById("statCentres").textContent = centres.length;
  document.getElementById("statStudents").textContent = students.length;
  document.getElementById("statAssigned").textContent =
    Object.keys(assignments).length;
}

function showLoader(message) {
  const loader = document.getElementById("loader");
  const loaderText = loader.querySelector(".loader-text");
  loaderText.textContent = message;
  loader.classList.add("active");
}

function hideLoader() {
  const loader = document.getElementById("loader");
  loader.classList.remove("active");
}

// ==================== INITIALIZE ====================

window.addEventListener("DOMContentLoaded", () => {
  initMap();
  updateStats();
  console.log("Application initialized");
});

// Make functions globally accessible
window.addCentre = addCentre;
window.clearCentres = clearCentres;
window.buildGraph = buildGraph;
window.simulateStudents = simulateStudents;
window.runAllotment = runAllotment;
window.showPath = showPath;
