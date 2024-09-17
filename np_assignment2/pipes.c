#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define NUM_PROC 6
#define BUF_LEN 256

// ANSI COLORS
#define FG_RST "\x1b[0m"
#define FG_GRN "\x1b[32m"
#define FG_YLW "\x1b[33m"

// PROCEDURES
int parent_proc(int idx);
int lower_to_upper(int idx);
int remove_first(int idx);
int remove_last(int idx);

// GLOBAL VARS
pid_t procs[NUM_PROC];
int pipes[NUM_PROC][2];
int (*proc_methods[NUM_PROC])(int) = {parent_proc, lower_to_upper, remove_first,
                                      remove_last, remove_first,   remove_last};

int parent_proc(int idx) {
    char buf[BUF_LEN + 1];
    bzero(buf, BUF_LEN + 1);
    int num_read = read(STDIN_FILENO, buf, BUF_LEN);
    if (buf[num_read] == '\n' || buf[num_read] == '\0') {
        buf[num_read] = '\0';
        num_read--;
    }

    write(pipes[(idx + 1) % NUM_PROC][1], buf, num_read);
    close(pipes[(idx + 1) % NUM_PROC][1]);

    bzero(buf, BUF_LEN + 1);
    num_read = read(pipes[idx][0], buf, BUF_LEN);

    close(pipes[idx][0]);
    fprintf(stdout, FG_GRN "FINAL OUPUT: %s" FG_RST "\n", buf);
    return 0;
}

int lower_to_upper(int idx) {
    char buf[BUF_LEN + 1];
    bzero(buf, BUF_LEN + 1);
    int num_read = read(pipes[idx][0], buf, BUF_LEN);
    close(pipes[idx][0]);

    for (int i = 0; i < num_read; i++)
        if (buf[i] >= 'a' && buf[i] <= 'z') buf[i] -= 32;

    write(pipes[(idx + 1) % NUM_PROC][1], buf, num_read);
    close(pipes[(idx + 1) % NUM_PROC][1]);
    fprintf(stdout, FG_YLW "PROCESS %d WITH IDX %d OUTPUT: %s" FG_RST "\n", getpid(), idx,
            buf);

    return 0;
}

int remove_first(int idx) {
    char buf[BUF_LEN + 1];
    bzero(buf, BUF_LEN + 1);
    int num_read = read(pipes[idx][0], buf, BUF_LEN);
    close(pipes[idx][0]);

    for (int i = 1; buf[i] != '\0'; i++) {
        buf[i - 1] = buf[i];
    }
    buf[num_read - 1] = '\0';

    write(pipes[(idx + 1) % NUM_PROC][1], buf, num_read - 1);
    close(pipes[(idx + 1) % NUM_PROC][1]);
    fprintf(stdout, FG_YLW "PROCESS %d WITH IDX %d OUTPUT: %s" FG_RST "\n", getpid(), idx,
            buf);

    return 0;
}

int remove_last(int idx) {
    char buf[BUF_LEN + 1];
    bzero(buf, BUF_LEN + 1);
    int num_read = read(pipes[idx][0], buf, BUF_LEN);
    close(pipes[idx][0]);

    buf[num_read - 1] = '\0';

    write(pipes[(idx + 1) % NUM_PROC][1], buf, num_read - 1);
    close(pipes[(idx + 1) % NUM_PROC][1]);
    fprintf(stdout, FG_YLW "PROCESS %d WITH IDX %d OUTPUT: %s" FG_RST "\n", getpid(), idx,
            buf);

    return 0;
}

int main(int argc, char *argv[]) {
    int proc_idx = 0;
    int curr_proc_idx = 0;
    procs[0] = getpid();

    for (int i = 0; i < NUM_PROC; i++) {
        pipe(pipes[i]);
    }

    for (int i = 1; i < NUM_PROC; i++) {
        proc_idx++;
        int res = fork();
        if (res == 0) {
            curr_proc_idx = proc_idx;
            break;
        }
        procs[i] = res;
    }

    // READ FROM PIPE AT INDEX CURR_PROC_IDX
    // WRITE FROM PIPE AT INDEX CURR_PROC_IDX + 1
    for (int i = 0; i < NUM_PROC; i++) {
        if (i != curr_proc_idx) close(pipes[i][0]);
        if (i != (curr_proc_idx + 1) % NUM_PROC) close(pipes[i][1]);
    }

    int res = proc_methods[curr_proc_idx](curr_proc_idx);
    if (curr_proc_idx == 0)
        for (int i = 1; i < NUM_PROC; i++) waitpid(procs[i], NULL, 0);

    return res;
}
