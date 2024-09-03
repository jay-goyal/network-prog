#include <complex.h>
#include <math.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <threads.h>
#include <unistd.h>

#define ANSI_COLOR_RESET "\x1b[0m"
#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"

int waitcount, maxlevel, level = 0;
int N, A, S;
int points = 0;
int num_signals;
sem_t *write_sem, *signal_sem;
int* pid_idx;
pid_t* pid_arr;
int self_idx;

int get_level(int idx) { return ceil(log(idx - 1) / log(2) - 1); }
int get_waitcount(int level) { return N - (int)(pow(2, level + 1) - 1); }
int get_tgt(int level) {
    if (level == maxlevel)
        return -1 * (get_waitcount(level - 1) - 1);
    else
        return -1 * (pow(2, level) - 1);
}

void alarm_handler(int signo) {
    // PREVENT OTHER SIGNALS FROM BEING RAISED
    sem_wait(signal_sem + self_idx);

    int alive_count = 1;
    pid_t self_pid = getpid();
    for (int i = 0; i < N; i++) {
        pid_t target_pid = pid_arr[i];
        if ((target_pid != self_pid) && (kill(target_pid, 0) == 0))
            alive_count++;
    }
    printf(ANSI_COLOR_MAGENTA "%d PROCESSES ARE ALIVE" ANSI_COLOR_RESET "\n",
           alive_count);
    alarm(3);

    // ALLOW OTHER SIGNALS TO BE RAISED
    sem_post(signal_sem + self_idx);
}

void handler(int signo) {
    if (waitcount > 0) {
        points -= S;
    } else if (waitcount > get_tgt(level)) {
        points -= S / 2;
    } else {
        points += A;
    }
    waitcount--;
    num_signals++;

    if (num_signals == N - 1)
        printf(ANSI_COLOR_GREEN
               "ALL SIGNALS PROCESSED FOR PROCESS AT LEVEL %d WITH IDX "
               "%d. FINAL POINTS: %d" ANSI_COLOR_RESET "\n",
               level, self_idx, points);

    // AFTER HANDLER RETURNS A HANDLER CAN BE EXECUTED
    sem_post(signal_sem + self_idx);
}

int main(int argc, char* argv[]) {
    printf(ANSI_COLOR_MAGENTA
           "CORRECT USAGE OF PROGRAM: <binary> <N> <A> <S>" ANSI_COLOR_RESET
           "\n");
    printf(ANSI_COLOR_MAGENTA "IF NOT USED CORRECTLY QUIT" ANSI_COLOR_RESET
                              "\n");
    if (argc == 4) {
        if (sscanf(argv[1], "%d", &N) != 1) {
            fprintf(stderr,
                    ANSI_COLOR_RED "N is not an integer" ANSI_COLOR_RESET "\n");
            exit(EXIT_FAILURE);
        }
        if (sscanf(argv[2], "%d", &A) != 1) {
            fprintf(stderr,
                    ANSI_COLOR_RED "A is not an integer" ANSI_COLOR_RESET "\n");
            exit(EXIT_FAILURE);
        }
        if (sscanf(argv[3], "%d", &S) != 1) {
            fprintf(stderr,
                    ANSI_COLOR_RED "S is not an integer" ANSI_COLOR_RESET "\n");
            exit(EXIT_FAILURE);
        }
    } else {
        fprintf(stderr, ANSI_COLOR_RED
                "INVALID USAGE OF PROGRAM" ANSI_COLOR_RESET "\n");
        exit(EXIT_FAILURE);
    }
    sleep(2);

    level = -1;
    int to_spawn = N;
    maxlevel = get_level(N);
    printf(ANSI_COLOR_CYAN
           "N = %d, A = %d, S = %d, MAXLEVEL = %d" ANSI_COLOR_RESET "\n",
           N, A, S, maxlevel);

    // INITIALIZE SHARED MEMORY POINTERS
    write_sem = (sem_t*)mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE,
                             MAP_ANONYMOUS | MAP_SHARED, 0, 0);
    signal_sem = (sem_t*)mmap(NULL, sizeof(sem_t) * N, PROT_READ | PROT_WRITE,
                              MAP_ANONYMOUS | MAP_SHARED, 0, 0);
    pid_idx = (int*)mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE,
                         MAP_ANONYMOUS | MAP_SHARED, 0, 0);
    pid_arr = (pid_t*)mmap(NULL, sizeof(pid_t) * N, PROT_READ | PROT_WRITE,
                           MAP_ANONYMOUS | MAP_SHARED, 0, 0);

    sem_init(write_sem, 1, 1);
    for (int i = 0; i < N; i++) sem_init(signal_sem + i, 1, 1);

    *pid_idx = 0;

start_label:
    num_signals = 0;
    level++;
    waitcount = get_waitcount(level);
    to_spawn--;
    int lspawn, rspawn;
    lspawn = rspawn = to_spawn / 2;

    signal(SIGUSR1, handler);
    signal(SIGALRM, alarm_handler);
    if (waitcount < 0) waitcount = 0;

    // IF EVEN NUMBER OF PROC TO BE CREATED AFTER ROOT:
    // LEFT AND RIGHT CREATE EQUAL NUMBER OF PROC
    // ---------------------------------------------------------------
    // IF ODD NUMBER OF PROCESSES TO BE CREATED AFTER ROOT:
    // LEFT CREATES ONE EXTRA PROC THAN RIGHT
    if (to_spawn > 0) {
        pid_t pid1 = fork();
        if (pid1 == 0) {
            if (to_spawn % 2 == 1) {
                lspawn++;
            }
            to_spawn = lspawn;
            goto start_label;
        }
        to_spawn--;
    }

    if (to_spawn > 0) {
        pid_t pid2 = fork();
        if (pid2 == 0) {
            to_spawn = rspawn;
            goto start_label;
        }
        to_spawn--;
    }

    pid_t self_pid = getpid();

    sem_wait(write_sem);

    // EXCLUSION SO THAT ONLY ONE PROCESS MODIFIES THE LIST OF PIDs
    self_idx = *pid_idx;
    pid_arr[*pid_idx] = self_pid;
    *pid_idx += 1;

    printf(
        ANSI_COLOR_YELLOW
        "TO_SPAWN = %d, LEVEL: %d, WAITCOUNT: %d, SELF_IDX: %d" ANSI_COLOR_RESET
        "\n",
        to_spawn, level, waitcount, self_idx);

    sem_post(write_sem);

    // CHECK THAT ALL PIDs ARE UPDATED
    while (*pid_idx < N) sleep(1);
    // WAIT FOR ALL CHILDREN TO SEND SIGNAL
    while (waitcount > 0) sleep(1);

    if (level == 0) {
        alarm(3);
    }

    printf(ANSI_COLOR_BLUE
           "PROCESS AT LEVEL %d WITH PID %d SENDING SIGNALS" ANSI_COLOR_RESET
           "\n",
           level, self_pid);
    for (int i = 0; i < N; i++) {
        pid_t tgt_pid = pid_arr[i];
        if (tgt_pid != self_pid) {
            // WE REQUEST LOCK SO THAT ONLY ONE PROCESS CAN SEND A SIGNAL TO A
            // PARTICULAR PROCESS AT A GIVEN TIME TO PREVENT SIGNAL LOSS
            sem_wait(signal_sem + i);
            kill(tgt_pid, SIGUSR1);
        }
    }

    while (1);

    return 0;
}
