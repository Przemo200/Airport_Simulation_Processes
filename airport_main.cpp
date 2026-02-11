#include <iostream>
#include <string>
#include <vector>
#include <ncurses.h>
#include <sys/wait.h>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <ctime>
#include <iomanip>
#include "shared_state.h"

AirportSharedState* state = nullptr;
int shm_fd = -1;

struct UserConfig {
    int runways;
    int gates_s, gates_m, gates_l;
    int crews;
    int trucks;
    int deicers;
};

UserConfig get_user_config() {
    UserConfig cfg;
    std::cout << "\nKONFIGURACJA LOTNISKA \n";

    std::cout << "Podaj liczbe pasow startowych (1 - " << MAX_RUNWAYS << "): ";
    std::cin >> cfg.runways;
    if (cfg.runways < 1) {
        std::cout << "Niepoprawna ilosc pasow (min 1). Ustawiam 1\n";
        cfg.runways = 1;
    } else if (cfg.runways > MAX_RUNWAYS) {
        std::cout << "Przekroczono limit pasow (" << MAX_RUNWAYS << "). Ustawiam " << MAX_RUNWAYS << "\n";
        cfg.runways = MAX_RUNWAYS;
    }

    int gates_left = MAX_GATES;

    std::cout << "\nKONFIGURACJA BRAMEK - lacznie maks " << MAX_GATES << " \n";

    std::cout << "Zostalo do dyspozycji: " << gates_left << " bramek.\n";
    std::cout << "Podaj liczbe malych bramek (S): ";
    std::cin >> cfg.gates_s;

    if (cfg.gates_s < 0) {
        std::cout << "Liczba bramek nie moze byc ujemna. Ustawiam 0\n";
        cfg.gates_s = 0;
    }
    if (cfg.gates_s > gates_left) {
        std::cout << "Przekroczono limit dostepnych bramek - ustawiam na maksa: " << gates_left << "\n";
        cfg.gates_s = gates_left;
    }
    gates_left -= cfg.gates_s;

    if (gates_left > 0) {
        std::cout << "Zostalo do dyspozycji: " << gates_left << " bramek.\n";
        std::cout << "Podaj liczbe srednich bramek (M): ";
        std::cin >> cfg.gates_m;

        if (cfg.gates_m < 0) {
            std::cout << "Liczba bramek nie moze byc ujemna - ustawiam 0\n";
            cfg.gates_m = 0;
        }
        if (cfg.gates_m > gates_left) {
            std::cout << "Przekroczono limit dostepnych bramek - ustawiam na maksa: " << gates_left << "\n";
            cfg.gates_m = gates_left;
        }
        gates_left -= cfg.gates_m;
    } else {
        std::cout << "Brak miejsca na bramki M - ustawiam 0.\n";
        cfg.gates_m = 0;
    }

    if (gates_left > 0) {
        std::cout << "Zostalo do dyspozycji: " << gates_left << " bramek.\n";
        std::cout << "Podaj liczbe duzych bramek (L): ";
        std::cin >> cfg.gates_l;

        if (cfg.gates_l < 0) {
            std::cout << "Liczba bramek nie moze byc ujemna - ustawiam: 0\n";
            cfg.gates_l = 0;
        }
        if (cfg.gates_l > gates_left) {
            std::cout << "Przekroczono limit dostepnych bramek - ustawiam na maksa: " << gates_left << "\n";
            cfg.gates_l = gates_left;
        }
        gates_left -= cfg.gates_l;
    } else {
        std::cout << "Brak miejsca na bramki L - ustawiam 0.\n";
        cfg.gates_l = 0;
    }

    int total = cfg.gates_s + cfg.gates_m + cfg.gates_l;
    if (total == 0) {
        std::cout << " Nie mozna utworzyc lotniska bez zadnej bramki! Ustawiam na 1 mala bramke (S).\n";
        cfg.gates_s = 1;
    }

    std::cout << "\nZASOBY OBSLUGI \n";

    std::cout << "Podaj liczbe ekip technicznych (1 - " << MAX_GROUND_CREWS << "): ";
    std::cin >> cfg.crews;
    if (cfg.crews < 1) {
        std::cout << "Za malo ekip! Minimum to 1. Ustawiam na 1\n";
        cfg.crews = 1;
    } else if (cfg.crews > MAX_GROUND_CREWS) {
        std::cout << "Za duzo ekip! Maksimum to " << MAX_GROUND_CREWS << ". Ustawiam na " << MAX_GROUND_CREWS << "\n";
        cfg.crews = MAX_GROUND_CREWS;
    }

    std::cout << "Podaj liczbe cystern z paliwem (1 - " << MAX_FUEL_TRUCKS << "): ";
    std::cin >> cfg.trucks;
    if (cfg.trucks < 1) {
        std::cout << "Za malo cystern! Minimum to 1. Ustawiam na 1\n";
        cfg.trucks = 1;
    } else if (cfg.trucks > MAX_FUEL_TRUCKS) {
        std::cout << "Za duzo cystern! Maksimum to " << MAX_FUEL_TRUCKS << ". Ustawiam na " << MAX_FUEL_TRUCKS << "\n";
        cfg.trucks = MAX_FUEL_TRUCKS;
    }

    std::cout << "Podaj liczbe odladzarek (1 - " << MAX_DEICERS << "): ";
    std::cin >> cfg.deicers;
    if (cfg.deicers < 1) {
        std::cout << "Za malo odladzarek! Minimum to 1. Ustawiam na 1\n";
        cfg.deicers = 1;
    } else if (cfg.deicers > MAX_DEICERS) {
        std::cout << "Za duzo odladzarek! Maksimum to " << MAX_DEICERS << ". Ustawiam na" << MAX_DEICERS << "\n";
        cfg.deicers = MAX_DEICERS;
    }

    return cfg;
}

void cleanup_ncurses() {
    if (stdscr != NULL) endwin();
}

void cleanup_ipc() {
    if (state != nullptr) {
        sem_destroy(&state->ui_lock);
        for(int i=0; i<state->config_runways; ++i) sem_destroy(&state->runways[i]);
        for(int i=0; i<state->config_total_gates; ++i) sem_destroy(&state->gates[i].lock);
        sem_destroy(&state->ground_crews);
        sem_destroy(&state->fuel_trucks);
        sem_destroy(&state->deicers);
        sem_destroy(&state->fuel_stock_lock);
        sem_destroy(&state->fuel_delivery_wait);
        munmap(state, sizeof(AirportSharedState));
    }
    if (shm_fd != -1) close(shm_fd);
    shm_unlink(SHM_NAME);
}

void handle_signal(int sig) {
    (void)sig;
    cleanup_ncurses();
    cleanup_ipc();
    exit(0);
}

void initialize_state(UserConfig cfg) {
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(AirportSharedState));

    void* ptr = mmap(NULL, sizeof(AirportSharedState), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    state = static_cast<AirportSharedState*>(ptr);

    memset(state, 0, sizeof(AirportSharedState));

    state->config_runways = cfg.runways;
    state->config_gates_S = cfg.gates_s;
    state->config_gates_M = cfg.gates_m;
    state->config_gates_L = cfg.gates_l;
    state->config_total_gates = cfg.gates_s + cfg.gates_m + cfg.gates_l;
    state->config_crews = cfg.crews;
    state->config_trucks = cfg.trucks;
    state->config_deicers = cfg.deicers;

    sem_init(&state->ui_lock, 1, 1);

    for(int i=0; i<state->config_runways; ++i) sem_init(&state->runways[i], 1, 1);

    int gid = 0;
    for(int i=0; i<state->config_gates_S; ++i) { state->gates[gid].size = PSize::S; sem_init(&state->gates[gid++].lock, 1, 1); }
    for(int i=0; i<state->config_gates_M; ++i) { state->gates[gid].size = PSize::M; sem_init(&state->gates[gid++].lock, 1, 1); }
    for(int i=0; i<state->config_gates_L; ++i) { state->gates[gid].size = PSize::L; sem_init(&state->gates[gid++].lock, 1, 1); }

    sem_init(&state->ground_crews, 1, state->config_crews);
    sem_init(&state->fuel_trucks, 1, state->config_trucks);
    sem_init(&state->deicers, 1, state->config_deicers);

    sem_init(&state->fuel_stock_lock, 1, 1);
    state->fuel_stock = INITIAL_FUEL_STOCK;
    sem_init(&state->fuel_delivery_wait, 1, 0);
}

void setup_ncurses() {
    initscr();
    cbreak();
    noecho();
    nodelay(stdscr, TRUE);
    curs_set(0);
    start_color();
    use_default_colors();
    init_pair(1, COLOR_WHITE, COLOR_BLUE);
    init_pair(2, COLOR_GREEN, -1);
    init_pair(3, COLOR_RED, -1);
    init_pair(4, COLOR_YELLOW, -1);
    init_pair(5, COLOR_MAGENTA, -1);
    init_pair(6, COLOR_WHITE, COLOR_RED);
}

char size_char(PSize s) { return (s == PSize::S) ? 'S' : (s == PSize::M ? 'M' : 'L'); }

char dest_char(Direction d) {
    if (d == Direction::NORTH) return 'N';
    if (d == Direction::EAST) return 'E';
    return 'W';
}

void manage_passengers() {
    sem_wait(&state->ui_lock);

    if (rand() % 100 < 15) {
        for (int i = 0; i < MAX_WAITING_PASSENGERS; ++i) {
            if (!state->terminal_queue[i].is_active) {
                state->terminal_queue[i].is_active = true;
                state->terminal_queue[i].arrival_time = time(NULL);

                int r_size = rand() % 100;
                if (r_size < 30) state->terminal_queue[i].group_size = 1;
                else if (r_size < 60) state->terminal_queue[i].group_size = 3;
                else if (r_size < 80) state->terminal_queue[i].group_size = 5;
                else if (r_size < 90) state->terminal_queue[i].group_size = 7;
                else state->terminal_queue[i].group_size = 9;

                int r = rand() % 3;
                state->terminal_queue[i].dest = (r==0) ? Direction::NORTH : (r==1) ? Direction::EAST : Direction::WEST;

                state->stats.pax_spawned += state->terminal_queue[i].group_size;
                break;
            }
        }
    }

    time_t now = time(NULL);
    for (int i = 0; i < MAX_WAITING_PASSENGERS; ++i) {
        if (state->terminal_queue[i].is_active) {
            double waiting = difftime(now, state->terminal_queue[i].arrival_time);
            if (waiting > PASSENGER_PATIENCE_SEC) {
                state->terminal_queue[i].is_active = false;
                state->stats.pax_resigned += state->terminal_queue[i].group_size;
            }
        }
    }

    sem_post(&state->ui_lock);
}

void draw_ui() {
    clear();
    int y = 0;

    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(y++, 0, "  LOTNISKO - PROCESY ");
    attroff(COLOR_PAIR(1) | A_BOLD);
    mvprintw(y++, 0, " [p] Nowy lot | [q] Raport i wyjscie | Dostawa paliwa jest co 20s");
    y++;

    sem_wait(&state->ui_lock);

    mvprintw(y, 0, "ZBIORNIK PALIWA:");
    attron(A_BOLD);
    mvprintw(y, 18, "%lld litrow", state->fuel_stock);
    attroff(A_BOLD);

    if (state->planes_waiting_for_delivery > 0) {
        attron(COLOR_PAIR(6) | A_BLINK);
        mvprintw(y, 40, "BRAK PALIWA - CZEKA %d SAMOLOTOW", state->planes_waiting_for_delivery);
        attroff(COLOR_PAIR(6) | A_BLINK);
    }
    y+=2;

    mvprintw(y++, 0, "STATYSTYKI PASAZEROW - OSOBY: Odlecieli: %d | Zrezygnowali: %d",
             state->stats.pax_total_flown,
             state->stats.pax_resigned);
    y++;

    int n_cnt = 0, e_cnt = 0, w_cnt = 0;
    int n_groups = 0, e_groups = 0, w_groups = 0;

    for(int i=0; i<MAX_WAITING_PASSENGERS; ++i) {
        if (state->terminal_queue[i].is_active) {
            if (state->terminal_queue[i].dest == Direction::NORTH) {
                n_cnt += state->terminal_queue[i].group_size;
                n_groups++;
            }
            if (state->terminal_queue[i].dest == Direction::EAST) {
                e_cnt += state->terminal_queue[i].group_size;
                e_groups++;
            }
            if (state->terminal_queue[i].dest == Direction::WEST) {
                w_cnt += state->terminal_queue[i].group_size;
                w_groups++;
            }
        }
    }
    mvprintw(y++, 0, "TERMINAL - oczekujacy (GRUPY -> OSOBY):");
    mvprintw(y++, 2, "NORTH (N): %2d (%3d os.) | EAST (E): %2d (%3d os.) | WEST (W): %2d (%3d os.)",
            n_groups, n_cnt, e_groups, e_cnt, w_groups, w_cnt);
    y++;

    int crews, trucks, deicers;
    sem_getvalue(&state->ground_crews, &crews);
    sem_getvalue(&state->fuel_trucks, &trucks);
    sem_getvalue(&state->deicers, &deicers);

    mvprintw(y++, 0, "ZASOBY PRZENOSZONE:");
    mvprintw(y++, 2, "Ekipy: %d/%d  |  Cysterny: %d/%d  |  Odladzarki: %d/%d",
             crews, state->config_crews, trucks, state->config_trucks, deicers, state->config_deicers);
    y++;

    mvprintw(y++, 0, "PASY STARTOWE (%d):", state->config_runways);
    int runways_y = y;

    for(int i=0; i<state->config_runways; ++i) {
        bool busy = state->runway_pid[i] != 0;
        int x_pos = 2 + (i * 22);

        attron(COLOR_PAIR(busy ? 3 : 2));
        mvprintw(runways_y, x_pos, "[PAS %d: %s]", i, busy ? "ZAJETY" : "WOLNY ");
        attroff(COLOR_PAIR(busy ? 3 : 2));

        if (busy) {
            mvprintw(runways_y + 1, x_pos + 1, "PID: %d", state->runway_pid[i]);
        } else {
            mvprintw(runways_y + 1, x_pos + 1, "           ");
        }
    }
    y += 3;

    mvprintw(y++, 0, "CZEKAJA NA PAS DO LADOWANIA - przestrzen powietrzna:");
    int land_q_count = 0;
    int q_x = 2;

    for(int i=0; i<MAX_CONCURRENT_PLANES; ++i) {
        if (state->planes[i].pid != 0 && state->planes[i].status == PStatus::WAIT_RW_L) {
            attron(COLOR_PAIR(4) | A_BOLD);
            if (q_x > 85) { q_x = 2; y++; }
            mvprintw(y, q_x, "[%d %c]", state->planes[i].pid, size_char(state->planes[i].size));
            q_x += 12;
            attroff(COLOR_PAIR(4) | A_BOLD);
            land_q_count++;
        }
    }
    if (land_q_count == 0) {
        attron(COLOR_PAIR(2)); mvprintw(y, 2, "(Pusto)"); attroff(COLOR_PAIR(2));
    }
    y += 2;

    mvprintw(y++, 0, "BRAMKI ( S:%d / M:%d / L:%d ):", state->config_gates_S, state->config_gates_M, state->config_gates_L);
    for(int i=0; i<state->config_total_gates; ++i) {
        bool busy = state->gates[i].plane_pid != 0;
        attron(COLOR_PAIR(busy ? 3 : 2));
        mvprintw(y++, 2, "BRAMKA %d [%c]: %s %s",
                 i, size_char(state->gates[i].size),
                 busy ? "ZAJETA przez" : "WOLNA",
                 busy ? std::to_string(state->gates[i].plane_pid).c_str() : "");
        attroff(COLOR_PAIR(busy ? 3 : 2));
    }
    y++;

    mvprintw(y++, 0, "POCZEKALNIA DO BRAMEK - brak odpowiedniego GATE dla danego samolotu:");
    int gate_q_count = 0;
    q_x = 2;

    for(int i=0; i<MAX_CONCURRENT_PLANES; ++i) {
        if (state->planes[i].pid != 0 && state->planes[i].status == PStatus::WAIT_GATE) {
            attron(COLOR_PAIR(4) | A_BOLD);
            if (q_x > 85) { q_x = 2; y++; }
            mvprintw(y, q_x, "[%d %c]", state->planes[i].pid, size_char(state->planes[i].size));
            q_x += 12;
            attroff(COLOR_PAIR(4) | A_BOLD);
            gate_q_count++;
        }
    }
    if (gate_q_count == 0) {
        attron(COLOR_PAIR(2)); mvprintw(y, 2, "(Pusto)"); attroff(COLOR_PAIR(2));
    }
    y += 2;

    mvprintw(y++, 0, "POCZEKALNIA DO STARTU - przed pasami:");
    int takeoff_q_count = 0;
    q_x = 2;

    for(int i=0; i<MAX_CONCURRENT_PLANES; ++i) {
        if (state->planes[i].pid != 0 && state->planes[i].status == PStatus::WAIT_RW_T) {
            attron(COLOR_PAIR(4) | A_BOLD);
            if (q_x > 85) { q_x = 2; y++; }
            mvprintw(y, q_x, "[%d %c]", state->planes[i].pid, size_char(state->planes[i].size));
            q_x += 12;
            attroff(COLOR_PAIR(4) | A_BOLD);
            takeoff_q_count++;
        }
    }
    if (takeoff_q_count == 0) {
        attron(COLOR_PAIR(2)); mvprintw(y, 2, "(Pusto)"); attroff(COLOR_PAIR(2));
    }
    y += 2;

    int row = 4;
    int col = 100;
    attron(A_BOLD); mvprintw(row++, col, "LISTA LOTOW - PROCESY:"); attroff(A_BOLD);

    mvprintw(row++, col, "PID   ROZMIAR   KIER             STATUS                                         PAX/CAP");

    for(int i=0; i<MAX_CONCURRENT_PLANES; ++i) {
        if (state->planes[i].pid != 0) {
            int color = (state->planes[i].status == PStatus::WAIT_FUEL_DELIVERY) ? 6 : 5;

            attron(COLOR_PAIR(color));
            const char* status_txt = "N/A";

            switch(state->planes[i].status) {
                case PStatus::ARRIVING:         status_txt = "Leci...";       break;
                case PStatus::WAIT_RW_L:        status_txt = "Czeka na pas (LAND)";  break;
                case PStatus::LANDING:          status_txt = "LADUJE";        break;

                case PStatus::TAXI_GATE:        status_txt = "Koluje na gate";   break;
                case PStatus::WAIT_GATE:        status_txt = "Czeka na wjazd na gate";    break;
                case PStatus::AT_GATE:          status_txt = "Jest na gate";       break;

                case PStatus::BOARDING:         status_txt = "BOARDING";      break;

                case PStatus::WAIT_CREW:        status_txt = "Czeka na ekipe";   break;
                case PStatus::SERVICING:        status_txt = "Obsluga przez ekipe";       break;

                case PStatus::WAIT_FUEL_TRUCK:  status_txt = "Czeka na cysterne";     break;
                case PStatus::WAIT_FUEL_DELIVERY: status_txt = "BRAK PALIWA! Czeka z zarezerwowana cysterna"; break;
                case PStatus::REFUELING:        status_txt = "Tankuje";       break;

                case PStatus::WAIT_DEICER:      status_txt = "Czeka na odladzanie";     break;
                case PStatus::DEICING:          status_txt = "Odladzanie";    break;

                case PStatus::READY_DEP:        status_txt = "Gotowy";        break;
                case PStatus::TAXI_RW:          status_txt = "Koluje na pas";    break;
                case PStatus::WAIT_RW_T:        status_txt = "Czeka na pas (START)";  break;
                case PStatus::TAKING_OFF:       status_txt = "STARTUJE";      break;

                default:                        status_txt = "...";           break;
            }

            mvprintw(row++, col, "%-8d %-2c     ->%-8c       %-46s %2d/%-2d",
                     state->planes[i].pid,
                     size_char(state->planes[i].size),
                     dest_char(state->planes[i].dest),
                     status_txt,
                     state->planes[i].passengers_on_board,
                     state->planes[i].capacity);
        }
    }

    sem_post(&state->ui_lock);
    refresh();
}

void spawn_plane() {
    sem_wait(&state->ui_lock);
    int slot_id = -1;
    for(int i=0; i<MAX_CONCURRENT_PLANES; ++i) {
        if (state->planes[i].pid == 0) { slot_id = i; break; }
    }
    if (slot_id == -1) { sem_post(&state->ui_lock); return; }

    pid_t pid = fork();

    if (pid == 0) {
        std::string s_id = std::to_string(slot_id);
        std::string s_rnd = std::to_string(state->planes_spawned_count);
        char* args[] = { (char*)"./plane_main", (char*)s_id.c_str(), (char*)s_rnd.c_str(), NULL };
        execv(args[0], args);
        exit(1);
    }
    else {
        state->planes[slot_id].pid = pid;
        state->planes[slot_id].status = PStatus::ARRIVING;
        state->planes[slot_id].passengers_on_board = 0;
        state->planes_spawned_count++;
        sem_post(&state->ui_lock);
    }
}

void run_fuel_producer() {
    sem_wait(&state->fuel_stock_lock);

    if (state->fuel_stock <= 80000) {

        state->fuel_stock += FUEL_REFILL_AMOUNT;

        int waiters = state->planes_waiting_for_delivery;
        state->planes_waiting_for_delivery = 0;

        sem_post(&state->fuel_stock_lock);

        for(int i=0; i<waiters; ++i) {
            sem_post(&state->fuel_delivery_wait);
        }
    }
    else {
        sem_post(&state->fuel_stock_lock);
    }
}

void print_final_report() {
    std::cout << "\n\n=======================================================================\n";
    std::cout << "                   RAPORT KONCOWY SYMULACJI LOTNISKA     \n";
    std::cout << "=======================================================================\n";
    std::cout << " 1. RUCH LOTNICZY:\n";
    std::cout << "    - Liczba lacznie obsluzonych samolotow, ktore odlecialy: " << state->stats.total_planes << "\n";
    std::cout << "       - Typ S: " << state->stats.planes_S_count << "\n";
    std::cout << "       - Typ M: " << state->stats.planes_M_count << "\n";
    std::cout << "       - Typ L: " << state->stats.planes_L_count << "\n";
    std::cout << "    - Zuzyte paliwo:  " << state->stats.total_fuel << " litrow\n";
    std::cout << "    - Odladzanie:     " << state->stats.total_deicings << " operacji\n\n";

    std::cout << " 2. STATYSTYKI PASAZEROW (OSOBY):\n";
    std::cout << "    - Pojawilo sie w terminalu:                                 " << state->stats.pax_spawned << "\n";
    std::cout << "    - Zrezygnowalo przez timeout:                               " << state->stats.pax_resigned << "\n";

    int total_flown = state->stats.pax_total_flown;
    int left_in_terminal = state->stats.pax_spawned - state->stats.pax_resigned - total_flown;

    std::cout << "    - Lacznie odlecialo:                                        " << total_flown << "\n";
    std::cout << "    - W terminalu i samolotach, ktore nie odlecialy zostalo:    " << left_in_terminal << "\n\n";

    double avg_global = (state->stats.total_planes > 0) ?
                        (double)state->stats.pax_total_flown / state->stats.total_planes : 0.0;

    double avg_S = (state->stats.planes_S_count > 0) ? (double)state->stats.pax_on_S / state->stats.planes_S_count : 0.0;
    double avg_M = (state->stats.planes_M_count > 0) ? (double)state->stats.pax_on_M / state->stats.planes_M_count : 0.0;
    double avg_L = (state->stats.planes_L_count > 0) ? (double)state->stats.pax_on_L / state->stats.planes_L_count : 0.0;

    double avg_N = (state->stats.flights_N_count > 0) ? (double)state->stats.pax_dir_N / state->stats.flights_N_count : 0.0;
    double avg_E = (state->stats.flights_E_count > 0) ? (double)state->stats.pax_dir_E / state->stats.flights_E_count : 0.0;
    double avg_W = (state->stats.flights_W_count > 0) ? (double)state->stats.pax_dir_W / state->stats.flights_W_count : 0.0;

    std::cout << std::fixed << std::setprecision(2);
    std::cout << " 3. SREDNIE OBICIAZENIE:\n";
    std::cout << "    - Srednia liczba pasazerow na lot - ogolnie: " << avg_global << "\n";
    std::cout << "    - Srednio wg typu samolotu:\n";
    std::cout << "       > Typ S (max 18):  " << avg_S << " pax/lot\n";
    std::cout << "       > Typ M (max 36): " << avg_M << " pax/lot\n";
    std::cout << "       > Typ L (max 48): " << avg_L << " pax/lot\n";

    std::cout << "    - Srednio wg kierunku lotu:\n";
    std::cout << "       > NORTH: " << avg_N << " pax/lot (lotow: " << state->stats.flights_N_count << ")\n";
    std::cout << "       > EAST:  " << avg_E << " pax/lot (lotow: " << state->stats.flights_E_count << ")\n";
    std::cout << "       > WEST:  " << avg_W << " pax/lot (lotow: " << state->stats.flights_W_count << ")\n";

    std::cout << "=======================================================================\n\n";
}

int main() {
    srand(time(NULL));
    signal(SIGINT, handle_signal);

    UserConfig cfg = get_user_config();

    cleanup_ipc();
    initialize_state(cfg);
    setup_ncurses();
    atexit(cleanup_ncurses);

    auto last_refill = std::chrono::steady_clock::now();

    bool running = true;
    while(running) {
        int ch = getch();
        if (ch == 'q') running = false;
        if (ch == 'p') spawn_plane();

        manage_passengers();

        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_refill).count() > 20) {
            run_fuel_producer();
            last_refill = now;
        }

        draw_ui();

        while(waitpid(-1, NULL, WNOHANG) > 0);

        usleep(50000);
    }

    sem_wait(&state->ui_lock);
    for(int i=0; i<MAX_CONCURRENT_PLANES; ++i) {
        if(state->planes[i].pid != 0) kill(state->planes[i].pid, SIGKILL);
    }
    sem_post(&state->ui_lock);

    cleanup_ncurses();

    print_final_report();

    cleanup_ipc();
    return 0;
}