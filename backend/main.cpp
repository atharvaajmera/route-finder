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
#include <chrono>
#include <thread>
#include <mutex>
#include <future>
#include <fstream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using json = nlohmann::json;

struct Student
{
    std::string student_id;
    double lat;
    double lon;
    long snapped_node_id;
    std::string category;
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

struct KDTreeNode
{
    long node_id;
    double lat;
    double lon;
    int axis;
    KDTreeNode *left;
    KDTreeNode *right;

    KDTreeNode(long id, double lat_, double lon_, int axis_)
        : node_id(id), lat(lat_), lon(lon_), axis(axis_), left(nullptr), right(nullptr) {}
};

// ==================== GLOBAL STATE ====================

std::unordered_map<long, std::vector<std::pair<long, double>>> graph;
std::unordered_map<long, Node> nodes;
KDTreeNode *kdtree_root = nullptr;
std::unordered_map<long, std::unordered_map<std::string, double>> allotment_lookup_map; // node_id -> (centre_id -> dist)
std::vector<Centre> centres;
std::vector<Student> students;
std::unordered_map<std::string, std::string> final_assignments;

// New: component map
std::unordered_map<long, int> node_component;

// ==================== UTILITY FUNCTIONS ====================

double haversine(double lat1, double lon1, double lat2, double lon2)
{
    const double R = 6371000;
    double phi1 = lat1 * M_PI / 180.0;
    double phi2 = lat2 * M_PI / 180.0;
    double delta_phi = (lat2 - lat1) * M_PI / 180.0;
    double delta_lambda = (lon2 - lon1) * M_PI / 180.0;

    double a = sin(delta_phi / 2) * sin(delta_phi / 2) +
               cos(phi1) * cos(phi2) * sin(delta_lambda / 2) * sin(delta_lambda / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));

    return R * c;
}

// ==================== FORWARD DECLARATIONS ====================

std::vector<long> find_k_nearest_nodes(double lat, double lon, int k);
long find_best_snap_node_fast(double lat, double lon);
void snap_all_students_fast();
std::vector<long> clean_and_validate_path(const std::vector<long> &path);
std::vector<long> a_star_bidirectional(long start_node, long goal_node);

// ==================== KD-TREE IMPLEMENTATION ====================

KDTreeNode *build_kdtree(std::vector<std::pair<long, std::pair<double, double>>> &points, int depth)
{
    if (points.empty())
        return nullptr;

    int axis = depth % 2;

    std::sort(points.begin(), points.end(),
              [axis](const auto &a, const auto &b)
              {
                  return (axis == 0) ? a.second.first < b.second.first : a.second.second < b.second.second;
              });

    size_t median_idx = points.size() / 2;
    long median_id = points[median_idx].first;
    double median_lat = points[median_idx].second.first;
    double median_lon = points[median_idx].second.second;

    KDTreeNode *node = new KDTreeNode(median_id, median_lat, median_lon, axis);

    std::vector<std::pair<long, std::pair<double, double>>> left_points(points.begin(), points.begin() + median_idx);
    std::vector<std::pair<long, std::pair<double, double>>> right_points(points.begin() + median_idx + 1, points.end());

    node->left = build_kdtree(left_points, depth + 1);
    node->right = build_kdtree(right_points, depth + 1);

    return node;
}

void kdtree_nearest_helper(KDTreeNode *node, double target_lat, double target_lon,
                           long &best_id, double &best_dist)
{
    if (!node)
        return;

    double dist = haversine(target_lat, target_lon, node->lat, node->lon);

    if (dist < best_dist)
    {
        best_dist = dist;
        best_id = node->node_id;
    }

    double diff = (node->axis == 0) ? (target_lat - node->lat) : (target_lon - node->lon);
    KDTreeNode *near_side = (diff < 0) ? node->left : node->right;
    KDTreeNode *far_side = (diff < 0) ? node->right : node->left;

    kdtree_nearest_helper(near_side, target_lat, target_lon, best_id, best_dist);

    double axis_dist = std::abs(diff) * 111000.0;
    if (axis_dist < best_dist)
    {
        kdtree_nearest_helper(far_side, target_lat, target_lon, best_id, best_dist);
    }
}

long find_nearest_node(double lat, double lon)
{
    if (kdtree_root)
    {
        long best_id = -1;
        double best_dist = std::numeric_limits<double>::max();
        kdtree_nearest_helper(kdtree_root, lat, lon, best_id, best_dist);

        if (best_id != -1)
            return best_id;
    }

    std::vector<long> nearest = find_k_nearest_nodes(lat, lon, 1);

    if (nearest.empty())
        return -1;

    return nearest[0];
}

std::vector<long> find_k_nearest_nodes(double lat, double lon, int k = 5)
{
    std::vector<std::pair<double, long>> distances;
    distances.reserve(nodes.size());

    for (const auto &[node_id, node] : nodes)
    {
        if (graph.find(node_id) == graph.end())
            continue;

        double dist = haversine(lat, lon, node.lat, node.lon);
        distances.push_back({dist, node_id});
    }

    int k_safe = std::min(k, (int)distances.size());
    if (k_safe == 0)
        return {};

    std::nth_element(distances.begin(), distances.begin() + k_safe - 1, distances.end());

    std::vector<long> result;
    for (int i = 0; i < k_safe; i++)
    {
        result.push_back(distances[i].second);
    }

    return result;
}

// ==================== IMPROVED SNAPPING & PATH FUNCTIONS ====================

long find_best_snap_node_fast(double lat, double lon)
{
    if (kdtree_root)
    {
        long best_id = -1;
        double best_dist = std::numeric_limits<double>::max();
        kdtree_nearest_helper(kdtree_root, lat, lon, best_id, best_dist);

        if (best_id != -1)
        {
            return best_id;
        }
    }

    long best_node = -1;
    double best_dist = std::numeric_limits<double>::max();

    for (const auto &[node_id, node] : nodes)
    {
        if (graph.find(node_id) == graph.end() || graph[node_id].empty())
        {
            continue;
        }

        double dist = haversine(lat, lon, node.lat, node.lon);
        if (dist < best_dist)
        {
            best_dist = dist;
            best_node = node_id;
        }
    }

    return best_node;
}

// ---------- COMPONENTS / CONNECTIVITY ----------
void compute_connected_components()
{
    node_component.clear();
    int comp_id = 0;
    std::vector<long> stack;
    for (const auto &p : nodes)
    {
        long nid = p.first;
        if (node_component.find(nid) != node_component.end())
            continue;
        if (graph.find(nid) == graph.end() || graph[nid].empty())
        {
            node_component[nid] = -1; // isolated
            continue;
        }
        comp_id++;
        stack.clear();
        stack.push_back(nid);
        node_component[nid] = comp_id;
        while (!stack.empty())
        {
            long cur = stack.back();
            stack.pop_back();
            if (graph.find(cur) == graph.end())
                continue;
            for (auto &e : graph[cur])
            {
                long nb = e.first;
                if (node_component.find(nb) == node_component.end())
                {
                    node_component[nb] = comp_id;
                    stack.push_back(nb);
                }
            }
        }
    }
    std::cerr << "Computed components, found " << comp_id << " components (isolated marked -1)\n";
}

long find_nearest_in_main_component(double lat, double lon)
{
    // find component frequencies
    std::unordered_map<int, int> comp_count;
    for (auto &p : node_component)
    {
        if (p.second > 0)
            comp_count[p.second]++;
    }
    int main_comp = -1;
    int maxc = 0;
    for (auto &q : comp_count)
        if (q.second > maxc)
        {
            maxc = q.second;
            main_comp = q.first;
        }
    if (main_comp == -1)
        return find_best_snap_node_fast(lat, lon);

    // search nearest among nodes in main_comp (linear but OK for snapping)
    long best = -1;
    double bd = std::numeric_limits<double>::max();
    for (auto &p : nodes)
    {
        long nid = p.first;
        if (node_component.find(nid) == node_component.end())
            continue;
        if (node_component[nid] != main_comp)
            continue;
        double d = haversine(lat, lon, p.second.lat, p.second.lon);
        if (d < bd)
        {
            bd = d;
            best = nid;
        }
    }
    return best;
}

void snap_all_students_fast()
{
    std::cout << "\n⚡ Snapping " << students.size() << " students to road network..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    int snapped = 0;
    int failed = 0;

    for (auto &student : students)
    {
        student.snapped_node_id = find_best_snap_node_fast(student.lat, student.lon);

        // fallback: avoid snapping into isolated or tiny components
        if (student.snapped_node_id != -1)
        {
            int comp = node_component.count(student.snapped_node_id) ? node_component[student.snapped_node_id] : -1;
            if (comp <= 0)
            { // isolated or not in main comp
                long alt = find_nearest_in_main_component(student.lat, student.lon);
                if (alt != -1)
                {
                    student.snapped_node_id = alt;
                }
            }
        }

        if (student.snapped_node_id == -1)
        {
            failed++;
        }
        else
        {
            snapped++;
        }

        if (snapped % 250 == 0)
        {
            std::cout << "  ✓ Snapped " << snapped << " students..." << std::endl;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "✅ Snapping complete: " << snapped << " snapped, " << failed << " failed in " << ms << "ms" << std::endl;
}

std::vector<long> clean_and_validate_path(const std::vector<long> &path)
{
    if (path.empty())
        return {};

    std::vector<long> cleaned_path;

    for (long node_id : path)
    {
        if (nodes.find(node_id) == nodes.end())
        {
            std::cerr << "⚠️  Path contains non-existent node: " << node_id << std::endl;
            continue;
        }

        if (graph.find(node_id) == graph.end() || graph[node_id].empty())
        {
            std::cerr << "⚠️  Path contains disconnected node: " << node_id << std::endl;
            continue;
        }

        cleaned_path.push_back(node_id);
    }

    return cleaned_path;
}

const double MAX_SPEED_MPS = 27.8;

double heuristic(long node1, long node2)
{
    if (nodes.find(node1) == nodes.end() || nodes.find(node2) == nodes.end())
        return 0.0;

    double distance_meters = haversine(nodes[node1].lat, nodes[node1].lon,
                                       nodes[node2].lat, nodes[node2].lon);

    double time_seconds = distance_meters / MAX_SPEED_MPS;

    return time_seconds;
}

// ==================== A* BIDIRECTIONAL ALGORITHM ====================

struct SearchNode
{
    long node_id;
    double g_score;
    double f_score;

    bool operator>(const SearchNode &other) const
    {
        return f_score > other.f_score;
    }
};

std::vector<long> a_star_bidirectional(long start_node, long goal_node)
{
    if (start_node == goal_node)
    {
        return {start_node};
    }

    if (graph.find(start_node) == graph.end() || graph.find(goal_node) == graph.end())
    {
        std::cerr << "⚠️  Start or goal node not in graph" << std::endl;
        return {};
    }

    std::unordered_map<long, double> g_score_forward, g_score_backward;
    std::unordered_map<long, long> came_from_forward, came_from_backward;
    std::priority_queue<SearchNode, std::vector<SearchNode>, std::greater<SearchNode>> open_forward, open_backward;
    std::set<long> closed_forward, closed_backward;

    g_score_forward[start_node] = 0.0;
    g_score_backward[goal_node] = 0.0;

    SearchNode start_search = {start_node, 0.0, heuristic(start_node, goal_node)};
    SearchNode goal_search = {goal_node, 0.0, heuristic(goal_node, start_node)};

    open_forward.push(start_search);
    open_backward.push(goal_search);

    long meeting_point = -1;
    int iterations = 0;
    const int MAX_ITERATIONS = 100000;

    while (!open_forward.empty() && !open_backward.empty() && iterations < MAX_ITERATIONS)
    {
        iterations++;

        if (!open_forward.empty())
        {
            auto current_search = open_forward.top();
            open_forward.pop();

            if (closed_forward.find(current_search.node_id) != closed_forward.end())
            {
                continue;
            }
            closed_forward.insert(current_search.node_id);

            if (closed_backward.find(current_search.node_id) != closed_backward.end())
            {
                meeting_point = current_search.node_id;
                break;
            }

            if (graph.find(current_search.node_id) != graph.end())
            {
                for (const auto &[neighbor, edge_weight] : graph[current_search.node_id])
                {
                    double tentative_g = g_score_forward[current_search.node_id] + edge_weight;

                    if (g_score_forward.find(neighbor) == g_score_forward.end() ||
                        tentative_g < g_score_forward[neighbor])
                    {

                        g_score_forward[neighbor] = tentative_g;
                        came_from_forward[neighbor] = current_search.node_id;

                        double f = tentative_g + heuristic(neighbor, goal_node);
                        open_forward.push({neighbor, tentative_g, f});
                    }
                }
            }
        }

        if (!open_backward.empty())
        {
            auto current_search = open_backward.top();
            open_backward.pop();

            if (closed_backward.find(current_search.node_id) != closed_backward.end())
            {
                continue;
            }
            closed_backward.insert(current_search.node_id);

            if (closed_forward.find(current_search.node_id) != closed_forward.end())
            {
                meeting_point = current_search.node_id;
                break;
            }

            if (graph.find(current_search.node_id) != graph.end())
            {
                for (const auto &[neighbor, edge_weight] : graph[current_search.node_id])
                {
                    double tentative_g = g_score_backward[current_search.node_id] + edge_weight;

                    if (g_score_backward.find(neighbor) == g_score_backward.end() ||
                        tentative_g < g_score_backward[neighbor])
                    {

                        g_score_backward[neighbor] = tentative_g;
                        came_from_backward[neighbor] = current_search.node_id;

                        double f = tentative_g + heuristic(neighbor, start_node);
                        open_backward.push({neighbor, tentative_g, f});
                    }
                }
            }
        }
    }

    if (meeting_point != -1)
    {
        std::vector<long> path_forward, path_backward;

        long node = meeting_point;
        while (came_from_forward.find(node) != came_from_forward.end())
        {
            path_forward.push_back(node);
            node = came_from_forward[node];
        }
        path_forward.push_back(start_node);
        std::reverse(path_forward.begin(), path_forward.end());

        node = meeting_point;
        while (came_from_backward.find(node) != came_from_backward.end())
        {
            path_backward.push_back(node);
            node = came_from_backward[node];
        }
        path_backward.push_back(goal_node);

        std::vector<long> full_path = path_forward;
        full_path.insert(full_path.end(), path_backward.begin(), path_backward.end());

        return full_path;
    }

    return {};
}

// ==================== OVERPASS API INTEGRATION ====================

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
}

std::string fetch_overpass_data(double min_lat, double min_lon, double max_lat, double max_lon, const std::string &graph_detail = "medium")
{
    std::cout << "Fetching real road data from Overpass API (detail: " << graph_detail << ")..." << std::endl;

    std::string highway_types;
    if (graph_detail == "low")
    {
        highway_types = "primary|secondary|tertiary";
        std::cout << "📉 Low detail: Major roads only (fastest)" << std::endl;
    }
    else if (graph_detail == "high")
    {
        highway_types = "motorway|trunk|primary|secondary|tertiary|residential|living_street|service|unclassified";
        std::cout << "📈 High detail: All roads (most accurate)" << std::endl;
    }
    else
    {
        highway_types = "primary|secondary|tertiary|residential|living_street|service|unclassified";
        std::cout << "📊 Medium detail: Most roads (balanced)" << std::endl;
    }

    std::stringstream query_stream;
    query_stream << std::fixed << std::setprecision(8);
    query_stream << "[out:json][timeout:60];";
    query_stream << "(";
    query_stream << "way[highway~\"" << highway_types << "\"]";
    query_stream << "(" << min_lat << "," << min_lon << "," << max_lat << "," << max_lon << ");";
    query_stream << "node(w);";
    query_stream << ");";
    query_stream << "out body;";
    std::string query = query_stream.str();

    std::cout << "Query: " << query << std::endl;

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
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
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
    return 30.0;
}

void build_graph_from_overpass(const json &osm_data)
{
    std::cout << "Building graph from real OSM node/way data..." << std::endl;

    nodes.clear();
    graph.clear();

    if (!osm_data.contains("elements") || osm_data["elements"].empty())
    {
        std::cerr << "ERROR: No valid elements in OSM data!" << std::endl;
        return;
    }

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

    int edge_count = 0;
    int oneway_count = 0;

    for (const auto &element : osm_data["elements"])
    {
        if (element["type"] == "way" && element.contains("nodes"))
        {
            std::string highway_type = "";
            bool is_oneway = false;
            double speed_kmh = 30.0;

            if (element.contains("tags"))
            {
                const auto &tags = element["tags"];

                if (tags.contains("highway"))
                {
                    highway_type = tags["highway"].get<std::string>();
                    speed_kmh = get_default_speed(highway_type);
                }

                if (tags.contains("oneway"))
                {
                    std::string oneway_val = tags["oneway"].get<std::string>();
                    is_oneway = (oneway_val == "yes" || oneway_val == "true" || oneway_val == "1");
                }

                if (tags.contains("maxspeed"))
                {
                    std::string maxspeed_str = tags["maxspeed"].get<std::string>();
                    try
                    {
                        speed_kmh = std::stod(maxspeed_str);
                    }
                    catch (...)
                    {
                    }
                }
            }

            const auto &way_node_ids = element["nodes"];

            for (size_t i = 0; i < way_node_ids.size() - 1; i++)
            {
                long node1_id = way_node_ids[i];
                long node2_id = way_node_ids[i + 1];

                if (nodes.find(node1_id) != nodes.end() && nodes.find(node2_id) != nodes.end())
                {
                    double dist_meters = haversine(
                        nodes[node1_id].lat, nodes[node1_id].lon,
                        nodes[node2_id].lat, nodes[node2_id].lon);

                    double dist_km = dist_meters / 1000.0;
                    double time_hours = dist_km / speed_kmh;
                    double time_seconds = time_hours * 3600.0;
                    double edge_weight = time_seconds;

                    if (is_oneway)
                    {
                        graph[node1_id].push_back({node2_id, edge_weight});
                        edge_count++;
                        oneway_count++;
                    }
                    else
                    {
                        graph[node1_id].push_back({node2_id, edge_weight});
                        graph[node2_id].push_back({node1_id, edge_weight});
                        edge_count += 2;
                    }
                }
            }
        }
    }

    std::cout << "Built graph with " << nodes.size() << " nodes and "
              << edge_count << " directed edges" << std::endl;
    std::cout << "Found " << oneway_count << " one-way street segments" << std::endl;

    // compute connected components now that graph built
    compute_connected_components();
}

void generate_simulated_graph_fallback(double min_lat, double min_lon, double max_lat, double max_lon)
{
    std::cout << "\n=== FALLBACK: Generating simulated road network ===" << std::endl;

    nodes.clear();
    graph.clear();

    const int GRID_SIZE = 80;
    double lat_step = (max_lat - min_lat) / GRID_SIZE;
    double lon_step = (max_lon - min_lon) / GRID_SIZE;

    long node_id = 1;
    std::vector<std::vector<long>> grid_nodes(GRID_SIZE, std::vector<long>(GRID_SIZE));

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

    for (int i = 0; i < GRID_SIZE; i++)
    {
        for (int j = 0; j < GRID_SIZE; j++)
        {
            long current = grid_nodes[i][j];

            std::vector<std::pair<int, int>> directions = {
                {0, 1}, {1, 0}, {1, 1}, {1, -1}, {0, -1}, {-1, 0}, {-1, -1}, {-1, 1}};

            for (const auto &dir : directions)
            {
                int ni = i + dir.first;
                int nj = j + dir.second;

                if (ni >= 0 && ni < GRID_SIZE && nj >= 0 && nj < GRID_SIZE)
                {
                    long neighbor = grid_nodes[ni][nj];
                    double dist = haversine(nodes[current].lat, nodes[current].lon,
                                            nodes[neighbor].lat, nodes[neighbor].lon);

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

    std::cout << "Simulated graph: " << nodes.size() << " nodes" << std::endl;

    // compute components for simulated graph
    compute_connected_components();
}

// ==================== DIJKSTRA ALGORITHM ====================

std::unordered_map<long, double> dijkstra(long start_node)
{
    std::unordered_map<long, double> distances;
    std::priority_queue<std::pair<double, long>,
                        std::vector<std::pair<double, long>>,
                        std::greater<std::pair<double, long>>>
        pq;

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
            continue;

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

// ==================== PARALLEL DIJKSTRA FUNCTIONS ====================

struct DijkstraResult
{
    std::string centre_id;
    long start_node;
    std::unordered_map<long, double> distances;
    std::unordered_map<long, long> parents;
    long long computation_time_ms;
    bool success;
    std::string error_message;
};

// Modified Dijkstra that also tracks parents for path reconstruction
std::pair<std::unordered_map<long, double>, std::unordered_map<long, long>>
dijkstra_with_parents(long start_node)
{
    std::unordered_map<long, double> distances;
    std::unordered_map<long, long> parents;
    std::priority_queue<std::pair<double, long>,
                        std::vector<std::pair<double, long>>,
                        std::greater<std::pair<double, long>>>
        pq;

    for (const auto &[node_id, _] : nodes)
    {
        distances[node_id] = std::numeric_limits<double>::max();
        parents[node_id] = -1;
    }
    distances[start_node] = 0.0;
    parents[start_node] = start_node;

    pq.push({0.0, start_node});

    while (!pq.empty())
    {
        auto [current_dist, current_node] = pq.top();
        pq.pop();

        if (current_dist > distances[current_node])
            continue;

        if (graph.find(current_node) != graph.end())
        {
            for (const auto &[neighbor, edge_weight] : graph[current_node])
            {
                double new_dist = current_dist + edge_weight;

                if (new_dist < distances[neighbor])
                {
                    distances[neighbor] = new_dist;
                    parents[neighbor] = current_node;
                    pq.push({new_dist, neighbor});
                }
            }
        }
    }

    return {distances, parents};
}

// Function to run Dijkstra for a single centre (used in parallel execution)
DijkstraResult run_dijkstra_for_centre(const Centre &centre)
{
    DijkstraResult result;
    result.centre_id = centre.centre_id;
    result.start_node = centre.snapped_node_id;
    result.success = false;

    try
    {
        auto start_time = std::chrono::high_resolution_clock::now();

        auto [distances, parents] = dijkstra_with_parents(centre.snapped_node_id);

        auto end_time = std::chrono::high_resolution_clock::now();
        result.computation_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                         end_time - start_time)
                                         .count();

        result.distances = std::move(distances);
        result.parents = std::move(parents);
        result.success = true;

        std::cout << "✓ Completed Dijkstra for " << centre.centre_id
                  << " in " << result.computation_time_ms << "ms" << std::endl;
    }
    catch (const std::exception &e)
    {
        result.error_message = e.what();
        std::cerr << "✗ Error in Dijkstra for " << centre.centre_id
                  << ": " << e.what() << std::endl;
    }

    return result;
}

// Save Dijkstra results to JSON files
bool save_dijkstra_results(const DijkstraResult &result,
                           const std::string &distances_file,
                           const std::string &parents_file)
{
    try
    {
        // Save distances
        json distances_json = json::object();
        for (const auto &[node_id, dist] : result.distances)
        {
            if (dist != std::numeric_limits<double>::max())
            {
                distances_json[std::to_string(node_id)] = dist;
            }
        }

        std::ofstream dist_out(distances_file);
        if (!dist_out.is_open())
        {
            std::cerr << "Failed to open " << distances_file << std::endl;
            return false;
        }
        dist_out << distances_json.dump(2);
        dist_out.close();

        // Save parents
        json parents_json = json::object();
        for (const auto &[node_id, parent] : result.parents)
        {
            if (parent != -1)
            {
                parents_json[std::to_string(node_id)] = parent;
            }
        }

        std::ofstream parent_out(parents_file);
        if (!parent_out.is_open())
        {
            std::cerr << "Failed to open " << parents_file << std::endl;
            return false;
        }
        parent_out << parents_json.dump(2);
        parent_out.close();

        std::cout << "✓ Saved results for " << result.centre_id << std::endl;
        std::cout << "  - Distances: " << distances_file << std::endl;
        std::cout << "  - Parents: " << parents_file << std::endl;

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error saving results: " << e.what() << std::endl;
        return false;
    }
}

// ==================== A* ALGORITHM ====================

std::vector<long> a_star(long start_node, long goal_node)
{
    std::unordered_map<long, double> g_score;
    std::unordered_map<long, double> f_score;
    std::unordered_map<long, long> came_from;

    auto compare = [&f_score](long a, long b)
    { return f_score[a] > f_score[b]; };
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

    return {};
}

// ==================== ALLOTMENT LOOKUP ====================

void build_allotment_lookup()
{
    std::cout << "Building allotment lookup map..." << std::endl;

    allotment_lookup_map.clear();

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

// ==================== DISTANCE-FIRST PRIORITY QUEUE ALLOTMENT ====================

// HELPER: Check if a student-centre assignment is valid
bool is_valid_assignment(const Student &student, const Centre &centre)
{
    // All constraints removed as per new logic.
    // All centres are identical and accept all student categories.
    return true;
}

// HELPER: Process a single priority queue batch (Distance-First)
void process_priority_queue(
    std::priority_queue<AssignmentPair, std::vector<AssignmentPair>, std::greater<AssignmentPair>> &pq,
    std::set<std::string> &assigned_students,
    std::unordered_map<std::string, int> &centre_loads)
{
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
            continue;

        // Check if this centre is full
        if (centre_loads[assignment.centre_id] >= target_centre->max_capacity)
        {
            continue;
        }

        // Make assignment
        final_assignments[assignment.student_id] = assignment.centre_id;
        assigned_students.insert(assignment.student_id);
        centre_loads[assignment.centre_id]++;
        target_centre->current_load = centre_loads[assignment.centre_id];
    }
}

// NEW: Distance-First Tiered Allotment Algorithm
void run_batch_greedy_allotment()
{
    std::cout << "\n🎯 Running TIERED DISTANCE-FIRST Allotment..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    std::set<std::string> assigned_students;
    std::unordered_map<std::string, int> centre_loads;
    for (auto &centre : centres)
    {
        centre_loads[centre.centre_id] = 0;
        centre.current_load = 0;
    }

    final_assignments.clear();

    // Separate students into batches (Male > PwD > Female priority based on commented code)
    std::vector<Student> female_students, pwd_students, male_students;
    for (const auto &student : students)
    {
        if (student.category == "female")
            female_students.push_back(student);
        else if (student.category == "pwd")
            pwd_students.push_back(student);
        else
            male_students.push_back(student);
    }

    std::cout << "📊 Distribution: Female=" << female_students.size()
              << " | PwD=" << pwd_students.size()
              << " | Male=" << male_students.size() << std::endl;

    // --- TIER 1: MALE (based on current priority) ---
    std::cout << "\n🟢 BATCH 1: Processing " << male_students.size() << " Male students..." << std::endl;
    std::priority_queue<AssignmentPair, std::vector<AssignmentPair>, std::greater<AssignmentPair>> pq_male;
    for (const auto &student : male_students)
    {
        if (allotment_lookup_map.find(student.snapped_node_id) == allotment_lookup_map.end())
            continue;
        const auto &centre_distances = allotment_lookup_map[student.snapped_node_id];
        for (const auto &centre : centres)
        {
            if (is_valid_assignment(student, centre))
            {
                auto it = centre_distances.find(centre.centre_id);
                if (it != centre_distances.end() && it->second != std::numeric_limits<double>::max())
                {
                    pq_male.push({it->second, student.student_id, centre.centre_id});
                }
            }
        }
    }
    process_priority_queue(pq_male, assigned_students, centre_loads);
    std::cout << "✅ Assigned " << assigned_students.size() << " male students" << std::endl;

    // --- TIER 2: PWD ---
    std::cout << "\n🔵 BATCH 2: Processing " << pwd_students.size() << " PwD students..." << std::endl;
    std::priority_queue<AssignmentPair, std::vector<AssignmentPair>, std::greater<AssignmentPair>> pq_pwd;
    for (const auto &student : pwd_students)
    {
        if (allotment_lookup_map.find(student.snapped_node_id) == allotment_lookup_map.end())
            continue;
        const auto &centre_distances = allotment_lookup_map[student.snapped_node_id];
        for (const auto &centre : centres)
        {
            if (is_valid_assignment(student, centre))
            {
                auto it = centre_distances.find(centre.centre_id);
                if (it != centre_distances.end() && it->second != std::numeric_limits<double>::max())
                {
                    pq_pwd.push({it->second, student.student_id, centre.centre_id});
                }
            }
        }
    }
    size_t prev_count = assigned_students.size();
    process_priority_queue(pq_pwd, assigned_students, centre_loads);
    std::cout << "✅ Assigned " << (assigned_students.size() - prev_count) << " PwD students" << std::endl;

    // --- TIER 3: FEMALE ---
    std::cout << "\n🟣 BATCH 3: Processing " << female_students.size() << " Female students..." << std::endl;
    std::priority_queue<AssignmentPair, std::vector<AssignmentPair>, std::greater<AssignmentPair>> pq_female;
    for (const auto &student : female_students)
    {
        if (allotment_lookup_map.find(student.snapped_node_id) == allotment_lookup_map.end())
            continue;
        const auto &centre_distances = allotment_lookup_map[student.snapped_node_id];
        for (const auto &centre : centres)
        {
            if (is_valid_assignment(student, centre))
            {
                auto it = centre_distances.find(centre.centre_id);
                if (it != centre_distances.end() && it->second != std::numeric_limits<double>::max())
                {
                    pq_female.push({it->second, student.student_id, centre.centre_id});
                }
            }
        }
    }
    prev_count = assigned_students.size();
    process_priority_queue(pq_female, assigned_students, centre_loads);
    std::cout << "✅ Assigned " << (assigned_students.size() - prev_count) << " female students" << std::endl;

    auto end = std::chrono::high_resolution_clock::now();
    long long total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "\n🎉 TIERED ALLOTMENT COMPLETE! Total Assigned: " << assigned_students.size()
              << " / " << students.size() << " students in " << total_ms << "ms" << std::endl;
}

// ==================== OLD SINGLE-PASS ALLOTMENT (DEPRECATED) ====================

// Helper function for deprecated run_allotment_single_pass
void run_local_swap_postprocess(std::unordered_map<std::string, std::vector<std::pair<std::string, long>>> &centre_to_students,
                                const std::unordered_map<std::string, std::unordered_map<long, double>> &centre_distances_map)
{
    // try simple swaps: for each pair of centres, try swapping a few students
    for (size_t i = 0; i < centres.size(); ++i)
    {
        for (size_t j = i + 1; j < centres.size(); ++j)
        {
            std::string c1 = centres[i].centre_id;
            std::string c2 = centres[j].centre_id;
            auto &list1 = centre_to_students[c1];
            auto &list2 = centre_to_students[c2];
            // only test small samples to keep it fast
            int limit = 40; // try up to 40x40 combinations
            for (int a = 0; a < (int)list1.size() && a < limit; ++a)
            {
                for (int b = 0; b < (int)list2.size() && b < limit; ++b)
                {
                    long n1 = list1[a].second;
                    long n2 = list2[b].second;
                    double before = centre_distances_map.at(c1).at(n1) + centre_distances_map.at(c2).at(n2);
                    double after = centre_distances_map.at(c1).at(n2) + centre_distances_map.at(c2).at(n1);
                    if (after + 1e-9 < before)
                    {
                        // accept swap if it reduces total cost
                        // swap in final_assignments
                        final_assignments[list1[a].first] = c2;
                        final_assignments[list2[b].first] = c1;
                        // update local lists to reflect swap
                        std::swap(list1[a], list2[b]);
                    }
                }
            }
        }
    }
}

void run_allotment_single_pass()
{
    std::cout << "\n⚡⚡ Running ULTRA-FAST Single-Pass Allotment..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    std::unordered_map<std::string, int> centre_remaining_capacity;
    for (const auto &centre : centres)
    {
        centre_remaining_capacity[centre.centre_id] = centre.max_capacity;
    }

    std::vector<Student> female_students, pwd_students, male_students;

    for (const auto &student : students)
    {
        if (student.category == "female")
        {
            female_students.push_back(student);
        }
        else if (student.category == "pwd")
        {
            pwd_students.push_back(student);
        }
        else
        {
            male_students.push_back(student);
        }
    }

    std::cout << "📊 Distribution: Female=" << female_students.size()
              << " | PwD=" << pwd_students.size()
              << " | Male=" << male_students.size() << std::endl;

    int total_assigned = 0;

    // We still expect a centre_distances_map prepared by caller (or use allotment_lookup_map)
    // Build a centre_distances_map from allotment_lookup_map for quick local use
    std::unordered_map<std::string, std::unordered_map<long, double>> centre_distances_map;
    for (const auto &centre : centres)
    {
        for (const auto &p : nodes)
        {
            long node_id = p.first;
            double d = std::numeric_limits<double>::max();
            if (allotment_lookup_map.find(node_id) != allotment_lookup_map.end())
            {
                auto &m = allotment_lookup_map[node_id];
                if (m.find(centre.centre_id) != m.end())
                    d = m.at(centre.centre_id);
            }
            centre_distances_map[centre.centre_id][node_id] = d;
        }
    }

    final_assignments.clear();

    auto process_tier = [&](const std::vector<Student> &tier_students, const std::string &tier_name)
    {
        std::cout << "\n"
                  << tier_name << " Processing " << tier_students.size() << " students..." << std::endl;
        auto tier_start = std::chrono::high_resolution_clock::now();

        int tier_assigned = 0;
        for (const auto &student : tier_students)
        {
            if (allotment_lookup_map.find(student.snapped_node_id) == allotment_lookup_map.end())
            {
                continue;
            }

            // selection with deterministic tie-break and capacity bias
            double best_distance = std::numeric_limits<double>::max();
            std::string best_centre_id;
            double best_secondary_metric = std::numeric_limits<double>::max(); // euclid

            for (const auto &centre : centres)
            {
                if (centre_remaining_capacity[centre.centre_id] <= 0)
                    continue;
                // lookup
                double distance = std::numeric_limits<double>::max();
                auto itnode = centre_distances_map.find(centre.centre_id);
                if (itnode != centre_distances_map.end())
                {
                    auto &cd = itnode->second;
                    if (cd.find(student.snapped_node_id) != cd.end())
                        distance = cd.at(student.snapped_node_id);
                }
                if (distance == std::numeric_limits<double>::max())
                    continue;

                // secondary metric: Euclidean (meters) to center to break near ties
                double eu = haversine(student.lat, student.lon, centre.lat, centre.lon);

                const double NEAR_TIE_M = 20.0;
                bool take = false;
                if (distance + 1e-9 < best_distance)
                {
                    take = true;
                }
                else if (std::abs(distance - best_distance) <= NEAR_TIE_M)
                {
                    if (eu + 1e-9 < best_secondary_metric)
                        take = true;
                    else if (std::abs(eu - best_secondary_metric) <= 1e-6)
                    {
                        if (best_centre_id.empty() || centre.centre_id < best_centre_id)
                            take = true;
                    }
                }
                if (!take && std::abs(distance - best_distance) <= NEAR_TIE_M && !best_centre_id.empty())
                {
                    if (centre_remaining_capacity[centre.centre_id] > centre_remaining_capacity[best_centre_id])
                    {
                        take = true;
                    }
                }

                if (take)
                {
                    best_distance = distance;
                    best_centre_id = centre.centre_id;
                    best_secondary_metric = eu;
                }
            }

            // Only assign if we found a reachable centre (best_distance < infinity)
            if (!best_centre_id.empty() && best_distance != std::numeric_limits<double>::max())
            {
                final_assignments[student.student_id] = best_centre_id;
                centre_remaining_capacity[best_centre_id]--;
                tier_assigned++;
            }
        }

        auto tier_end = std::chrono::high_resolution_clock::now();
        long long tier_ms = std::chrono::duration_cast<std::chrono::milliseconds>(tier_end - tier_start).count();

        std::cout << "✅ " << tier_name << ": " << tier_assigned << " assigned in " << tier_ms << "ms" << std::endl;
        return tier_assigned;
    };

    total_assigned += process_tier(female_students, "🟣 TIER 1 (Female)");
    total_assigned += process_tier(pwd_students, "🔵 TIER 2 (PwD)");
    total_assigned += process_tier(male_students, "🟢 TIER 3 (Male)");

    // Build reverse mapping centre -> list of student node_ids for swaps
    std::unordered_map<std::string, std::vector<std::pair<std::string, long>>> centre_to_students;
    for (const auto &s : students)
    {
        auto it = final_assignments.find(s.student_id);
        if (it == final_assignments.end())
            continue;
        std::string cid = it->second;
        centre_to_students[cid].push_back({s.student_id, s.snapped_node_id});
    }

    // run local-swap postprocess (cheap)
    run_local_swap_postprocess(centre_to_students, centre_distances_map);

    auto end = std::chrono::high_resolution_clock::now();
    long long total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "\n🎉 ALLOTMENT COMPLETE!" << std::endl;
    std::cout << "   Total Assigned: " << final_assignments.size() << " / " << students.size() << std::endl;
    std::cout << "   Time: " << total_ms << "ms" << std::endl;
}

// ==================== HTTP SERVER ====================

int main()
{
    httplib::Server server;

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
            
            // --- FIX: SAFE ACCESS FOR BOUNDS ---
            // Use .value("key", default_value) to safely get data or a default
            double min_lat = body.value("min_lat", 26.0);
            double min_lon = body.value("min_lon", 72.0);
            double max_lat = body.value("max_lat", 27.0);
            double max_lon = body.value("max_lon", 74.0);
            
            // --- FIX: SAFE ACCESS FOR GRAPH_DETAIL ---
            std::string graph_detail = body.value("graph_detail", "medium");
            std::cout << "📊 Graph detail level: " << graph_detail << std::endl;
            
            // --- FIX: SAFE ACCESS FOR CENTRES ARRAY & NESTED KEYS ---
            centres.clear();
            if (body.contains("centres") && body["centres"].is_array()) 
            {
                for (const auto& c : body["centres"]) {
                    // Check that 'c' is actually an object before accessing
                    if (c.is_object()) {
                        Centre centre;
                        
                        // Use .value() for safe nested access
                        centre.centre_id = c.value("centre_id", "default_id");
                        centre.lat = c.value("lat", 0.0);
                        centre.lon = c.value("lon", 0.0);
                        centre.max_capacity = c.value("max_capacity", 500);
                        centre.current_load = 0;
                        centre.has_wheelchair_access = c.value("has_wheelchair_access", false);
                        centre.is_female_only = c.value("is_female_only", false);
                        
                        centres.push_back(centre);
                    }
                }
            } else {
                std::cout << "⚠️  WARNING: No 'centres' array found in request body." << std::endl;
            }
            
            auto time_fetch_start = std::chrono::high_resolution_clock::now();
            std::string osm_json_string = fetch_overpass_data(min_lat, min_lon, max_lat, max_lon, graph_detail);
            auto time_fetch_end = std::chrono::high_resolution_clock::now();
            
            json osm_data = json::parse(osm_json_string);
            
            auto time_build_graph_start = std::chrono::high_resolution_clock::now();
            build_graph_from_overpass(osm_data);
            auto time_build_graph_end = std::chrono::high_resolution_clock::now();
            
            if (nodes.empty()) {
                std::cout << "\n⚠️  Overpass API failed - using simulated graph fallback" << std::endl;
                generate_simulated_graph_fallback(min_lat, min_lon, max_lat, max_lon);
            }
            
            auto time_kdtree_start = std::chrono::high_resolution_clock::now();
            std::cout << "\n🌳 Building KD-Tree for " << nodes.size() << " nodes..." << std::endl;
            
            std::vector<std::pair<long, std::pair<double, double>>> node_points;
            for (const auto &[node_id, node] : nodes) {
                if (graph.find(node_id) != graph.end() && !graph[node_id].empty()) {
                    node_points.push_back({node_id, {node.lat, node.lon}});
                }
            }
            
            std::cout << "  ✅ Filtered to " << node_points.size() << " connected nodes" << std::endl;
            
            if (kdtree_root) {
                kdtree_root = nullptr;
            }
            
            kdtree_root = build_kdtree(node_points, 0);
            auto time_kdtree_end = std::chrono::high_resolution_clock::now();
            std::cout << "✅ KD-Tree built successfully" << std::endl;
            
            for (auto& centre : centres) {
                centre.snapped_node_id = find_nearest_node(centre.lat, centre.lon);
            }
            
            auto time_dijkstra_start = std::chrono::high_resolution_clock::now();
            build_allotment_lookup();
            auto time_dijkstra_end = std::chrono::high_resolution_clock::now();
            
            long long time_fetch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_fetch_end - time_fetch_start).count();
            long long time_build_graph_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_build_graph_end - time_build_graph_start).count();
            long long time_kdtree_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_kdtree_end - time_kdtree_start).count();
            long long time_dijkstra_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_dijkstra_end - time_dijkstra_start).count();
            
            json response;
            response["status"] = "success";
            response["nodes_count"] = nodes.size();
            response["edges_count"] = graph.size();
            
            response["timing"] = {
                {"fetch_overpass_ms", time_fetch_ms},
                {"build_graph_ms", time_build_graph_ms},
                {"build_kdtree_ms", time_kdtree_ms},
                {"dijkstra_precompute_ms", time_dijkstra_ms},
                {"total_ms", time_fetch_ms + time_build_graph_ms + time_kdtree_ms + time_dijkstra_ms}
            };
            
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
            auto time_start = std::chrono::high_resolution_clock::now();
            auto body = json::parse(req.body);
            
            // STEP 1: Snap students to graph nodes (using improved snapping)
            auto time_snap_start = std::chrono::high_resolution_clock::now();
            students.clear();
            for (const auto& s : body["students"]) {
                Student student;
                student.student_id = s["student_id"];
                student.lat = s["lat"];
                student.lon = s["lon"];
                student.category = s["category"];
                student.snapped_node_id = find_best_snap_node_fast(student.lat, student.lon);  // Use improved snapping

                // fallback: ensure snapped node in main component
                if (student.snapped_node_id != -1) {
                    int comp = node_component.count(student.snapped_node_id) ? node_component[student.snapped_node_id] : -1;
                    if (comp <= 0) {
                        long alt = find_nearest_in_main_component(student.lat, student.lon);
                        if (alt != -1) student.snapped_node_id = alt;
                    }
                }

                students.push_back(student);
            }
            auto time_snap_end = std::chrono::high_resolution_clock::now();
            long long time_snap_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_snap_end - time_snap_start).count();
            
            // STEP 2: Build Dijkstra cache from each centre
            std::cout << "\n📍 Computing distances from centres..." << std::endl;
            auto time_dijkstra_start = std::chrono::high_resolution_clock::now();
            
            std::unordered_map<std::string, std::unordered_map<long, double>> centre_distances_map;
            
            for (const auto &centre : centres) {
                std::cout << "  Dijkstra from " << centre.centre_id << "..." << std::endl;
                centre_distances_map[centre.centre_id] = dijkstra(centre.snapped_node_id);
            }

            // Update global allotment lookup map (so diagnostics and swaps can use it)
            allotment_lookup_map.clear();
            for (const auto &c : centres) {
                const auto &dmap = centre_distances_map[c.centre_id];
                for (const auto &p : dmap) {
                    allotment_lookup_map[p.first][c.centre_id] = p.second;
                }
            }
            
            auto time_dijkstra_end = std::chrono::high_resolution_clock::now();
            long long time_dijkstra_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_dijkstra_end - time_dijkstra_start).count();
            
            // STEP 3: Run Distance-First Priority Queue Allotment
            std::cout << "\n🎯 Running Distance-First allotment..." << std::endl;
            auto time_allotment_start = std::chrono::high_resolution_clock::now();
            
            // Call the new distance-first algorithm
            run_batch_greedy_allotment();
            
            auto time_allotment_end = std::chrono::high_resolution_clock::now();
            long long time_allotment_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_allotment_end - time_allotment_start).count();
            
            auto time_end = std::chrono::high_resolution_clock::now();
            long long time_total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_end - time_start).count();
            
            std::cout << "\n🎉 ALLOTMENT COMPLETE!" << std::endl;
            std::cout << "   Total Assigned: " << final_assignments.size() << " / " << students.size() << std::endl;
            std::cout << "   Snap Time: " << time_snap_ms << "ms" << std::endl;
            std::cout << "   Dijkstra Time: " << time_dijkstra_ms << "ms" << std::endl;
            std::cout << "   Allotment Time: " << time_allotment_ms << "ms" << std::endl;
            std::cout << "   Total Time: " << time_total_ms << "ms" << std::endl;
            
            json response;
            response["status"] = "success";
            response["assignments"] = final_assignments;
            
            // --- NEW DEBUGGING CODE ---
            json all_distances;
            for (const auto& student : students) {
                if (allotment_lookup_map.find(student.snapped_node_id) != allotment_lookup_map.end()) {
                    // Add all pre-computed distances for this student
                    all_distances[student.student_id] = allotment_lookup_map[student.snapped_node_id];
                } else {
                    // Student's node wasn't in the map (e.g., snapped to -1)
                    all_distances[student.student_id] = json::object();
                }
            }
            response["debug_distances"] = all_distances;
            // --- END NEW DEBUGGING CODE ---
            
            response["timing"] = {
                {"snap_students_ms", time_snap_ms},
                {"dijkstra_ms", time_dijkstra_ms},
                {"allotment_ms", time_allotment_ms},
                {"total_ms", time_total_ms}
            };
            
            res.set_content(response.dump(), "application/json");
            
        } catch (const std::exception& e) {
            json error_response;
            error_response["status"] = "error";
            error_response["message"] = e.what();
            res.set_content(error_response.dump(), "application/json");
        } });

    // ========== /export-diagnostics endpoint ==========
    server.Get("/export-diagnostics", [](const httplib::Request &req, httplib::Response &res)
               {
        try {
            auto now = std::chrono::system_clock::now();
            std::time_t now_time = std::chrono::system_clock::to_time_t(now);
            char timestamp[100];
            std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", std::gmtime(&now_time));
            
            json diagnostic_report;
            
            // Metadata
            diagnostic_report["metadata"] = {
                {"run_id", "run_" + std::string(timestamp)},
                {"timestamp", timestamp},
                {"city", "Unnamed"},
                {"num_students", students.size()},
                {"num_centres", centres.size()},
                {"capacity_per_centre", centres.empty() ? 0 : centres[0].max_capacity},
                {"notes", "Detailed diagnostic export"}
            };
            
            // Count assignments per centre
            std::unordered_map<std::string, int> centre_assignment_count;
            for (const auto &centre : centres) {
                centre_assignment_count[centre.centre_id] = 0;
            }
            for (const auto &[student_id, centre_id] : final_assignments) {
                centre_assignment_count[centre_id]++;
            }
            
            // Export centres
            json centres_json = json::array();
            for (const auto &centre : centres) {
                centres_json.push_back({
                    {"centre_id", centre.centre_id},
                    {"lat", centre.lat},
                    {"lon", centre.lon},
                    {"graph_node_id", centre.snapped_node_id},
                    {"assigned_students", centre_assignment_count[centre.centre_id]}
                });
            }
            diagnostic_report["centres"] = centres_json;
            
            // Export students with diagnostics
            json students_json = json::array();
            int unreachable_count = 0;
            int large_snap_count = 0;
            double sum_snap_distance = 0;
            int snap_count = 0;
            
            for (const auto &student : students) {
                json student_json;
                student_json["student_id"] = student.student_id;
                student_json["lat"] = student.lat;
                student_json["lon"] = student.lon;
                student_json["category"] = student.category;
                student_json["snap_node_id"] = student.snapped_node_id;
                
                // Calculate snap distance
                if (nodes.find(student.snapped_node_id) != nodes.end()) {
                    const Node &snapped_node = nodes[student.snapped_node_id];
                    double snap_dist = haversine(student.lat, student.lon, snapped_node.lat, snapped_node.lon);
                    student_json["snap_distance_m"] = snap_dist;
                    sum_snap_distance += snap_dist;
                    snap_count++;
                    if (snap_dist > 100) large_snap_count++;
                } else {
                    student_json["snap_distance_m"] = -1;
                }
                
                // Assignment info
                bool is_assigned = final_assignments.find(student.student_id) != final_assignments.end();
                student_json["assigned_centre_id"] = is_assigned ? final_assignments[student.student_id] : nullptr;
                
                // alt distances & reachability & component
                std::map<std::string,double> alt;
                int reachable_centres = 0;
                double best = std::numeric_limits<double>::max();
                double second_best = std::numeric_limits<double>::max();
                std::string best_id = "";
                for (const auto &centre : centres) {
                    double d = std::numeric_limits<double>::max();
                    if (allotment_lookup_map.find(student.snapped_node_id) != allotment_lookup_map.end()) {
                        auto &m = allotment_lookup_map[student.snapped_node_id];
                        if (m.find(centre.centre_id) != m.end()) d = m.at(centre.centre_id);
                    }
                    alt[centre.centre_id] = d;
                    if (d < std::numeric_limits<double>::max()) reachable_centres++;
                    if (d < best) { second_best = best; best = d; best_id = centre.centre_id; }
                    else if (d < second_best) second_best = d;
                }
                student_json["alt_distances_m"] = alt;
                student_json["component_id"] = node_component.count(student.snapped_node_id) ? node_component[student.snapped_node_id] : -1;
                student_json["reachable_count"] = reachable_centres;
                student_json["near_tie"] = (second_best < std::numeric_limits<double>::max() && std::abs(second_best - best) < 20.0);

                if (!is_assigned) unreachable_count++;
                
                students_json.push_back(student_json);
            }
            diagnostic_report["students"] = students_json;
            
            // Summary statistics
            diagnostic_report["summary"] = {
                {"unreachable_count", unreachable_count},
                {"large_snap_count", large_snap_count},
                {"avg_snap_distance_m", snap_count > 0 ? sum_snap_distance / snap_count : 0}
            };
            
            res.set_content(diagnostic_report.dump(2), "application/json");
            
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
            auto time_start = std::chrono::high_resolution_clock::now();
            
            std::vector<long> student_candidates;
            std::vector<long> centre_candidates;
            
            if (req.has_param("student_node_id") && req.has_param("centre_node_id")) {
                student_candidates.push_back(std::stol(req.get_param_value("student_node_id")));
                centre_candidates.push_back(std::stol(req.get_param_value("centre_node_id")));
            } else if (req.has_param("student_lat") && req.has_param("student_lon") &&
                       req.has_param("centre_lat") && req.has_param("centre_lon")) {
                double student_lat = std::stod(req.get_param_value("student_lat"));
                double student_lon = std::stod(req.get_param_value("student_lon"));
                double centre_lat = std::stod(req.get_param_value("centre_lat"));
                double centre_lon = std::stod(req.get_param_value("centre_lon"));
                
                student_candidates = find_k_nearest_nodes(student_lat, student_lon, 5);
                centre_candidates = find_k_nearest_nodes(centre_lat, centre_lon, 5);
                
                std::cout << "Finding path: trying " << student_candidates.size() 
                          << "x" << centre_candidates.size() << " combinations" << std::endl;
            } else {
                throw std::runtime_error("Missing required parameters");
            }
            
            auto time_astar_start = std::chrono::high_resolution_clock::now();
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
            auto time_astar_end = std::chrono::high_resolution_clock::now();
            
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
            
            auto time_end = std::chrono::high_resolution_clock::now();
            long long time_astar_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_astar_end - time_astar_start).count();
            long long time_total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_end - time_start).count();
            
            response["timing"] = {
                {"astar_ms", time_astar_ms},
                {"total_ms", time_total_ms}
            };
            
            res.set_content(response.dump(), "application/json");
            
        } catch (const std::exception& e) {
            json error_response;
            error_response["status"] = "error";
            error_response["message"] = e.what();
            res.set_content(error_response.dump(), "application/json");
        } });

    // ========== /parallel-dijkstra endpoint ==========
    server.Post("/parallel-dijkstra", [](const httplib::Request &req, httplib::Response &res)
                {
        try {
            auto start_time = std::chrono::high_resolution_clock::now();
            
            auto body = json::parse(req.body);
            
            std::string workflow_name = body.value("workflow_name", "Parallel_Dijkstra");
            std::string workflow_type = body.value("workflow_type", "parallel");
            
            std::cout << "\n🚀 Starting " << workflow_name << std::endl;
            std::cout << "   Type: " << workflow_type << std::endl;
            
            if (centres.empty()) {
                json error_response;
                error_response["status"] = "error";
                error_response["message"] = "No centres loaded. Please call /build-graph first.";
                res.set_content(error_response.dump(), "application/json");
                return;
            }
            
            if (nodes.empty() || graph.empty()) {
                json error_response;
                error_response["status"] = "error";
                error_response["message"] = "Graph not built. Please call /build-graph first.";
                res.set_content(error_response.dump(), "application/json");
                return;
            }
            
            std::cout << "   Processing " << centres.size() << " centres..." << std::endl;
            
            // Parallel execution using std::async
            std::vector<std::future<DijkstraResult>> futures;
            
            auto parallel_start = std::chrono::high_resolution_clock::now();
            
            // Launch async tasks for each centre
            for (const auto &centre : centres) {
                futures.push_back(std::async(std::launch::async, 
                                            run_dijkstra_for_centre, 
                                            std::ref(centre)));
            }
            
            // Collect results
            std::vector<DijkstraResult> results;
            for (auto &future : futures) {
                results.push_back(future.get());
            }
            
            auto parallel_end = std::chrono::high_resolution_clock::now();
            long long parallel_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                parallel_end - parallel_start).count();
            
            // Save results if requested
            bool save_to_files = body.value("save_to_files", false);
            std::string output_dir = body.value("output_dir", "./");
            
            int successful_count = 0;
            int failed_count = 0;
            long long total_computation_time = 0;
            
            json results_json = json::array();
            
            for (const auto &result : results) {
                json result_obj;
                result_obj["centre_id"] = result.centre_id;
                result_obj["start_node"] = result.start_node;
                result_obj["success"] = result.success;
                result_obj["computation_time_ms"] = result.computation_time_ms;
                
                if (result.success) {
                    successful_count++;
                    total_computation_time += result.computation_time_ms;
                    
                    result_obj["reachable_nodes"] = 0;
                    for (const auto &[node, dist] : result.distances) {
                        if (dist != std::numeric_limits<double>::max()) {
                            result_obj["reachable_nodes"] = 
                                result_obj["reachable_nodes"].get<int>() + 1;
                        }
                    }
                    
                    // Save to files if requested
                    if (save_to_files) {
                        std::string dist_file = output_dir + result.centre_id + "_distances.json";
                        std::string parent_file = output_dir + result.centre_id + "_parents.json";
                        
                        bool saved = save_dijkstra_results(result, dist_file, parent_file);
                        result_obj["saved_to_files"] = saved;
                        if (saved) {
                            result_obj["distances_file"] = dist_file;
                            result_obj["parents_file"] = parent_file;
                        }
                    }
                } else {
                    failed_count++;
                    result_obj["error_message"] = result.error_message;
                }
                
                results_json.push_back(result_obj);
            }
            
            auto end_time = std::chrono::high_resolution_clock::now();
            long long total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - start_time).count();
            
            // Calculate speedup
            double avg_sequential_time = successful_count > 0 ? 
                (double)total_computation_time / successful_count : 0;
            double estimated_sequential_time = avg_sequential_time * centres.size();
            double speedup = estimated_sequential_time > 0 ? 
                estimated_sequential_time / parallel_time_ms : 0;
            
            std::cout << "\n✅ Parallel Dijkstra Complete!" << std::endl;
            std::cout << "   Successful: " << successful_count << " / " << centres.size() << std::endl;
            std::cout << "   Failed: " << failed_count << std::endl;
            std::cout << "   Parallel Execution Time: " << parallel_time_ms << "ms" << std::endl;
            std::cout << "   Estimated Sequential Time: " << (int)estimated_sequential_time << "ms" << std::endl;
            std::cout << "   Speedup: " << std::fixed << std::setprecision(2) << speedup << "x" << std::endl;
            
            json response;
            response["status"] = "success";
            response["workflow_name"] = workflow_name;
            response["workflow_type"] = workflow_type;
            response["centres_processed"] = centres.size();
            response["successful"] = successful_count;
            response["failed"] = failed_count;
            response["results"] = results_json;
            
            response["timing"] = {
                {"parallel_execution_ms", parallel_time_ms},
                {"total_time_ms", total_time_ms},
                {"avg_per_centre_ms", successful_count > 0 ? total_computation_time / successful_count : 0},
                {"estimated_sequential_ms", (int)estimated_sequential_time},
                {"speedup", speedup}
            };
            
            response["performance_metrics"] = {
                {"num_threads_used", centres.size()},
                {"nodes_in_graph", nodes.size()},
                {"edges_in_graph", graph.size()}
            };
            
            res.set_content(response.dump(2), "application/json");
            
        } catch (const std::exception& e) {
            std::cerr << "❌ Error in parallel-dijkstra: " << e.what() << std::endl;
            json error_response;
            error_response["status"] = "error";
            error_response["message"] = e.what();
            res.set_content(error_response.dump(), "application/json");
        } });

    std::cout << "Server starting on http://localhost:8080" << std::endl;
    server.listen("0.0.0.0", 8080);

    return 0;
}
