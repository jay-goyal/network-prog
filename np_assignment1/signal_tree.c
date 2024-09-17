#include <bits/types/sigset_t.h>
#include <complex.h>
#include <errno.h>
#include <math.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <threads.h>
#include <time.h>
#include <unistd.h>

#define FG_RST "\x1b[0m"
#define FG_RED "\x1b[31m"
#define FG_GRN "\x1b[32m"
#define FG_YLW "\x1b[33m"
#define FG_BLUE "\x1b[34m"
#define FG_MAG "\x1b[35m"
#define FG_CYAN "\x1b[36m"

int maxlevel, level = 0;
int N, A, S;
int points;
sem_t *write_sem, *signal_sem;
int* pid_idx;
pid_t* pid_arr;
int self_idx;
pid_t pid1 = 0, pid2 = 0;
bool to_kill = false;
sigset_t block_alarm;

int get_level(int idx) { return ceil(log(idx - 1) / log(2) - 1); }

void alarm_handler(int signo) {
    /*if (sem_trywait(signal_sem + self_idx) != 0) {*/
    /*    printf(FG_MAG*/
    /*           "ALARM TRIGGERED WHILE SIGNAL EXECUTING. NOT COUNTING PROCESSES" FG_RST*/
    /*           "\n");*/
    /*    alarm(3);*/
    /*    return;*/
    /*}*/
    int alive_count = 1;
    pid_t self_pid = getpid();
    for (int i = 0; i < N; i++) {
        pid_t target_pid = pid_arr[i];
        if ((target_pid != self_pid) && (kill(target_pid, 0) == 0)) alive_count++;
    }
    printf(FG_MAG "%d PROCESSES ARE ALIVE" FG_RST "\n", alive_count);
    alarm(3);
    /*sem_post(signal_sem + self_idx);*/
}

void handler(int signo, siginfo_t* si, void* ucontext) {
    sigprocmask(SIG_BLOCK, &block_alarm, NULL);

    if (to_kill) {
        sem_post(signal_sem + self_idx);
        return;
    }

    // GET LEVEL OF PROCESS SENDING SIGNAL
    union sigval src_val = si->si_value;
    int src_level = src_val.sival_int;

    if (src_level < level) {
        points += A;
    } else if (src_level > level) {
        points -= S;
    } else {
        points -= S / 2;
    }

    if (points <= 0) {
        to_kill = true;
        printf(FG_GRN
               "POINTS ZERO OR NEGATIVE FOR PROCESS AT LEVEL %d WITH IDX %d. FINAL "
               "POINTS: %d" FG_RST "\n",
               level, self_idx, points);
    }

    // AFTER HANDLER RETURNS A HANDLER CAN BE EXECUTED
    sem_post(signal_sem + self_idx);
    sigprocmask(SIG_UNBLOCK, &block_alarm, NULL);
}

int main(int argc, char* argv[]) {
    // PARSE COMMAND LINE OPTIONS
    printf(FG_MAG "CORRECT USAGE OF PROGRAM: <binary> <N> <A> <S>" FG_RST "\n");
    printf(FG_MAG "IF NOT USED CORRECTLY QUIT" FG_RST "\n");
    if (argc == 4) {
        if (sscanf(argv[1], "%d", &N) != 1) {
            fprintf(stderr, FG_RED "N is not an integer" FG_RST "\n");
            exit(EXIT_FAILURE);
        }
        if (sscanf(argv[2], "%d", &A) != 1) {
            fprintf(stderr, FG_RED "A is not an integer" FG_RST "\n");
            exit(EXIT_FAILURE);
        }
        if (sscanf(argv[3], "%d", &S) != 1) {
            fprintf(stderr, FG_RED "S is not an integer" FG_RST "\n");
            exit(EXIT_FAILURE);
        }
    } else {
        fprintf(stderr, FG_RED "INVALID USAGE OF PROGRAM" FG_RST "\n");
        exit(EXIT_FAILURE);
    }
    sleep(2);

    level = -1;
    int to_spawn = N;
    maxlevel = get_level(N);
    printf(FG_CYAN "N = %d, A = %d, S = %d, MAXLEVEL = %d" FG_RST "\n", N, A, S,
           maxlevel);

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
    points = N;
    level++;
    to_spawn--;
    int lspawn, rspawn;
    lspawn = rspawn = to_spawn / 2;
    int child_count = 0;

    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = handler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGUSR1, &sa, NULL);

    // SETUP ALARM
    if (level == 0) signal(SIGALRM, alarm_handler);
    sigemptyset(&block_alarm);
    sigaddset(&block_alarm, SIGALRM);

    // IF EVEN NUMBER OF PROC TO BE CREATED AFTER ROOT:
    // LEFT AND RIGHT CREATE EQUAL NUMBER OF PROC
    // ---------------------------------------------------------------
    // IF ODD NUMBER OF PROCESSES TO BE CREATED AFTER ROOT:
    // LEFT CREATES ONE EXTRA PROC THAN RIGHT
    if (to_spawn > 0) {
        pid1 = fork();
        if (pid1 == 0) {
            if (to_spawn % 2 == 1) {
                lspawn++;
            }
            to_spawn = lspawn;
            goto start_label;
        }
        to_spawn--;
        child_count++;
    }

    if (to_spawn > 0) {
        pid2 = fork();
        if (pid2 == 0) {
            to_spawn = rspawn;
            goto start_label;
        }
        to_spawn--;
        child_count++;
    }

    pid_t self_pid = getpid();

    sem_wait(write_sem);

    // EXCLUSION SO THAT ONLY ONE PROCESS MODIFIES THE LIST OF PIDs
    self_idx = *pid_idx;
    pid_arr[*pid_idx] = self_pid;
    *pid_idx += 1;

    sem_post(write_sem);

    printf(FG_YLW "TO_SPAWN = %d, LEVEL: %d, SELF_IDX: %d, CHILD_COUNT: %d" FG_RST "\n",
           to_spawn, level, self_idx, child_count);

    // CHECK THAT ALL PIDs ARE UPDATED
    while (*pid_idx < N) sleep(3);

    if (level == 0) alarm(3);

    printf(FG_BLUE "PROCESS AT LEVEL %d WITH PID %d SENDING SIGNALS" FG_RST "\n", level,
           self_pid);

    union sigval sv;
    sv.sival_int = level;

    do {
        for (int i = 0; i < N; i++) {
            pid_t tgt_pid = pid_arr[i];
            if ((tgt_pid != self_pid) && (kill(tgt_pid, 0) == 0)) {
                // WE REQUEST LOCK SO THAT ONLY ONE PROCESS CAN SEND A SIGNAL TO A
                // PARTICULAR PROCESS AT A GIVEN TIME TO PREVENT SIGNAL LOSS
                while (sem_trywait(signal_sem + i) == 0);
                sigqueue(tgt_pid, SIGUSR1, sv);
            }
        }
        if (child_count > 0) {
            if (pid1) {
                if (waitpid(pid1, NULL, WNOHANG) > 0) {
                    pid1 = 0;
                    child_count--;
                }
            }
            if (pid2) {
                if (waitpid(pid2, NULL, WNOHANG) > 0) {
                    pid2 = 0;
                    child_count--;
                }
            }
        }
    } while (to_kill == false);

    while (child_count > 0) {
        if (pid1) {
            if (waitpid(pid1, NULL, WNOHANG) > 0) {
                pid1 = 0;
                child_count--;
            }
        }
        if (pid2) {
            if (waitpid(pid2, NULL, WNOHANG) > 0) {
                pid2 = 0;
                child_count--;
            }
        }
        sleep(1);
    }

    if (level == 0) {
        sem_destroy(write_sem);
        for (int i = 0; i < N; i++) sem_destroy(signal_sem + i);
    }

    printf(FG_MAG "PROCESS AT LEVEL %d WITH IDX %d EXITING" FG_RST "\n", level, self_idx);

    exit(EXIT_SUCCESS);
}
