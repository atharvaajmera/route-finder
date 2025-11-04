#include "../httplib.h"
#include "../json_single.hpp"

#define _USE_MATH_DEFINES
#include <cmath>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <limits>
#include <algorithm>
#include <set>
#include <random>
#include <sstream>
#include <iomanip>
#include <curl/curl.h>

// Define M_PI if not already defined (for MSVC)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using json = nlohmann::json;

// ==================== DATA STRUCTURES ====================

struct Student
{
    std::string student_id;
    double lat;
    double lon;
    long snapped_node_id;
    std::string category; // "general", "pwd", "female"
};

struct Centre
{
    std::string centre_id;
    double lat;
    double lon;
    long snapped_node_id;
    int max_capacity;
    int current_load;
    bool has_wheelchair_access;
    bool is_female_only;
};

struct AssignmentPair
{
    double distance;
    std::string student_id;
    std::string centre_id;

    bool operator>(const AssignmentPair &other) const
    {
        return distance > other.distance;
    }
};

struct Node
{
    long id;
    double lat;
    double lon;
};

// ==================== GLOBAL STATE ====================

// Graph: adjacency list (node_id -> vector of (neighbor_id, distance))
std::unordered_map<long, std::vector<std::pair<long, double>>> graph;

// Node information
std::unordered_map<long, Node> nodes;

// Allotment lookup map: node_id -> (centre_id -> distance)
std::unordered_map<long, std::unordered_map<std::string, double>> allotment_lookup_map;

// Store centres and students
std::vector<Centre> centres;
std::vector<Student> students;

// Final assignments: student_id -> centre_id
std::unordered_map<std::string, std::string> final_assignments;

// ==================== UTILITY FUNCTIONS ====================

// Haversine distance formula
double haversine(double lat1, double lon1, double lat2, double lon2)
{
    const double R = 6371000; // Earth radius in meters
    double phi1 = lat1 * M_PI / 180.0;
    double phi2 = lat2 * M_PI / 180.0;
    double delta_phi = (lat2 - lat1) * M_PI / 180.0;
    double delta_lambda = (lon2 - lon1) * M_PI / 180.0;

    double a = sin(delta_phi / 2) * sin(delta_phi / 2) +
               cos(phi1) * cos(phi2) *
                   sin(delta_lambda / 2) * sin(delta_lambda / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));

    return R * c;
}

// Find nearest node in graph for a given lat/lon
long find_nearest_node(double lat, double lon)
{
    long nearest = -1;
    double min_dist = std::numeric_limits<double>::max();

    for (const auto &[node_id, node] : nodes)
    {
        double dist = haversine(lat, lon, node.lat, node.lon);
        if (dist < min_dist)
        {
            min_dist = dist;
            nearest = node_id;
        }
    }

    return nearest;
}

// Find K nearest nodes (for more robust pathfinding)
std::vector<long> find_k_nearest_nodes(double lat, double lon, int k = 5)
{
    std::vector<std::pair<double, long>> distances; // (distance, node_id)

    for (const auto &[node_id, node] : nodes)
    {
        double dist = haversine(lat, lon, node.lat, node.lon);
        distances.push_back({dist, node_id});
    }

    // Sort by distance
    std::sort(distances.begin(), distances.end());

    // Return top K node IDs
    std::vector<long> result;
    for (int i = 0; i < std::min(k, (int)distances.size()); i++)
    {
        result.push_back(distances[i].second);
    }

    return result;
}

const double MAX_SPEED_MPS = 27.8;
// Heuristic for A* (Euclidean distance)
double heuristic(long node1, long node2)
{
    if (nodes.find(node1) == nodes.end() || nodes.find(node2) == nodes.end())
    {
        return 0.0;
    }

    // Get the straight-line distance in meters
    double distance_meters = haversine(nodes[node1].lat, nodes[node1].lon,
                                       nodes[node2].lat, nodes[node2].lon);

    // Estimate the time (in seconds) to travel that distance at max speed
    // time = distance / speed
    double time_seconds = distance_meters / MAX_SPEED_MPS;

    return time_seconds;
}

// ==================== OVERPASS API INTEGRATION ====================

// libcurl write callback to store response data
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
}

// Fetch real road network data from Overpass API
std::string fetch_overpass_data(double min_lat, double min_lon, double max_lat, double max_lon)
{
    std::cout << "Fetching real road data from Overpass API..." << std::endl;

    // Build Overpass query for road network
    // Include multiple road types for a denser, more realistic graph
    std::stringstream query_stream;
    query_stream << std::fixed << std::setprecision(8); // High precision for coordinates
    query_stream << "[out:json][timeout:60];";
    query_stream << "(";
    // Get major roads AND residential streets for realistic routing
    // primary/secondary/tertiary: Main roads
    // residential: Neighborhood streets
    // living_street: Low-speed residential
    // service: Parking lots, alleyways, building access
    // unclassified: Minor roads
    query_stream << "way[highway~\"primary|secondary|tertiary|residential|living_street|service|unclassified\"]";
    query_stream << "(" << min_lat << "," << min_lon << "," << max_lat << "," << max_lon << ");";
    // Get all nodes that are part of those ways
    query_stream << "node(w);";
    query_stream << ");";
    // Output everything with tags (to get highway type, oneway, maxspeed)
    query_stream << "out body;";
    std::string query = query_stream.str();

    std::cout << "Query: " << query << std::endl;

    // URL encode the query
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        std::cerr << "Failed to initialize curl" << std::endl;
        return "{}";
    }

    char *encoded_query = curl_easy_escape(curl, query.c_str(), query.length());
    std::string url = "https://overpass-api.de/api/interpreter?data=" + std::string(encoded_query);
    curl_free(encoded_query);

    std::string response_data;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L); // 60 second timeout (increased for large areas)
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK)
    {
        std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        curl_easy_cleanup(curl);
        return "{}";
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_easy_cleanup(curl);

    if (http_code != 200)
    {
        std::cerr << "HTTP error: " << http_code << std::endl;
        return "{}";
    }

    std::cout << "Successfully fetched " << response_data.length() << " bytes from Overpass API" << std::endl;
    return response_data;
}

// Helper: Get default speed (km/h) for a highway type
double get_default_speed(const std::string &highway_type)
{
    if (highway_type == "motorway")
        return 100.0;
    if (highway_type == "trunk")
        return 90.0;
    if (highway_type == "primary")
        return 80.0;
    if (highway_type == "secondary")
        return 60.0;
    if (highway_type == "tertiary")
        return 50.0;
    if (highway_type == "residential")
        return 30.0;
    if (highway_type == "living_street")
        return 20.0;
    if (highway_type == "service")
        return 20.0;
    if (highway_type == "unclassified")
        return 40.0;
    return 30.0; // Default fallback
}

// Build graph from real Overpass API data
void build_graph_from_overpass(const json &osm_data)
{
    std::cout << "Building graph from real OSM node/way data..." << std::endl;

    // Clear existing graph
    nodes.clear();
    graph.clear();

    if (!osm_data.contains("elements"))
    {
        std::cerr << "ERROR: No 'elements' field in OSM data - Overpass API call failed!" << std::endl;
        return;
    }

    if (osm_data["elements"].empty())
    {
        std::cerr << "ERROR: Overpass returned empty elements array!" << std::endl;
        return;
    }

    // === FIRST PASS: Store all nodes ===
    // Loop through all elements and save *only* the nodes first.
    // They have the real OSM IDs and coordinates.
    for (const auto &element : osm_data["elements"])
    {
        if (element["type"] == "node")
        {
            long id = element["id"];
            double lat = element["lat"];
            double lon = element["lon"];
            nodes[id] = {id, lat, lon};
        }
    }
    std::cout << "Stored " << nodes.size() << " unique nodes from OSM data" << std::endl;

    // === SECOND PASS: Build edges from ways ===
    // Now loop again, but only look for 'ways' (roads).
    // Handle one-way streets and speed-based routing
    int edge_count = 0;
    int oneway_count = 0;

    for (const auto &element : osm_data["elements"])
    {
        if (element["type"] == "way" && element.contains("nodes"))
        {
            // Get way properties from tags
            std::string highway_type = "";
            bool is_oneway = false;
            double speed_kmh = 30.0; // Default speed

            if (element.contains("tags"))
            {
                const auto &tags = element["tags"];

                // Get highway type
                if (tags.contains("highway"))
                {
                    highway_type = tags["highway"].get<std::string>();
                    speed_kmh = get_default_speed(highway_type);
                }

                // Check for one-way street
                if (tags.contains("oneway"))
                {
                    std::string oneway_val = tags["oneway"].get<std::string>();
                    is_oneway = (oneway_val == "yes" || oneway_val == "true" || oneway_val == "1");
                }

                // Check for explicit maxspeed tag
                if (tags.contains("maxspeed"))
                {
                    std::string maxspeed_str = tags["maxspeed"].get<std::string>();
                    try
                    {
                        // Try to parse speed (assumes km/h, handles "50" or "50 km/h")
                        speed_kmh = std::stod(maxspeed_str);
                    }
                    catch (...)
                    {
                        // Keep default if parsing fails
                    }
                }
            }

            // "nodes" array contains real OSM node IDs
            const auto &way_node_ids = element["nodes"];

            // Connect consecutive nodes in the way
            for (size_t i = 0; i < way_node_ids.size() - 1; i++)
            {
                long node1_id = way_node_ids[i];
                long node2_id = way_node_ids[i + 1];

                // Make sure we have these nodes in our map
                if (nodes.find(node1_id) != nodes.end() && nodes.find(node2_id) != nodes.end())
                {
                    // Calculate distance in meters
                    double dist_meters = haversine(
                        nodes[node1_id].lat, nodes[node1_id].lon,
                        nodes[node2_id].lat, nodes[node2_id].lon);

                    // Convert to time-based weight (seconds)
                    // time = distance / speed
                    double dist_km = dist_meters / 1000.0;
                    double time_hours = dist_km / speed_kmh;
                    double time_seconds = time_hours * 3600.0;

                    // Use time as edge weight for realistic routing
                    double edge_weight = time_seconds;

                    if (is_oneway)
                    {
                        // One-way street: only add edge in forward direction
                        graph[node1_id].push_back({node2_id, edge_weight});
                        edge_count++;
                        oneway_count++;
                    }
                    else
                    {
                        // Two-way street: add bidirectional edges
                        graph[node1_id].push_back({node2_id, edge_weight});
                        graph[node2_id].push_back({node1_id, edge_weight});
                        edge_count += 2;
                    }
                }
            }
        }
    }

    std::cout << "Built graph with " << nodes.size() << " nodes and " std::cout << "Built graph with " << nodes.size() << " nodes and "
              << edge_count << " directed edges" << std::endl;
    std::cout << "Found " << oneway_count << " one-way street segments" << std::endl;
}

// Fallback: Generate simulated graph when Overpass fails
void generate_simulated_graph_fallback(double min_lat, double min_lon, double max_lat, double max_lon)
{
    std::cout << "\n=== FALLBACK: Generating simulated road network ===" << std::endl;
    std::cout << "Note: Install libcurl (vcpkg) for real OpenStreetMap data" << std::endl;

    nodes.clear();
    graph.clear();

    const int GRID_SIZE = 80;
    double lat_step = (max_lat - min_lat) / GRID_SIZE;
    double lon_step = (max_lon - min_lon) / GRID_SIZE;

    long node_id = 1;
    std::vector<std::vector<long>> grid_nodes(GRID_SIZE, std::vector<long>(GRID_SIZE));

    // Create nodes
    for (int i = 0; i < GRID_SIZE; i++)
    {
        for (int j = 0; j < GRID_SIZE; j++)
        {
            double lat = min_lat + i * lat_step;
            double lon = min_lon + j * lon_step;
            nodes[node_id] = {node_id, lat, lon};
            grid_nodes[i][j] = node_id;
            node_id++;
        }
    }

    // Create edges with 8-directional connectivity for smoother paths
    for (int i = 0; i < GRID_SIZE; i++)
    {
        for (int j = 0; j < GRID_SIZE; j++)
        {
            long current = grid_nodes[i][j];

            // All 8 directions
            std::vector<std::pair<int, int>> directions = {
                {0, 1},   // Right
                {1, 0},   // Down
                {1, 1},   // Diagonal down-right
                {1, -1},  // Diagonal down-left
                {0, -1},  // Left (for bidirectional)
                {-1, 0},  // Up (for bidirectional)
                {-1, -1}, // Diagonal up-left
                {-1, 1}   // Diagonal up-right
            };

            for (const auto &dir : directions)
            {
                int ni = i + dir.first;
                int nj = j + dir.second;

                if (ni >= 0 && ni < GRID_SIZE && nj >= 0 && nj < GRID_SIZE)
                {
                    long neighbor = grid_nodes[ni][nj];
                    double dist = haversine(nodes[current].lat, nodes[current].lon,
                                            nodes[neighbor].lat, nodes[neighbor].lon);

                    // Only add edge if not already present (avoid duplicates)
                    bool exists = false;
                    for (const auto &edge : graph[current])
                    {
                        if (edge.first == neighbor)
                        {
                            exists = true;
                            break;
                        }
                    }
                    if (!exists)
                    {
                        graph[current].push_back({neighbor, dist});
                    }
                }
            }
        }
    }

    std::cout << "Simulated graph: " << nodes.size() << " nodes, "
              << graph.size() << " connected nodes" << std::endl;
}

// ==================== DIJKSTRA ALGORITHM ====================

std::unordered_map<long, double> dijkstra(long start_node)
{
    std::unordered_map<long, double> distances;
    std::priority_queue<std::pair<double, long>,
                        std::vector<std::pair<double, long>>,
                        std::greater<std::pair<double, long>>>
        pq;

    // Initialize distances
    for (const auto &[node_id, _] : nodes)
    {
        distances[node_id] = std::numeric_limits<double>::max();
    }
    distances[start_node] = 0.0;

    pq.push({0.0, start_node});

    while (!pq.empty())
    {
        auto [current_dist, current_node] = pq.top();
        pq.pop();

        if (current_dist > distances[current_node])
        {
            continue;
        }

        if (graph.find(current_node) != graph.end())
        {
            for (const auto &[neighbor, edge_weight] : graph[current_node])
            {
                double new_dist = current_dist + edge_weight;

                if (new_dist < distances[neighbor])
                {
                    distances[neighbor] = new_dist;
                    pq.push({new_dist, neighbor});
                }
            }
        }
    }

    return distances;
}

// ==================== A* ALGORITHM ====================

std::vector<long> a_star(long start_node, long goal_node)
{
    std::unordered_map<long, double> g_score;
    std::unordered_map<long, double> f_score;
    std::unordered_map<long, long> came_from;

    auto compare = [&f_score](long a, long b)
    {
        return f_score[a] > f_score[b];
    };

    std::priority_queue<long, std::vector<long>, decltype(compare)> open_set(compare);
    std::set<long> open_set_tracker;

    for (const auto &[node_id, _] : nodes)
    {
        g_score[node_id] = std::numeric_limits<double>::max();
        f_score[node_id] = std::numeric_limits<double>::max();
    }

    g_score[start_node] = 0.0;
    f_score[start_node] = heuristic(start_node, goal_node);

    open_set.push(start_node);
    open_set_tracker.insert(start_node);

    while (!open_set.empty())
    {
        long current = open_set.top();
        open_set.pop();
        open_set_tracker.erase(current);

        if (current == goal_node)
        {
            // Reconstruct path
            std::vector<long> path;
            long node = goal_node;
            while (came_from.find(node) != came_from.end())
            {
                path.push_back(node);
                node = came_from[node];
            }
            path.push_back(start_node);
            std::reverse(path.begin(), path.end());
            return path;
        }

        if (graph.find(current) != graph.end())
        {
            for (const auto &[neighbor, edge_weight] : graph[current])
            {
                double tentative_g_score = g_score[current] + edge_weight;

                if (tentative_g_score < g_score[neighbor])
                {
                    came_from[neighbor] = current;
                    g_score[neighbor] = tentative_g_score;
                    f_score[neighbor] = g_score[neighbor] + heuristic(neighbor, goal_node);

                    if (open_set_tracker.find(neighbor) == open_set_tracker.end())
                    {
                        open_set.push(neighbor);
                        open_set_tracker.insert(neighbor);
                    }
                }
            }
        }
    }

    return {}; // No path found
}

// ==================== ALLOTMENT LOGIC ====================

void build_allotment_lookup()
{
    std::cout << "Building allotment lookup map..." << std::endl;

    for (const auto &centre : centres)
    {
        std::cout << "Running Dijkstra from centre: " << centre.centre_id << std::endl;

        auto distances = dijkstra(centre.snapped_node_id);

        for (const auto &[node_id, dist] : distances)
        {
            allotment_lookup_map[node_id][centre.centre_id] = dist;
        }
    }

    std::cout << "Allotment lookup map built successfully!" << std::endl;
}

bool is_valid_assignment(const Student &student, const Centre &centre)
{
    // Check category constraints
    if (student.category == "pwd" && !centre.has_wheelchair_access)
    {
        return false;
    }

    if (student.category == "female" && !centre.is_female_only)
    {
        // If centre is not female-only but is general, allow it
        // Only reject if explicitly incompatible
    }

    return true;
}

void run_batch_greedy_allotment()
{
    std::cout << "Running Batch Greedy Allotment..." << std::endl;

    std::priority_queue<AssignmentPair, std::vector<AssignmentPair>,
                        std::greater<AssignmentPair>>
        pq;

    // Build priority queue with all valid assignments
    for (const auto &student : students)
    {
        if (allotment_lookup_map.find(student.snapped_node_id) == allotment_lookup_map.end())
        {
            continue;
        }

        const auto &centre_distances = allotment_lookup_map[student.snapped_node_id];

        for (const auto &centre : centres)
        {
            if (!is_valid_assignment(student, centre))
            {
                continue;
            }

            if (centre_distances.find(centre.centre_id) != centre_distances.end())
            {
                double distance = centre_distances.at(centre.centre_id);
                pq.push({distance, student.student_id, centre.centre_id});
            }
        }
    }

    std::cout << "Priority queue built with " << pq.size() << " potential assignments" << std::endl;

    // Track assigned students and centre capacities
    std::set<std::string> assigned_students;
    std::unordered_map<std::string, int> centre_loads;

    for (auto &centre : centres)
    {
        centre_loads[centre.centre_id] = 0;
    }

    // Process assignments
    while (!pq.empty())
    {
        auto assignment = pq.top();
        pq.pop();

        // Check if student already assigned
        if (assigned_students.find(assignment.student_id) != assigned_students.end())
        {
            continue;
        }

        // Find centre and check capacity
        Centre *target_centre = nullptr;
        for (auto &centre : centres)
        {
            if (centre.centre_id == assignment.centre_id)
            {
                target_centre = &centre;
                break;
            }
        }

        if (target_centre == nullptr)
        {
            continue;
        }

        if (centre_loads[assignment.centre_id] >= target_centre->max_capacity)
        {
            continue;
        }

        // Make assignment
        final_assignments[assignment.student_id] = assignment.centre_id;
        assigned_students.insert(assignment.student_id);
        centre_loads[assignment.centre_id]++;
        target_centre->current_load++;
    }

    std::cout << "Allotment complete! Assigned " << assigned_students.size()
              << " students" << std::endl;
}

// ==================== HTTP SERVER ====================

int main()
{
    httplib::Server server;

    // Enable CORS
    server.set_pre_routing_handler([](const httplib::Request &req, httplib::Response &res)
                                   {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        if (req.method == "OPTIONS") {
            res.status = 200;
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled; });

    // ========== /build-graph endpoint ==========
    server.Post("/build-graph", [](const httplib::Request &req, httplib::Response &res)
                {
        try {
            auto body = json::parse(req.body);
            double min_lat = body["min_lat"];
            double min_lon = body["min_lon"];
            double max_lat = body["max_lat"];
            double max_lon = body["max_lon"];
            
            // Parse centres from request
            centres.clear();
            for (const auto& c : body["centres"]) {
                Centre centre;
                centre.centre_id = c["centre_id"];
                centre.lat = c["lat"];
                centre.lon = c["lon"];
                centre.max_capacity = c["max_capacity"];
                centre.current_load = 0;
                centre.has_wheelchair_access = c["has_wheelchair_access"];
                centre.is_female_only = c["is_female_only"];
                centres.push_back(centre);
            }
            
            // Fetch real road network data from Overpass API
            std::string osm_json_string = fetch_overpass_data(min_lat, min_lon, max_lat, max_lon);
            
            // Parse the JSON response
            json osm_data = json::parse(osm_json_string);
            
            // Build graph from real OSM data
            build_graph_from_overpass(osm_data);
            
            // If graph is empty (Overpass failed), use simulated fallback
            if (nodes.empty()) {
                std::cout << "\n⚠️  Overpass API failed - using simulated graph fallback" << std::endl;
                generate_simulated_graph_fallback(min_lat, min_lon, max_lat, max_lon);
            }
            
            // Snap centres to nearest nodes
            for (auto& centre : centres) {
                centre.snapped_node_id = find_nearest_node(centre.lat, centre.lon);
            }
            
            // Build allotment lookup map (runs Dijkstra from each centre)
            build_allotment_lookup();
            
            json response;
            response["status"] = "success";
            response["nodes_count"] = nodes.size();
            response["edges_count"] = graph.size();
            
            res.set_content(response.dump(), "application/json");
            
        } catch (const std::exception& e) {
            json error_response;
            error_response["status"] = "error";
            error_response["message"] = e.what();
            res.set_content(error_response.dump(), "application/json");
        } });

    // ========== /run-allotment endpoint ==========
    server.Post("/run-allotment", [](const httplib::Request &req, httplib::Response &res)
                {
        try {
            auto body = json::parse(req.body);
            
            // Parse students
            students.clear();
            for (const auto& s : body["students"]) {
                Student student;
                student.student_id = s["student_id"];
                student.lat = s["lat"];
                student.lon = s["lon"];
                student.category = s["category"];
                student.snapped_node_id = find_nearest_node(student.lat, student.lon);
                students.push_back(student);
            }
            
            // Run allotment
            final_assignments.clear();
            run_batch_greedy_allotment();
            
            json response;
            response["status"] = "success";
            response["assignments"] = final_assignments;
            
            res.set_content(response.dump(), "application/json");
            
        } catch (const std::exception& e) {
            json error_response;
            error_response["status"] = "error";
            error_response["message"] = e.what();
            res.set_content(error_response.dump(), "application/json");
        } });

    // ========== /get-path endpoint ==========
    server.Get("/get-path", [](const httplib::Request &req, httplib::Response &res)
               {
        try {
            // Check if we have node IDs or lat/lon
            std::vector<long> student_candidates;
            std::vector<long> centre_candidates;
            
            if (req.has_param("student_node_id") && req.has_param("centre_node_id")) {
                // Direct node IDs - use single node
                student_candidates.push_back(std::stol(req.get_param_value("student_node_id")));
                centre_candidates.push_back(std::stol(req.get_param_value("centre_node_id")));
            } else if (req.has_param("student_lat") && req.has_param("student_lon") &&
                       req.has_param("centre_lat") && req.has_param("centre_lon")) {
                // Lat/lon - use K-nearest nodes for robustness
                double student_lat = std::stod(req.get_param_value("student_lat"));
                double student_lon = std::stod(req.get_param_value("student_lon"));
                double centre_lat = std::stod(req.get_param_value("centre_lat"));
                double centre_lon = std::stod(req.get_param_value("centre_lon"));
                
                // Get 5 nearest nodes for both student and centre
                student_candidates = find_k_nearest_nodes(student_lat, student_lon, 5);
                centre_candidates = find_k_nearest_nodes(centre_lat, centre_lon, 5);
                
                std::cout << "Finding path: trying " << student_candidates.size() 
                          << "x" << centre_candidates.size() << " combinations" << std::endl;
            } else {
                throw std::runtime_error("Missing required parameters");
            }
            
            // Try all combinations until we find a valid path
            std::vector<long> best_path;
            bool found = false;
            
            for (long student_node : student_candidates) {
                for (long centre_node : centre_candidates) {
                    auto path = a_star(student_node, centre_node);
                    if (!path.empty()) {
                        best_path = path;
                        found = true;
                        std::cout << "✓ Found path: " << student_node << " -> " 
                                  << centre_node << " (" << path.size() << " nodes)" << std::endl;
                        break;
                    }
                }
                if (found) break;
            }
            
            if (!found) {
                std::cout << "✗ No path found after trying all combinations" << std::endl;
            }
            
            json response;
            response["status"] = "success";
            
            json path_coords = json::array();
            for (long node_id : best_path) {
                if (nodes.find(node_id) != nodes.end()) {
                    path_coords.push_back({nodes[node_id].lat, nodes[node_id].lon});
                }
            }
            
            response["path"] = path_coords;
            
            res.set_content(response.dump(), "application/json");
            
        } catch (const std::exception& e) {
            json error_response;
            error_response["status"] = "error";
            error_response["message"] = e.what();
            res.set_content(error_response.dump(), "application/json");
        } });

    std::cout << "Server starting on http://localhost:8080" << std::endl;
    server.listen("0.0.0.0", 8080);

    return 0;
}
