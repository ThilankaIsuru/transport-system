#ifndef TRANSPORT_SYSTEM_H
#define TRANSPORT_SYSTEM_H

#include <deque>
#include <string>
#include <vector>

enum class Mode { Bus, Train };
enum class PassengerState { Waiting, Onboard, Completed, Unreachable };

struct Location {
  int id;
  std::string name;
};

struct Route {
  int id;
  std::string name;
  Mode mode;
  std::vector<int> stops;
  std::vector<int> travelMinutes;
  int frequency;
  int capacity;
};

struct Edge {
  int to;
  int routeId;
  int minutes;
};

struct JourneyLeg {
  int from;
  int to;
  int routeId;
  int minutes;
};

struct Journey {
  bool reachable = false;
  int estimatedMinutes = 0;
  std::vector<JourneyLeg> legs;
};

struct Passenger {
  int id = 0;
  int origin = 0;
  int destination = 0;
  int requestedAt = 0;
  int readyAt = 0;
  int boardedAt = 0;
  int completedAt = -1;
  int waitingMinutes = 0;
  int ridingMinutes = 0;
  int transferMinutes = 0;
  std::size_t nextLeg = 0;
  PassengerState state = PassengerState::Waiting;
  Journey journey;
  std::vector<std::string> events;
};

struct Vehicle {
  int id;
  int routeId;
  std::size_t stopIndex;
  int arrivesAt;
  bool finished;
  std::vector<int> passengers;
};

inline std::string modeName(Mode mode) {
  return mode == Mode::Bus ? "Bus" : "Train";
}

std::string clockTime(int minutes);

class TransportNetwork {
public:
  static const int transferTime = 2;
  int addLocation(const std::string &name);
  int addRoute(const std::string &name, Mode mode,
               const std::vector<int> &stops, const std::vector<int> &times,
               int frequency, int capacity);
  Journey planJourney(int origin, int destination) const;
  const std::vector<Location> &locations() const { return locations_; }
  const std::vector<Route> &routes() const { return routes_; }
  static TransportNetwork createDemoCity();

private:
  void addBothDirections(const std::string &name, Mode mode,
                         std::vector<int> stops, std::vector<int> times,
                         int frequency, int capacity);
  std::vector<Location> locations_;
  std::vector<Route> routes_;
  std::vector<std::vector<Edge>> adjacency_;
};

struct SimulationConfig {
  int startTime = 6 * 60;
  int demandEnd = 22 * 60;       // No new requests at or after this time.
  int serviceEnd = 23 * 60 + 30; // No new vehicle departures after this time.
  int endTime = 24 * 60;
  unsigned int seed = 42;
};

class Simulation {
public:
  explicit Simulation(const TransportNetwork &network,
                      SimulationConfig config = SimulationConfig());
  void reset(SimulationConfig config = SimulationConfig());
  void generateDemand(int passengerCount);
  int addPassenger(int origin, int destination, int requestedAt);
  void run();
  void printOverview() const;
  void printSummary() const;
  void printPassenger(int id) const;
  void verifyInvariants() const;

private:
  const TransportNetwork &network_;
  SimulationConfig config_;
  std::vector<Passenger> passengers_;
  std::vector<Vehicle> vehicles_;
  // queues_[route][stop] contains passenger IDs in order of queue entry.
  std::vector<std::vector<std::deque<int>>> queues_;
  bool hasRun_ = false;
  int peakBusLoad_ = 0;
  int peakTrainLoad_ = 0;
  int peakQueue_ = 0;
  void startPassenger(Passenger &passenger, int time);
  void dispatchVehicles(int time);
  void enqueue(Passenger &passenger, int time);
  void processArrival(Vehicle &vehicle, int time);
  void board(Vehicle &vehicle, int time);
  void log(Passenger &passenger, int time, const std::string &message);
};

#endif
