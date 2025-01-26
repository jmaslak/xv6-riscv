#include "kernel/types.h"
#include "kernel/syscall.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    printf("Testing dup2()\n");

    int fd_stdin  = dup(0);
    int fd_stdout = dup(1);
    int fd_stderr = dup(2);

    //
    // Just verify expected dup() behavior.
    close(0);
    close(2);
    int fd_test = dup(fd_stdout);
    if (fd_test != 0) {
        fprintf(fd_stderr, "ERR: fd_test expected 0, actual %d\n", fd_test);
        exit(1);
    }
    close(0);

    //
    // Do 1000 dup2() calls!
    for (int i=0; i<1000; i++) {
        dup2(fd_stdout, 2);
    }

    //
    // Make sure that the old file is closed if needed and that closes
    // don't different handles.
    int p_to_client[2], p_to_server[2];
    if (pipe(p_to_client)) {
        fprintf(fd_stderr, "ERR: can't open p_to_client\n");
        exit(1);
    }
    if (pipe(p_to_server)) {
        fprintf(fd_stderr, "ERR: can't open p_to_server\n");
        exit(1);
    }

    int pid = fork();
    if (pid < 0) {
        fprintf(fd_stderr, "ERR: fork failed\n");
    } else if (pid == 0) {
        // Client
        if (close(p_to_server[0])) {
            fprintf(fd_stderr, "ERR: can't close to-server read pipe\n");
            exit(1);
        }
        if (close(p_to_client[1])) {
            fprintf(fd_stderr, "ERR: can't close to-server write pipe\n");
            exit(1);
        }
        
        // We are going to dupe self-to-self here. This should not cause
        // any problems.  This verifies we don't increment the ref
        // count.
        dup2(p_to_client[0], p_to_client[0]);

        // We're also going to make sure we can increment a ref count
        // here, but duping, duping back, and closing the second copy.
        int newfd = dup(fd_stdin);
        dup2(p_to_client[0], newfd);
        dup2(newfd, p_to_client[0]);
        if (close(newfd)) {
            fprintf(fd_stderr, "ERR: client close after dup2 failed\n");
            exit(1);
        }

        char buff[1];
        // We are waiting for the server to close the pipe.
        while (read(p_to_client[0], buff, 1)) { }

        buff[0] = 'A';
        int ret = write(p_to_server[1], buff, 1);
        if (ret != 1) {
            fprintf(fd_stderr, "ERR: can't write to to-server pipe (%d)\n", ret);
            exit(1);
        }
        exit(0);
    } else {
        // Server
        if (close(p_to_server[1])) {
            fprintf(fd_stderr, "ERR: can't close to-server write pipe\n");
            exit(1);
        }
        if (close(p_to_client[0])) {
            fprintf(fd_stderr, "ERR: can't close to-client read pipe\n");
            exit(1);
        }

        // We are going to dupe over the to_client write pipe to close
        // it. This should cause the client to send us a character.
        // If it fails to close, we will hang.
        dup2(fd_stdout, p_to_client[1]);

        char buff[1];
        int ret = read(p_to_server[0], buff, 1);
        if (ret != 1) {
            fprintf(fd_stderr, "ERR: can't read from client\n");
            exit(1);
        }
        if (buff[0] != 'A') {
            fprintf(fd_stderr, "ERR: read invalid value from client\n");
            exit(1);
        }

        if (close(p_to_client[1])) {
            fprintf(fd_stderr, "ERR: close to_client (really stdout) failed\n");
            exit(1);
        }
    }


    //
    // Output test.
    dup2(fd_stdout, 2);
    fprintf(2, "1111111111\n");
    printf("A bunch of '1' characters should have appeareda bove this line.\n");

    //
    // Reset things
    dup2(fd_stdin, 0);
    dup2(fd_stdout, 1);
    dup2(fd_stderr, 2);

    printf("All good\n");
    exit(0);
}

