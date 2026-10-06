// Passenger and vehicle simulation engine
#include "TransportSystem.h"
#include <algorithm>
#include <random>
#include <stdexcept>

using namespace std;

Simulation::Simulation(const TransportNetwork& network, SimulationConfig config)
    : network_(network) {
    reset(config);
}

void Simulation::reset(SimulationConfig config) {
    if (config.startTime < 0 || config.startTime >= config.demandEnd ||
        config.demandEnd > config.serviceEnd || config.serviceEnd > config.endTime)
        throw invalid_argument("Invalid simulation time range.");
    config_ = config;
    passengers_.clear();
    vehicles_.clear();
    queues_.assign(network_.routes().size(), vector<deque<int>>(network_.locations().size()));
    hasRun_ = false;
    peakBusLoad_ = 0;
    peakTrainLoad_ = 0;
    peakQueue_ = 0;
}

void Simulation::log(Passenger& passenger, int time, const string& message) {
    passenger.events.push_back(clockTime(time) + "  " + message);
}

int Simulation::addPassenger(int origin, int destination, int requestedAt) {
    if (hasRun_) throw logic_error("Create a new simulation before adding passengers.");
    if (requestedAt < config_.startTime || requestedAt >= config_.demandEnd)
        throw invalid_argument("Request time is outside demand hours.");
    Passenger passenger;
    passenger.id = static_cast<int>(passengers_.size());
    passenger.origin = origin;
    passenger.destination = destination;
    passenger.requestedAt = requestedAt;
    passenger.readyAt = requestedAt;
    passenger.journey = network_.planJourney(origin, destination);
    passengers_.push_back(passenger);
    return passenger.id;
}

static int chooseLocation(const vector<int>& locations, mt19937& random) {
    return locations[uniform_int_distribution<int>(0, static_cast<int>(locations.size()) - 1)(random)];
}

void Simulation::generateDemand(int passengerCount) {
    if (hasRun_ || passengerCount < 0)
        throw logic_error("Generate demand before running the simulation.");
    vector<int> commuteStops, all;
    for (const Location& location : network_.locations()) {
        all.push_back(location.id);
        if (location.name == "City Hall" || location.name == "School" ||
            location.name == "Hospital" || location.name == "University")
            commuteStops.push_back(location.id);
    }
    if (all.size() < 2) throw logic_error("Demand needs at least two locations.");
    if (commuteStops.empty()) commuteStops = all;
    mt19937 random(config_.seed);

    vector<int> origins = all, destinations = all;
    if (config_.startTime >= 7 * 60 && config_.startTime < 9 * 60) destinations = commuteStops;
    else if (config_.startTime >= 16 * 60 && config_.startTime < 19 * 60) origins = commuteStops;

    int duration = config_.demandEnd - config_.startTime;
    for (int i = 0; i < passengerCount; ++i) {
        // spread passenger requests evenly across the hour
        int time = config_.startTime + (i + 1) * duration / (passengerCount + 1);
        int from = chooseLocation(origins, random);
        int to = chooseLocation(destinations, random);
        while (to == from) to = chooseLocation(all, random);
        addPassenger(from, to, time);
    }
}

// puts passenger into the waiting queue at their boarding stop
void Simulation::enqueue(Passenger& passenger, int time) {
    const JourneyLeg& leg = passenger.journey.legs.at(passenger.nextLeg);
    passenger.state = PassengerState::Waiting;
    queues_[leg.routeId][leg.from].push_back(passenger.id);
    peakQueue_ = max(peakQueue_, static_cast<int>(queues_[leg.routeId][leg.from].size()));
    log(passenger, time, "Waiting at " + network_.locations()[leg.from].name +
        " for " + network_.routes()[leg.routeId].name);
}

// called when a vehicle arrives at a stop - drops off passengers due here
// those transferring get re-queued with a walk time penalty
void Simulation::processArrival(Vehicle& vehicle, int time) {
    const Route& route = network_.routes()[vehicle.routeId];
    if (vehicle.stopIndex == 0) return;
    vector<int> remainingPassengers;
    for (int id : vehicle.passengers) {
        Passenger& passenger = passengers_[id];
        const JourneyLeg& leg = passenger.journey.legs.at(passenger.nextLeg);
        if (leg.routeId != route.id || leg.to != route.stops[vehicle.stopIndex])
            throw logic_error("Passenger and vehicle paths disagree.");
        passenger.ridingMinutes += time - passenger.boardedAt;
        ++passenger.nextLeg;
        if (passenger.nextLeg == passenger.journey.legs.size()) {
            passenger.state = PassengerState::Completed;
            passenger.completedAt = time;
            log(passenger, time, "Arrived at " + network_.locations()[passenger.destination].name);
        } else if (passenger.journey.legs[passenger.nextLeg].routeId == route.id) {
            passenger.boardedAt = time;
            remainingPassengers.push_back(id);
        } else {
            passenger.transferMinutes += TransportNetwork::transferTime;
            passenger.readyAt = time + TransportNetwork::transferTime;
            log(passenger, time, "Transfer at " + network_.locations()[leg.to].name + "; ready to board at " + clockTime(passenger.readyAt));
            enqueue(passenger, time);
        }
    }
    vehicle.passengers.swap(remainingPassengers);
}

// boards waiting passengers onto the vehicle (skips anyone still mid-transfer walk)
void Simulation::board(Vehicle& vehicle, int time) {
    const Route& route = network_.routes()[vehicle.routeId];
    if (vehicle.stopIndex + 1 == route.stops.size()) {
        if (!vehicle.passengers.empty()) throw logic_error("Passengers left at terminal.");
        vehicle.finished = true;
        return;
    }
    deque<int>& queue = queues_[route.id][route.stops[vehicle.stopIndex]];
    for (auto it = queue.begin(); it != queue.end();) {
        Passenger& passenger = passengers_[*it];
        if (passenger.readyAt > time || static_cast<int>(vehicle.passengers.size()) >= route.capacity) {
            ++it;
            continue;
        }
        passenger.waitingMinutes += time - passenger.readyAt;
        passenger.boardedAt = time;
        passenger.state = PassengerState::Onboard;
        vehicle.passengers.push_back(passenger.id);
        log(passenger, time, "Boarded " + modeName(route.mode) + " " + route.name + " at " + network_.locations()[route.stops[vehicle.stopIndex]].name);
        it = queue.erase(it);
    }
    if (route.mode == Mode::Bus) peakBusLoad_ = max(peakBusLoad_, static_cast<int>(vehicle.passengers.size()));
    if (route.mode == Mode::Train) peakTrainLoad_ = max(peakTrainLoad_, static_cast<int>(vehicle.passengers.size()));
    vehicle.arrivesAt = time + route.travelMinutes[vehicle.stopIndex];
    ++vehicle.stopIndex;
}

void Simulation::startPassenger(Passenger& passenger, int time) {
    if (!passenger.journey.reachable) {
        passenger.state = PassengerState::Unreachable;
        log(passenger, time, "No connected journey exists.");
    } else if (passenger.journey.legs.empty()) {
        passenger.state = PassengerState::Completed;
        passenger.completedAt = time;
        log(passenger, time, "Already at destination.");
    } else {
        enqueue(passenger, time);
    }
}

// sends a new vehicle for every route that's due a departure at this minute
void Simulation::dispatchVehicles(int time) {
    if (time >= config_.serviceEnd || time >= config_.endTime) return;
    for (const Route& route : network_.routes())
        if ((time - config_.startTime) % route.frequency == 0)
            vehicles_.push_back({static_cast<int>(vehicles_.size()), route.id, 0, time, false, {}});
}

void Simulation::run() {
    if (hasRun_) throw logic_error("This simulation has already run.");
    hasRun_ = true;
    vector<vector<int>> requests(config_.endTime + 1);
    for (const Passenger& passenger : passengers_)
        requests[passenger.requestedAt].push_back(passenger.id);

    for (int time = config_.startTime; time <= config_.endTime; ++time) {
        for (int id : requests[time]) startPassenger(passengers_[id], time);
        dispatchVehicles(time);
        // alighting happens before boarding so transfers work correctly
        for (Vehicle& vehicle : vehicles_)
            if (!vehicle.finished && vehicle.arrivesAt == time) processArrival(vehicle, time);
        for (Vehicle& vehicle : vehicles_)
            if (!vehicle.finished && vehicle.arrivesAt == time) board(vehicle, time);
    }
    verifyInvariants();
}

void Simulation::verifyInvariants() const {
    vector<int> present(passengers_.size(), 0);
    for (const auto& routeQueues : queues_)
        for (const auto& queue : routeQueues)
            for (int id : queue) {
                ++present.at(id);
                if (passengers_[id].state != PassengerState::Waiting)
                    throw logic_error("Queue contains a non-waiting passenger.");
            }
    for (const Vehicle& vehicle : vehicles_) {
        if (vehicle.passengers.size() > static_cast<size_t>(network_.routes()[vehicle.routeId].capacity))
            throw logic_error("Vehicle capacity exceeded.");
        for (int id : vehicle.passengers) {
            ++present.at(id);
            if (passengers_[id].state != PassengerState::Onboard)
                throw logic_error("Vehicle contains a non-onboard passenger.");
        }
    }
    for (const Passenger& passenger : passengers_) {
        bool active = passenger.state == PassengerState::Waiting || passenger.state == PassengerState::Onboard;
        if (present[passenger.id] != (active ? 1 : 0))
            throw logic_error("Passenger is missing or duplicated.");
        if (passenger.state == PassengerState::Completed &&
            passenger.completedAt - passenger.requestedAt != passenger.waitingMinutes +
                passenger.ridingMinutes + passenger.transferMinutes)
            throw logic_error("Journey time accounting does not balance.");
    }
}
