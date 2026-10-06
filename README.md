# Smart City Public Transport Simulator

C++ implementation of IS2202 Problem C. The program uses three implementation files and one shared header.

## Run

Double-click **Run Transport.bat** to compile and run. A C++11 compiler (g++) is required and is available on this computer. The built application is **TransportSystem.exe**.

Manual build:

```powershell
g++ -std=c++11 -Wall -Wextra -Wpedantic -O2 Network.cpp Simulation.cpp Main.cpp -o TransportSystem.exe
./TransportSystem.exe
```

## Menu

```text
1. Show city locations
2. Show bus routes
3. Show train routes
4. Simulate passenger demand
5. Show efficiency results
0. Exit
```

Options 1-3 show the city and its services. Option 4 asks for a start time, an origin ID, and a different destination ID. It simulates that passenger alongside other generated requests, then shows the chosen passenger's planned route, actual timeline, and journey summary. The journey may use a bus, a train, or both, depending on the locations. Option 5 displays efficiency results from the latest run. Running another simulation replaces the previous results.

## Three main files

| File | Responsibility |
| --- | --- |
| Network.cpp | City graph, route definitions, and Dijkstra journey planning |
| Simulation.cpp | Demand, schedules, vehicle movement, boarding, transfers, and consistency checks |
| Main.cpp | Menu, validated time/location input, chosen journey, and efficiency output |

TransportSystem.h provides shared records and class declarations. These are suggested areas for three members to understand and present; record actual contributions in the submission.

## Assignment coverage

- Graph model: ten locations connected by a directed weighted adjacency list.
- Bus network: two routes, each operating in both directions with departure intervals and capacities.
- Train network: one line in both directions, connected to bus services at shared stops.
- Variable demand: user-entered time selects morning, evening, or quiet-period demand, with requests arriving throughout the next hour.
- Combined journeys: selecting Airport (7) to City Hall (0) demonstrates a train journey via Film Hall to Library, then a bus via School to City Hall.
- Profiling: completion rate, average/minimum/maximum journey and waiting times, riding and transfer times, vehicle dispatches, peak vehicle occupancy, and peak route queue. Unfinished and unreachable passengers are also reported.

## How to demonstrate

1. Use options 1-3 to explain the locations, routes, travel times, and capacities.
2. Select option 4, enter **07:30**, choose origin **7 (Airport)** and destination **0 (City Hall)**. Observe the morning demand and the passenger's actual boarding, transfer, and arrival times.
3. Select option 5 to see the efficiency results.
4. Repeat options 4 and 5 for **12:00** and **17:00**, entering the origin and destination each time, to compare different demand levels.

## Demand and timing assumptions

Enter any time from 00:00 to 23:59 using HH:MM. The chosen start time determines the total for that one-hour request window:

| Start time | Demand | Passengers |
| --- | --- | --- |
| 07:00-08:59 | Morning peak | 150 |
| 16:00-18:59 | Evening peak | 120 |
| Other times | Quiet period | 50 |

These counts are assumptions and include the user-selected passenger, who requests travel at the start time. The remaining requests are spread across the hour. Their origins/destinations are randomly selected with fixed seed 42; morning journeys emphasize City Hall, School, Hospital, and University as destinations, and evening journeys emphasize them as origins. Demand remains fixed for the hour even when it crosses a peak boundary.

Each run is independent. Departures begin at the entered time. No new vehicles depart at or after 45 minutes after the one-hour request window ends. Vehicles already dispatched continue moving until the simulation cutoff, 60 minutes after requests stop. The displayed simulation window therefore spans two hours. For a 07:30 start, requests stop at 08:30, new departures stop at 09:15, and the simulation ends at 09:30. Any unfinished passengers at that cutoff are reported. Times after midnight are marked next day. Random distributions can differ between compilers.

## Data structures and process

The graph uses `vector<vector<Edge>>`. Each edge stores a destination, route ID, and positive travel time. Dijkstra uses a min-priority queue and distance/predecessor vectors. Its state is (location, previous route), allowing two minutes for changing services without charging passengers who stay aboard. It minimizes riding plus transfer walking time, excluding timetable waiting and crowding.

Passenger and vehicle records are kept in vectors. Each route/stop pair has a deque of passenger IDs. Ready passengers board in queue order until capacity is reached. Someone still walking through an interchange does not block other ready passengers.

The simulation advances in whole minutes: introduce requests, dispatch vehicles, process all arrivals, then board passengers. Arrivals precede boarding for every route. Each dispatch is a vehicle trip; fleet reuse, traffic, depots, and driver shifts are outside the model. Boarding/alighting adds no extra time. All travel times are fictional assumptions.

## Understanding efficiency

- Completed/total passengers shows how much demand was served.
- Average waiting time measures time spent ready to board but waiting for a vehicle.
- Average journey time measures request-to-arrival time, including waiting, riding, and transfer walking.
- Time averages use completed journeys only. Unfinished and unreachable passengers are reported when present; averages are unavailable if none complete.
- Vehicle dispatches count individual trips, not a fixed fleet of reusable vehicles. Total seat capacity sums the seats on dispatched trips; it is not an occupancy percentage.
- Peak bus/train occupancy is the largest onboard count observed on any vehicle of that mode.
- Peak route queue is the largest queue for one directional route at one stop, including passengers still completing a transfer walk. It does not combine all routes at a stop.

Compare different start times. Passenger destinations and timetable alignment also affect averages, so higher demand does not guarantee a higher average wait. These figures describe the assumed system, not proof of optimality.

## Validation and submission

Input checks reject invalid menu choices, times, and location IDs, and require different origin and destination IDs. Runtime checks detect missing/duplicated passengers, excess capacity, and completed journey times that do not balance.

The assignment also requires a source ZIP and a report of at most ten pages covering design, implementation, running instructions, and screenshots, plus the presentation/viva.

## Transport network diagram

All connections operate in both directions. The diagram uses the exact connections implemented in Network.cpp.

![City transport network](City%20Network.png)

| Service | Stops in outbound order | Segment times (minutes) |
| --- | --- | --- |
| B1 | 0 - 1 - 2 - 3 - 4 - 5 | 5, 4, 6, 5, 6 |
| B2 | 6 - 9 - 8 - 4 | 5, 8, 6 |
| T1 | 6 - 2 - 8 - 7 | 6, 7, 9 |

The combined graph has branches and alternative paths. For example, Library (2) to Market (4) can use bus B1 through Hospital (3), or train T1 to Film Hall (8) followed by bus B2. Each individual service still follows a fixed stop sequence; the existing routing and simulation algorithms are unchanged.

| Service | Departures from each end | Capacity per vehicle |
| --- | --- | --- |
| B1 | Every 12 minutes | 30 |
| B2 | Every 10 minutes | 30 |
| T1 | Every 15 minutes | 100 |
