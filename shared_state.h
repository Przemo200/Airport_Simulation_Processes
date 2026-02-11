#pragma once

#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstddef>
#include <csignal>
#include <ctime>

const int MAX_RUNWAYS = 4;
const int MAX_GATES = 20;

const int MAX_GROUND_CREWS = 20;
const int MAX_FUEL_TRUCKS = 20;
const int MAX_DEICERS = 20;

const long long INITIAL_FUEL_STOCK = 25000;
const long long FUEL_REFILL_AMOUNT = 40000;

const int MAX_CONCURRENT_PLANES = 30;
const char* SHM_NAME = "/airport_ultimate_shm";

const int MAX_WAITING_PASSENGERS = 100;
const int PASSENGER_PATIENCE_SEC = 25;

enum class PSize { S, M, L };

enum class Direction { NORTH, EAST, WEST };

enum class PStatus {
    NONE,
    ARRIVING,
    WAIT_RW_L,
    LANDING,
    TAXI_GATE,
    WAIT_GATE,
    AT_GATE,
    BOARDING,
    WAIT_CREW,
    SERVICING,
    WAIT_FUEL_TRUCK,
    WAIT_FUEL_DELIVERY,
    REFUELING,
    WAIT_DEICER,
    DEICING,
    READY_DEP,
    TAXI_RW,
    WAIT_RW_T,
    TAKING_OFF
};

struct Gate {
    PSize size;
    sem_t lock;
    pid_t plane_pid;
};

struct PlaneSlot {
    pid_t pid;
    PStatus status;
    PSize size;

    Direction dest;
    int capacity;
    int passengers_on_board;

    int gate_id;
    int runway_id;
    bool needs_deicing;
    int work_units;
};

struct Passenger {
    bool is_active;
    Direction dest;
    time_t arrival_time;
    int group_size;
};

struct Statistics {
    int total_planes;
    long long total_fuel;
    int total_deicings;

    int pax_spawned;
    int pax_resigned;
    int pax_total_flown;

    int planes_S_count;
    int planes_M_count;
    int planes_L_count;

    int flights_N_count;
    int flights_E_count;
    int flights_W_count;

    int pax_on_S;
    int pax_on_M;
    int pax_on_L;

    int pax_dir_N;
    int pax_dir_E;
    int pax_dir_W;
};

struct AirportSharedState {
    sem_t ui_lock;

    int config_runways;
    int config_gates_S;
    int config_gates_M;
    int config_gates_L;
    int config_total_gates;

    int config_crews;
    int config_trucks;
    int config_deicers;

    sem_t runways[MAX_RUNWAYS];
    pid_t runway_pid[MAX_RUNWAYS];
    Gate gates[MAX_GATES];

    sem_t ground_crews;
    sem_t fuel_trucks;
    sem_t deicers;

    sem_t fuel_stock_lock;
    long long fuel_stock;
    sem_t fuel_delivery_wait;
    int planes_waiting_for_delivery;

    PlaneSlot planes[MAX_CONCURRENT_PLANES];
    int planes_spawned_count;

    Passenger terminal_queue[MAX_WAITING_PASSENGERS];

    Statistics stats;
};