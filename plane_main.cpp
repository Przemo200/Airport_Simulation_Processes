#include <iostream>
#include <string>
#include <cstdlib>
#include <ctime>
#include <sys/wait.h>
#include <random>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <unistd.h>
#include "shared_state.h"

AirportSharedState* state = nullptr;
PlaneSlot* my_slot = nullptr;

void attach_memory() {
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);

    void* ptr = mmap(NULL, sizeof(AirportSharedState), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    state = static_cast<AirportSharedState*>(ptr);
    close(shm_fd);
}

void set_status(PStatus s) {
    sem_wait(&state->ui_lock);
    my_slot->status = s;
    sem_post(&state->ui_lock);
}

const float Slowmo = 1.2;

void work_ms(int min_ms, int max_ms) {
    int delay = min_ms + (rand() % (max_ms - min_ms + 1));
    usleep(delay * 1000 * Slowmo);
}

int acquire_runway() {
    int id = -1;
    while (id == -1) {
        for (int i = 0; i < state->config_runways; ++i) {
            if (sem_trywait(&state->runways[i]) == 0) {
                id = i;

                sem_wait(&state->ui_lock);
                state->runway_pid[i] = getpid();
                sem_post(&state->ui_lock);
                break;
            }
        }
        if (id == -1) usleep(100000);
    }
    return id;
}

void release_runway(int id) {
    sem_wait(&state->ui_lock);
    state->runway_pid[id] = 0;
    sem_post(&state->ui_lock);
    usleep(600000);
    sem_post(&state->runways[id]);
}

int acquire_compatible_gate(PSize my_size) {
    int id = -1;
    while (id == -1) {
        for (int i = 0; i < state->config_total_gates; ++i) {
            bool compatible = false;
            PSize g_size = state->gates[i].size;

            if (my_size == PSize::S) compatible = true;
            else if (my_size == PSize::M) {
                if (g_size == PSize::M || g_size == PSize::L) compatible = true;
            }
            else if (my_size == PSize::L) {
                if (g_size == PSize::L) compatible = true;
            }

            if (compatible) {
                if (sem_trywait(&state->gates[i].lock) == 0) {
                    id = i;
                    sem_wait(&state->ui_lock);
                    my_slot->gate_id = id;
                    sem_post(&state->ui_lock);
                    break;
                }
            }
        }
        if (id == -1) usleep(200000);
    }
    return id;
}

void release_gate(int id) {
    sem_wait(&state->ui_lock);
    my_slot->gate_id = -1;
    state->gates[id].plane_pid = 0;
    sem_post(&state->ui_lock);
    sem_post(&state->gates[id].lock);
}

void visual_occupy_gate(int id) {
    sem_wait(&state->ui_lock);
    state->gates[id].plane_pid = getpid();
    sem_post(&state->ui_lock);
}

void perform_boarding() {
    set_status(PStatus::BOARDING);
    work_ms(1000, 2000);

    sem_wait(&state->ui_lock);

    int taken = 0;
    for(int i=0; i<MAX_WAITING_PASSENGERS; ++i) {
        if (state->terminal_queue[i].is_active && state->terminal_queue[i].dest == my_slot->dest) {
            int grp_size = state->terminal_queue[i].group_size;

            if (taken + grp_size <= my_slot->capacity) {
                state->terminal_queue[i].is_active = false;
                taken += grp_size;
            }
        }
    }
    my_slot->passengers_on_board = taken;

    sem_post(&state->ui_lock);

    work_ms(taken * 100, taken * 200 + 500);
}

void process_refueling() {
        int needed = 7000 + (rand() % 5000);

        while (sem_trywait(&state->fuel_trucks) != 0) {
            set_status(PStatus::WAIT_FUEL_TRUCK);
            work_ms(500, 1500);
        }

        set_status(PStatus::REFUELING);

        sem_wait(&state->fuel_stock_lock);

        while (state->fuel_stock < needed) {
            set_status(PStatus::WAIT_FUEL_DELIVERY);
            state->planes_waiting_for_delivery++;

            sem_post(&state->fuel_stock_lock);

            sem_wait(&state->fuel_delivery_wait);

            sem_wait(&state->fuel_stock_lock);
        }

        state->fuel_stock -= needed;
        state->stats.total_fuel += needed;

        sem_post(&state->fuel_stock_lock);

        set_status(PStatus::REFUELING);
        work_ms(1000, 6000);

        sem_post(&state->fuel_trucks);
}

void process_deicing() {
    if (my_slot->needs_deicing) {
        while (sem_trywait(&state->deicers) != 0) {
             set_status(PStatus::WAIT_DEICER);
             work_ms(500, 1500);
        }

        work_ms(1000, 1000);

        set_status(PStatus::DEICING);

        sem_wait(&state->ui_lock);
        state->stats.total_deicings++;
        sem_post(&state->ui_lock);

        work_ms(1500, 3500);

        sem_post(&state->deicers);
    }
}

int main(int argc, char* argv[]) {

    int slot_id = std::atoi(argv[1]);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist_100(0, 99);
    std::uniform_int_distribution<> dist_3(0, 2);

    srand(rd());

    attach_memory();
    my_slot = &state->planes[slot_id];

    int r = dist_100(gen);

    sem_wait(&state->ui_lock);

    if (r < 40) {
        my_slot->size = PSize::S;
        my_slot->capacity = 18;
    }
    else if (r < 70) {
        my_slot->size = PSize::M;
        my_slot->capacity = 36;
    }
    else {
        my_slot->size = PSize::L;
        my_slot->capacity = 48;
    }

    int d = dist_3(gen);
    if (d == 0) my_slot->dest = Direction::NORTH;
    else if (d == 1) my_slot->dest = Direction::EAST;
    else my_slot->dest = Direction::WEST;

    sem_post(&state->ui_lock);

    my_slot->needs_deicing = (dist_100(gen) < 40);
    my_slot->work_units = 100;

    work_ms(500, 1000);

    set_status(PStatus::WAIT_RW_L);
    int rw = acquire_runway();
    set_status(PStatus::LANDING);
    work_ms(1500, 4000);
    release_runway(rw);

    set_status(PStatus::WAIT_GATE);
    int gt = acquire_compatible_gate(my_slot->size);
    set_status(PStatus::TAXI_GATE);
    work_ms(1000, 3500);
    visual_occupy_gate(gt);
    set_status(PStatus::AT_GATE);

    work_ms(1000, 2000);

    perform_boarding();

    while (sem_trywait(&state->ground_crews) != 0) {
        set_status(PStatus::WAIT_CREW);
        work_ms(500, 1500);
    }

    set_status(PStatus::SERVICING);
    work_ms(3000, 7000);

    sem_post(&state->ground_crews);

    process_refueling();

    process_deicing();

    set_status(PStatus::READY_DEP);
    work_ms(500, 1000);

    release_gate(gt);

    set_status(PStatus::TAXI_RW);
    work_ms(500, 1000);

    set_status(PStatus::WAIT_RW_T);
    rw = acquire_runway();
    set_status(PStatus::TAKING_OFF);
    work_ms(1500, 3000);
    release_runway(rw);

    sem_wait(&state->ui_lock);

    state->stats.total_planes++;

    if (my_slot->size == PSize::S) state->stats.planes_S_count++;
    else if (my_slot->size == PSize::M) state->stats.planes_M_count++;
    else state->stats.planes_L_count++;

    if (my_slot->dest == Direction::NORTH) state->stats.flights_N_count++;
    else if (my_slot->dest == Direction::EAST) state->stats.flights_E_count++;
    else state->stats.flights_W_count++;

    int pax = my_slot->passengers_on_board;
    state->stats.pax_total_flown += pax;

    if (my_slot->dest == Direction::NORTH) {
        state->stats.pax_dir_N += pax;
    }
    else if (my_slot->dest == Direction::EAST) {
        state->stats.pax_dir_E += pax;
    }
    else {
        state->stats.pax_dir_W += pax;
    }

    if (my_slot->size == PSize::S) state->stats.pax_on_S += pax;
    else if (my_slot->size == PSize::M) state->stats.pax_on_M += pax;
    else state->stats.pax_on_L += pax;

    my_slot->pid = 0;
    sem_post(&state->ui_lock);

    return 0;
}