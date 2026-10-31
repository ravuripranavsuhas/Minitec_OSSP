/* ============================================================
   Smart Process Management System
   Demonstrates: fork(), exec(), wait(), kill(), getpid(), getppid()
   Compile: gcc process_manager.c -o pm
   Run:     ./pm
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

/* Keep track of children we've spawned so we can wait()/kill() them later */
#define MAX_CHILDREN 32
static pid_t children[MAX_CHILDREN];
static int   child_count = 0;

/* ------------------------------------------------------------
   1. CREATE  ->  fork() + exec()
   ------------------------------------------------------------
   fork() clones the calling process. It returns TWICE:
     - in the PARENT it returns the child's new PID (> 0)
     - in the CHILD  it returns 0
     - on failure it returns -1 (no child was created)
   The child then calls exec() to replace its own memory image
   with a new program, while keeping the same PID.
*/
void create_process(void) {
    char cmd[128];
    printf("Enter a command to run (e.g. ls -l, date, echo hi): ");
    getchar(); /* consume leftover newline from previous scanf */
    fgets(cmd, sizeof(cmd), stdin);
    cmd[strcspn(cmd, "\n")] = 0; /* strip trailing newline */

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        return;
    }

    if (pid == 0) {
        /* ---- CHILD PROCESS ---- */
        printf("[child] Child PID %d spawned, parent PID %d\n",
               getpid(), getppid());

        /* Split the typed command into argv[] for execvp */
        char *argv[16];
        int i = 0;
        char *token = strtok(cmd, " ");
        while (token != NULL && i < 15) {
            argv[i++] = token;
            token = strtok(NULL, " ");
        }
        argv[i] = NULL;

        if (i == 0) {
            fprintf(stderr, "[child] No command given, exiting.\n");
            _exit(1);
        }

        /* execvp() REPLACES this process's code/data/stack in place.
           If it succeeds, the lines after it never run, because this
           process is no longer running process_manager's code at all. */
        execvp(argv[0], argv);

        /* Only reached if execvp() itself failed (e.g. bad command) */
        perror("[child] execvp failed");
        _exit(1);
    } else {
        /* ---- PARENT PROCESS ---- */
        if (child_count < MAX_CHILDREN) {
            children[child_count++] = pid;
        }
        printf("[parent] Spawned child PID %d (not waiting yet)\n", pid);
    }
}

/* ------------------------------------------------------------
   2. MONITOR  ->  getpid() / getppid()
   ------------------------------------------------------------
   Just reads kernel-maintained bookkeeping fields for the
   calling process; no process is created or destroyed.
*/
void monitor_process(void) {
    printf("This program : PID = %d, Parent PID = %d\n", getpid(), getppid());
    if (child_count == 0) {
        printf("No tracked children yet.\n");
        return;
    }
    printf("Tracked children:\n");
    for (int i = 0; i < child_count; i++) {
        /* kill(pid, 0) sends no signal but checks if the PID still exists */
        if (kill(children[i], 0) == 0)
            printf("  [%d] PID %d - alive\n", i, children[i]);
        else
            printf("  [%d] PID %d - not running (exited or reaped)\n", i, children[i]);
    }
}

/* ------------------------------------------------------------
   3. SYNCHRONIZE  ->  wait()
   ------------------------------------------------------------
   wait() blocks the calling (parent) process until ANY one of
   its children terminates. It returns that child's PID and,
   through the status pointer, how the child exited.
*/
void wait_for_child(void) {
    if (child_count == 0) {
        printf("No children to wait for.\n");
        return;
    }
    int status;
    printf("[parent] Blocking on wait()...\n");
    pid_t finished = wait(&status);

    if (finished == -1) {
        perror("wait failed");
        return;
    }

    if (WIFEXITED(status)) {
        printf("[parent] Child %d exited normally, status %d\n",
               finished, WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        printf("[parent] Child %d was killed by signal %d\n",
               finished, WTERMSIG(status));
    }
}

/* ------------------------------------------------------------
   4. TERMINATE  ->  kill()
   ------------------------------------------------------------
   kill() doesn't necessarily kill -- it delivers a SIGNAL to a
   target PID. SIGTERM (15) asks the process to shut down
   gracefully; SIGKILL (9) forces immediate termination.
*/
void terminate_process(void) {
    if (child_count == 0) {
        printf("No children to terminate.\n");
        return;
    }
    monitor_process();
    printf("Enter index of child to terminate: ");
    int idx;
    scanf("%d", &idx);
    if (idx < 0 || idx >= child_count) {
        printf("Invalid index.\n");
        return;
    }

    printf("Sending SIGTERM to PID %d...\n", children[idx]);
    if (kill(children[idx], SIGTERM) == 0) {
        printf("Signal sent. Reap it with 'wait' to avoid a zombie.\n");
    } else {
        perror("kill failed");
    }
}

/* ------------------------------------------------------------ */
void print_menu(void) {
    printf("\n===== SMART PROCESS MANAGEMENT SYSTEM =====\n");
    printf("PID: %d  |  Parent PID: %d\n", getpid(), getppid());
    printf("1. Create process   (fork + exec)\n");
    printf("2. Monitor processes (getpid/getppid/kill-check)\n");
    printf("3. Wait for a child  (wait)\n");
    printf("4. Terminate a child (kill)\n");
    printf("5. Exit\n");
    printf("Choose an option: ");
}

int main(void) {
    int choice;
    printf("Smart Process Management System started. Main PID = %d\n", getpid());

    while (1) {
        print_menu();
        if (scanf("%d", &choice) != 1) {
            /* clear bad input */
            while (getchar() != '\n');
            continue;
        }

        switch (choice) {
            case 1: create_process();      break;
            case 2: monitor_process();     break;
            case 3: wait_for_child();      break;
            case 4: terminate_process();   break;
            case 5:
                printf("Exiting. Reaping any remaining children...\n");
                while (waitpid(-1, NULL, WNOHANG) > 0);
                return 0;
            default:
                printf("Invalid option.\n");
        }
    }
}
