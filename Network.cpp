// City graph model and shortest path planning
#include "TransportSystem.h"
#include <algorithm>
#include <functional>
#include <iomanip>
#include <limits>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>

using namespace std;

string clockTime(int minutes) {
    ostringstream out;
    out << setfill('0') << setw(2) << (minutes / 60) % 24 << ':'
        << setw(2) << minutes % 60;
    if (minutes >= 24 * 60) out << " (next day)";
    return out.str();
}

int TransportNetwork::addLocation(const string& name) {
    int id = static_cast<int>(locations_.size());
    locations_.push_back({id, name});
    adjacency_.push_back({});
    return id;
}

int TransportNetwork::addRoute(const string& name, Mode mode,
        const vector<int>& stops, const vector<int>& times,
        int frequency, int capacity) {
    if (stops.size() < 2 || times.size() + 1 != stops.size() ||
        frequency <= 0 || capacity <= 0) {
        throw invalid_argument("Invalid route definition.");
    }
    set<int> seen;
    for (int stop : stops) {
        if (stop < 0 || stop >= static_cast<int>(locations_.size()) || !seen.insert(stop).second)
            throw invalid_argument("Route stops must exist and may not repeat.");
    }
    for (int time : times) {
        if (time <= 0) throw invalid_argument("Travel times must be positive.");
    }
    int id = static_cast<int>(routes_.size());
    routes_.push_back({id, name, mode, stops, times, frequency, capacity});
    for (size_t i = 0; i < times.size(); ++i)
        adjacency_[stops[i]].push_back({stops[i + 1], id, times[i]});
    return id;
}

// Dijkstra - finds the shortest path, adds a penalty when switching routes (transfer)
Journey TransportNetwork::planJourney(int origin, int destination) const {
    if (origin < 0 || destination < 0 || origin >= static_cast<int>(locations_.size()) ||
        destination >= static_cast<int>(locations_.size()))
        throw out_of_range("Unknown location ID.");
    Journey result;
    if (origin == destination) { result.reachable = true; return result; }

    // state encodes (stop, current route) so we can detect when a passenger
    // changes route and add the transfer walk penalty at that point
    const int statesPerLocation = static_cast<int>(routes_.size()) + 1;
    const int noRoute = statesPerLocation - 1;
    const int stateCount = static_cast<int>(locations_.size()) * statesPerLocation;
    const int infinity = numeric_limits<int>::max() / 4;
    vector<int> distance(stateCount, infinity), parent(stateCount, -1);
    vector<JourneyLeg> incoming(stateCount);
    typedef pair<int, int> Entry;
    priority_queue<Entry, vector<Entry>, greater<Entry>> frontier;
    int start = origin * statesPerLocation + noRoute;
    distance[start] = 0;
    frontier.push({0, start});
    int finish = -1;
    while (!frontier.empty()) {
        Entry current = frontier.top();
        frontier.pop();
        int state = current.second;
        if (current.first != distance[state]) continue;
        int stop = state / statesPerLocation;
        int previousRoute = state % statesPerLocation;
        if (stop == destination) { finish = state; break; }
        for (const Edge& edge : adjacency_[stop]) {
            int transferMinutes = 0;
            if (previousRoute != noRoute && previousRoute != edge.routeId)
                transferMinutes = transferTime;
            int nextState = edge.to * statesPerLocation + edge.routeId;
            int newDistance = distance[state] + edge.minutes + transferMinutes;
            if (newDistance < distance[nextState]) {
                distance[nextState] = newDistance;
                parent[nextState] = state;
                incoming[nextState] = {stop, edge.to, edge.routeId, edge.minutes};
                frontier.push({newDistance, nextState});
            }
        }
    }
    if (finish == -1) return result;
    result.reachable = true;
    result.estimatedMinutes = distance[finish];
    for (int state = finish; state != start; state = parent[state])
        result.legs.push_back(incoming[state]);
    reverse(result.legs.begin(), result.legs.end());
    return result;
}

void TransportNetwork::addBothDirections(const string& name, Mode mode,
        vector<int> stops, vector<int> times, int frequency, int capacity) {
    addRoute(name + " outbound", mode, stops, times, frequency, capacity);
    reverse(stops.begin(), stops.end());
    reverse(times.begin(), times.end());
    addRoute(name + " inbound", mode, stops, times, frequency, capacity);
}

// builds the 10-node directed weighted graph - nodes are locations, edges are routes
TransportNetwork TransportNetwork::createDemoCity() {
    TransportNetwork city;
    city.addLocation("City Hall");
    city.addLocation("School");
    city.addLocation("Library");      // interchange: bus B1 + train T1
    city.addLocation("Hospital");
    city.addLocation("Market");       // interchange: bus B1 + bus B2
    city.addLocation("University");
    city.addLocation("Park");
    city.addLocation("Airport");
    city.addLocation("Film Hall");    // interchange: bus B2 + train T1
    city.addLocation("Restaurant");

    city.addBothDirections("B1", Mode::Bus,   {0, 1, 2, 3, 4, 5}, {5, 4, 6, 5, 6}, 12, 30);
    city.addBothDirections("B2", Mode::Bus,   {6, 9, 8, 4},        {5, 8, 6},       10, 30);
    city.addBothDirections("T1", Mode::Train, {6, 2, 8, 7},        {6, 7, 9},       15, 100);
    return city;
}
