// User interface, menus, and efficiency profiling output
#include "TransportSystem.h"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace std;

void Simulation::printOverview() const {
  int busTrips = 0, trainTrips = 0, totalSeats = 0;
  for (const Vehicle &v : vehicles_) {
    const Route &r = network_.routes()[v.routeId];
    if (r.mode == Mode::Bus)
      ++busTrips;
    else
      ++trainTrips;
    totalSeats += r.capacity;
  }
  cout << "\nSimulation Parameters:\n"
       << "  Start time:          " << clockTime(config_.startTime) << '\n'
       << "  Request window:      " << clockTime(config_.startTime) << " - "
       << clockTime(config_.demandEnd) << " (" << passengers_.size()
       << " requests)\n"
       << "  Simulation window:   " << clockTime(config_.startTime) << " - "
       << clockTime(config_.endTime) << '\n'
       << "  Fleet dispatches:    " << busTrips << " bus trips, " << trainTrips
       << " train trips (" << totalSeats << " total seats)\n";
}

void Simulation::printSummary() const {
  if (!hasRun_) {
    cout << "Please run option 4 first to generate efficiency results.\n";
    return;
  }
  int completed = 0, unfinished = 0, unreachable = 0;
  long long totalWaiting = 0, totalJourneyTime = 0, totalRiding = 0,
            totalTransfer = 0;
  int minWait = -1, maxWait = 0;
  int minJourney = -1, maxJourney = 0;
  int transfersCount = 0;

  // loop through all passengers and accumulate stats for completed journeys
  // only
  for (const Passenger &passenger : passengers_) {
    if (passenger.state == PassengerState::Completed) {
      ++completed;
      int journeyTime = passenger.completedAt - passenger.requestedAt;
      totalWaiting += passenger.waitingMinutes;
      totalJourneyTime += journeyTime;
      totalRiding += passenger.ridingMinutes;
      totalTransfer += passenger.transferMinutes;

      if (minWait == -1 || passenger.waitingMinutes < minWait)
        minWait = passenger.waitingMinutes;
      if (passenger.waitingMinutes > maxWait)
        maxWait = passenger.waitingMinutes;

      if (minJourney == -1 || journeyTime < minJourney)
        minJourney = journeyTime;
      if (journeyTime > maxJourney)
        maxJourney = journeyTime;

      if (passenger.transferMinutes > 0)
        ++transfersCount;
    } else if (passenger.state == PassengerState::Unreachable) {
      ++unreachable;
    } else {
      ++unfinished;
    }
  }

  int busTrips = 0, trainTrips = 0, totalSeats = 0;
  for (const Vehicle &v : vehicles_) {
    const Route &r = network_.routes()[v.routeId];
    if (r.mode == Mode::Bus)
      ++busTrips;
    else
      ++trainTrips;
    totalSeats += r.capacity;
  }

  double completionRate =
      passengers_.empty() ? 0.0 : (completed * 100.0 / passengers_.size());
  double avgWait =
      completed > 0 ? static_cast<double>(totalWaiting) / completed : 0.0;
  double avgJourney =
      completed > 0 ? static_cast<double>(totalJourneyTime) / completed : 0.0;
  double avgRiding =
      completed > 0 ? static_cast<double>(totalRiding) / completed : 0.0;
  double avgTransfer =
      completed > 0 ? static_cast<double>(totalTransfer) / completed : 0.0;

  cout << fixed << setprecision(1);
  cout << "\nEFFICIENCY RESULTS: " << clockTime(config_.startTime) << " - "
       << clockTime(config_.demandEnd) << '\n'
       << "\nPassenger Demand & Completion:\n"
       << "  Total requests:       " << passengers_.size() << " passengers\n"
       << "  Completed journeys:   " << completed << " of "
       << passengers_.size() << " (" << completionRate << "%)\n"
       << "  Unfinished at cutoff: " << unfinished << '\n'
       << "  No route available:   " << unreachable << '\n';

  if (completed > 0) {
    double ridePct =
        totalJourneyTime > 0 ? (totalRiding * 100.0 / totalJourneyTime) : 0.0;
    double waitPct =
        totalJourneyTime > 0 ? (totalWaiting * 100.0 / totalJourneyTime) : 0.0;
    double walkPct =
        totalJourneyTime > 0 ? (totalTransfer * 100.0 / totalJourneyTime) : 0.0;

    cout << "\nPassenger Time Profiling:\n"
         << "  Average journey time: " << avgJourney
         << " minutes (min: " << minJourney << ", max: " << maxJourney << ")\n"
         << "  Average waiting time: " << avgWait
         << " minutes (min: " << minWait << ", max: " << maxWait << ")\n"
         << "  Average riding time:  " << avgRiding << " minutes\n"
         << "  Average transfer walk:" << avgTransfer << " minutes\n"
         << "  Time composition:     " << ridePct << "% riding, " << waitPct
         << "% waiting, " << walkPct << "% transfer walk\n"
         << "  Transfers required:   " << transfersCount << " of " << completed
         << " passengers (" << (transfersCount * 100.0 / completed) << "%)\n";
  } else {
    cout
        << "\nPassenger Time Profiling: unavailable (no completed journeys).\n";
  }

  cout << "\nFleet & Capacity Profiling:\n"
       << "  Vehicle dispatches:   " << busTrips << " bus trips, " << trainTrips
       << " train trips\n"
       << "  Total seat capacity:  " << totalSeats << " seats provided\n"
       << "  Peak bus occupancy:   " << peakBusLoad_ << " of 30 seats ("
       << (peakBusLoad_ * 100.0 / 30.0) << "%)\n"
       << "  Peak train occupancy: " << peakTrainLoad_ << " of 100 seats ("
       << (peakTrainLoad_ * 100.0 / 100.0) << "%)\n"
       << "  Peak route queue:     " << peakQueue_
       << " passengers (one route at one stop)\n";

  if (unfinished > 0 || unreachable > 0) {
    cout << "\nNotes:\n"
         << "  Still travelling or waiting at " << clockTime(config_.endTime)
         << ": " << unfinished << '\n'
         << "  No available route: " << unreachable << '\n'
         << "  Averages include completed journeys only.\n";
  }
}

void Simulation::printPassenger(int id) const {
  if (id < 0 || id >= static_cast<int>(passengers_.size())) {
    cout << "Passenger ID not found.\n";
    return;
  }
  const Passenger &passenger = passengers_[id];
  cout << "\nSIMULATED PASSENGER JOURNEY\n"
       << "Passenger " << passenger.id << ": "
       << network_.locations()[passenger.origin].name << " -> "
       << network_.locations()[passenger.destination].name << '\n'
       << "Requested at: " << clockTime(passenger.requestedAt) << '\n';

  if (passenger.journey.reachable && !passenger.journey.legs.empty()) {
    cout << "\nPlanned Route:\n";
    int legNum = 1;
    size_t i = 0;
    while (i < passenger.journey.legs.size()) {
      int currentRouteId = passenger.journey.legs[i].routeId;
      int startStop = passenger.journey.legs[i].from;
      int totalLegTime = 0;
      vector<string> viaNames;
      while (i < passenger.journey.legs.size() &&
             passenger.journey.legs[i].routeId == currentRouteId) {
        totalLegTime += passenger.journey.legs[i].minutes;
        if (i + 1 < passenger.journey.legs.size() &&
            passenger.journey.legs[i + 1].routeId == currentRouteId) {
          viaNames.push_back(
              network_.locations()[passenger.journey.legs[i].to].name);
        }
        ++i;
      }
      int endStop = passenger.journey.legs[i - 1].to;
      const Route &r = network_.routes()[currentRouteId];
      cout << "  Leg " << legNum++ << ": " << modeName(r.mode) << " " << r.name
           << ": " << network_.locations()[startStop].name << " -> "
           << network_.locations()[endStop].name << " (" << totalLegTime
           << " min";
      if (!viaNames.empty()) {
        cout << ", via ";
        for (size_t v = 0; v < viaNames.size(); ++v) {
          cout << viaNames[v] << (v + 1 < viaNames.size() ? ", " : "");
        }
      }
      cout << ")\n";

      if (i < passenger.journey.legs.size()) {
        cout << "  Transfer at " << network_.locations()[endStop].name << " ("
             << TransportNetwork::transferTime << " min walk)\n";
      }
    }
  }

  cout << "\nTimeline:\n";
  for (const string &event : passenger.events) {
    if (event.find("Boarded") != string::npos ||
        event.find("Transfer") != string::npos ||
        event.find("Arrived") != string::npos)
      cout << "  " << event << '\n';
  }

  string status = "Unreachable";
  if (passenger.state == PassengerState::Completed)
    status = "Completed";
  else if (passenger.state == PassengerState::Waiting)
    status = "Waiting";
  else if (passenger.state == PassengerState::Onboard)
    status = "Onboard";

  cout << "\nJourney Summary:\n"
       << "  Status:              " << status << '\n';
  if (passenger.state == PassengerState::Completed) {
    int totalJourneyTime = passenger.completedAt - passenger.requestedAt;
    cout << "  Total journey time:  " << totalJourneyTime << " minutes"
         << " (waiting: " << passenger.waitingMinutes << " min"
         << ", riding: " << passenger.ridingMinutes << " min"
         << ", transfer: " << passenger.transferMinutes << " min)\n";
  } else {
    cout << "  Status: " << status << '\n';
  }
}

namespace {
int readNumber(const string &prompt, int minimum, int maximum) {
  while (true) {
    cout << prompt;
    string line;
    if (!getline(cin, line))
      throw runtime_error("Input closed.");
    istringstream input(line);
    int value;
    char extra;
    if ((input >> value) && !(input >> extra) && value >= minimum &&
        value <= maximum)
      return value;
    cout << "Enter a whole number from " << minimum << " to " << maximum
         << ".\n";
  }
}

int readTime() {
  while (true) {
    cout << "Enter start time (24-hour format, e.g. 07:30): ";
    string line;
    if (!getline(cin, line))
      throw runtime_error("Input closed.");
    istringstream input(line);
    int hour, minute;
    char colon, extra;
    if ((input >> hour >> colon >> minute) && !(input >> extra) &&
        colon == ':' && hour >= 0 && hour < 24 && minute >= 0 && minute < 60)
      return hour * 60 + minute;
    cout << "Please enter a valid time from 00:00 to 23:59.\n";
  }
}

void showLocations(const TransportNetwork &city) {
  cout << "\n=== CITY LOCATIONS ===\n\n";
  for (const Location &location : city.locations())
    cout << location.id << ". " << location.name << "\n";
}

void showRoutes(const TransportNetwork &city, Mode mode) {
  cout << "\n=== " << modeName(mode) << " NETWORK ===\n";
  for (const Route &route : city.routes()) {
    if (route.mode != mode)
      continue;

    int totalTime = 0;
    for (int t : route.travelMinutes)
      totalTime += t;

    cout << "\nRoute " << route.name << " (" << modeName(route.mode)
         << ", every " << route.frequency << " min, capacity " << route.capacity
         << ")\n"
         << "  Stops: ";
    for (size_t i = 0; i < route.stops.size(); ++i) {
      cout << city.locations()[route.stops[i]].name;
      if (i < route.travelMinutes.size()) {
        cout << " -(" << route.travelMinutes[i] << "m)-> ";
      }
    }
    cout << "\n  Total travel time: " << totalTime << " minutes ("
         << route.stops.size() << " stops)\n";
  }
}

} // namespace

int main() {
  try {
    TransportNetwork city = TransportNetwork::createDemoCity();
    Simulation simulation(city);

    cout << "SMART CITY PUBLIC TRANSPORT SIMULATOR\n";
    while (true) {
      cout << "\n1. Show city locations\n2. Show bus routes\n3. Show train "
              "routes\n"
           << "4. Simulate passenger demand\n5. Show efficiency results\n0. "
              "Exit\n";
      int choice = readNumber("Choice: ", 0, 5);
      if (choice == 0)
        break;

      switch (choice) {
      case 1:
        showLocations(city);
        break;
      case 2:
      case 3:
        if (choice == 2)
          showRoutes(city, Mode::Bus);
        else
          showRoutes(city, Mode::Train);
        break;
      case 4: {
        SimulationConfig config;
        config.startTime = readTime();

        cout << "\nCity Locations:\n";
        for (size_t i = 0; i < city.locations().size(); ++i) {
          if (i % 2 == 0 && i + 1 < city.locations().size())
            cout << "  [" << city.locations()[i].id << "] " << left << setw(16)
                 << city.locations()[i].name;
          else
            cout << "  [" << city.locations()[i].id << "] "
                 << city.locations()[i].name << '\n';
        }
        cout << '\n';

        int maxLocation = static_cast<int>(city.locations().size()) - 1;
        int origin =
            readNumber("Enter origin location ID (0-9): ", 0, maxLocation);
        cout << "Origin selected: " << city.locations()[origin].name << '\n';

        int destination;
        while (true) {
          destination = readNumber("Enter destination location ID (0-9): ", 0,
                                   maxLocation);
          if (destination != origin)
            break;
          cout << "Destination must be different from origin.\n";
        }
        cout << "Destination selected: " << city.locations()[destination].name
             << '\n';

        // demand level changes based on time of day - peaks in morning and
        // evening
        int passengerCount = 50;
        string demand = "Quiet period";
        if (config.startTime >= 7 * 60 && config.startTime < 9 * 60) {
          passengerCount = 150;
          demand = "Morning peak";
        } else if (config.startTime >= 16 * 60 && config.startTime < 19 * 60) {
          passengerCount = 120;
          demand = "Evening peak";
        }
        cout << "\n"
             << demand << ": " << passengerCount
             << " passengers over the next hour.\n";
        config.demandEnd = config.startTime + 60;
        config.serviceEnd = config.demandEnd + 45;
        config.endTime = config.demandEnd + 60;
        simulation.reset(config);
        int example =
            simulation.addPassenger(origin, destination, config.startTime);
        simulation.generateDemand(passengerCount - 1);
        simulation.run();
        cout
            << "Simulation complete. Select option 5 for efficiency results.\n";
        simulation.printOverview();
        simulation.printPassenger(example);
        break;
      }
      case 5:
        simulation.printSummary();
        break;
      }
    }
    return 0;
  } catch (const exception &error) {
    cerr << "Error: " << error.what() << '\n';
    return 1;
  }
}
