#include "kernel/types.h"
#include "user/user.h"

void panic(char *s);
void child(int pipe_read, int pipe_write);
void parent(int pipe_read, int pipe_write);

// We'll time a 100,000 serves.
#define SERVES (100000l)

int delay = 0;  // Used to delay writes in child process overwrite parent.

int main(int argc, char *argv[]) {
    int p_to_client[2];
    int p_to_server[2];

    if (pipe(p_to_client)) panic("main: could not create to-client pipe\n");
    if (pipe(p_to_server)) panic("main: could not create to-server pipe\n");

    sleep(10);

    int pid = fork();
    if (pid < 0) {
        panic("main: could not fork\n");
    } else if (pid == 0) {
        delay = 20;
        if (close(p_to_server[0])) panic("child: could not close to-server read pipe\n");
        if (close(p_to_client[1])) panic("child: could not close to-client write pipe\n");
        child(p_to_client[0], p_to_server[1]);
    } else {
        if (close(p_to_client[0])) panic("parent: could not close to-client read pipe\n");
        if (close(p_to_server[1])) panic("parent: could not close to-server write pipe\n");
        parent(p_to_server[0], p_to_client[1]);
    }
    wait((int*) 0);
    exit(0);
}

void child(int pipe_read, int pipe_write) {
    char buff[1];

    while (read(pipe_read, buff, 1) == 1) {
        if (write(pipe_write, buff, 1) != 1) panic("child: could not write to parent\n");
    }

    if (close(pipe_read)) panic("child: could not close read pipe\n");
    if (close(pipe_write)) panic("child: could not close write pipe\n");

    sleep(delay);
    printf("child: exiting normally\n");
}

void parent(int pipe_read, int pipe_write) {
    char buff[1];

    uint64 cnt = 0;

    buff[0] = 'b';
    uint64 start_time = nanotime();

    // Initial volley
    if (write(pipe_write, buff, 1) != 1) panic("parent: could not write to pipe\n");

    int ok = 0;
    while (read(pipe_read, buff, 1) == 1) {
        if (++cnt == SERVES) {
            printf("parent: number of serves has been reached\n");
            ok = 1;
            break;
        }

        if (write(pipe_write, buff, 1) != 1) panic("parent: could not write to child\n");
    }

    if (ok) {
        uint64 end_time = nanotime();
        int per_second = 1000000000l * SERVES / (end_time - start_time);
        printf("Round trips per second (%lu trips): %d/sec\n", SERVES, per_second);
    } else {
        panic("parent: could not read from pipe!");
    }

    if (close(pipe_read)) panic("parent: could not close read pipe\n");
    if (close(pipe_write)) panic("parent: could not close write pipe\n");
}

void panic(char *s) {
    sleep(delay);
    write(stderr, s, strlen(s));  // Ya, we ignore errors, because what
                                  // would we do anyhow?
    sleep(40);
    exit(1);
}

