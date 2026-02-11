# Airport simulation — based on multi-processes and IPC

A Linux multi-process simulation of an airport that manages competing resources - runways, gates, ground crews, fuel trucks and de-icers - using **POSIX shared memory** and **POSIX semaphores** with a real-time **ncurses** dashboard

Each airplane is an **independent process** (`plane_main`), while the airport manager is a separate process (`airport_main`) responsible for initialization, UI, spawning planes, passenger terminal simulation and periodic fuel deliveries.

---

## Features

- **True multi-process architecture** - each plane is a separate OS process with its own PID
- **POSIX shared memory** - for a single shared state structure used by all processes
- **POSIX semaphores** - for synchronization and resource control:
  - Binary semaphores for runways and gates
  - Counting semaphores for portable resources
  - A conditional “sleep/wakeup” semaphore for planes waiting for fuel delivery
- **Real-time ncurses UI**:
  - Runways and who occupies them (PID)
  - Gates occupancy by size (S/M/L) and PID
  - Queues - waiting to land / waiting for a gate / waiting to take off
  - Passenger terminal stats per direction (N/E/W)
  - Fuel tank level + alert when planes are waiting for delivery
- **Passenger terminal simulation**:
  - Random passenger groups appear over time (1/3/5/7/9 people)
  - Groups abandon after a patience timeout
  - Planes board matching destination passengers up to capacity
- **Fuel producer–consumer mechanism**:
  - Planes consume fuel while refueling
  - If the tank is insufficient, planes sleep until the airport process delivers fuel
  - Airport delivers fuel periodically and wakes waiting planes
- **Clean shutdown**:
  - Signal handling (`SIGINT`) to destroy semaphores, unmap memory and unlink shared memory
  - Zombie prevention with non-blocking `waitpid(..., WNOHANG)` in the main loop

---

## Architecture overview

### Processes

- `airport_main` 
  - Interactive configuration (runways / gates / resources)
  - Creates and initializes shared memory + semaphores
  - Runs ncurses UI loop
  - Spawns planes via `fork()` + `execv()`
  - Simulates passenger arrivals and patience timeouts
  - Acts as the fuel producer with periodic deliveries
  - Reaps finished plane processes to avoid zombies

- `plane_main`
  - Attaches to existing shared memory
  - Runs a plane state machine - arriving, landing, gate, boarding, servicing, refuel, de-ice, departure
  - Competes for shared resources using semaphores

### Shared state

All processes map a single `AirportSharedState` struct containing:
- configuration values - counts of runways/gates/resources
- semaphore arrays for runways and gate locks
- counting semaphores for crews/trucks/deicers
- fuel stock + lock + delivery-wait semaphore
- planes table - PID, status, size, gate/runway assignment
- passenger terminal queue
- global statistics - planes served, fuel usage, passenger outcomes, direction stats

---

## Plane lifecycle - state machine

A plane process follows these phases:

1. **Arrive**
2. **Land**
3. **Wait for compatible gate**
   - S planes fit any gate
   - M planes fit M or L
   - L planes fit only L
4. **Taxi to gate**
5. **Board passengers** - match destination groups, capacity limited
6. **Service**
7. **Refuel**
   - acquire fuel truck
   - if fuel tank is insufficient - sleep on `fuel_delivery_wait`
8. **De-ice** - optional
9. **Taxi to runway**
10. **Take off**
11. Update statistics and exit

---

## Requirements

- Linux - POSIX IPC
- CMake ≥ 3.10
- C++17 compiler
- `ncurses` development package
- POSIX realtime library (`-lrt`) and pthreads

---
## Extras

This project was developed for the Operating Systems course at **Wroclaw University of Science and Technology** and received a final grade of **5.0**. A detailed project report - in Polish - is also included in the repository as a PDF file.

---
## Author

- **Author -** [Przemysław Dyjak](https://www.linkedin.com/in/przemys%C5%82aw-dyjak-666a11356/?trk=public-profile-join-page)

