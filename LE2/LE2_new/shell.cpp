/****************
LE2: Introduction to Unnamed Pipes
****************/
#include <iostream>
#include <unistd.h> // pipe, fork, dup2, execvp, close
using namespace std;

int main () {
    // lists all the files in the root directory in the long format
    char* cmd1[] = {(char*) "ls", (char*) "-al", (char*) "/", nullptr};
    // translates all input from lowercase to uppercase
    char* cmd2[] = {(char*) "tr", (char*) "a-z", (char*) "A-Z", nullptr};

    // TODO: add functionality
     int pipefd[2];
    // Create the pipe
    if (pipe(pipefd) == -1) {
        perror("pipe failed");
        return 1;
    }
    
    // Create child to run first command
    pid_t pipefd1 = fork();
    if (pipefd1 == -1) {
        perror("fork failed");
        return 1;
    }
    if(pipefd1 == 0) {
        // In child, redirect output to write end of pipe
        dup2(pipefd[1], STDOUT_FILENO);
        // Close the read end of the pipe on the child side.
        close(pipefd[0]);
        // In child, execute the command
        execvp(cmd1[0], cmd1);
    }

    // Create another child to run second command
    pid_t pipefd2 = fork();
    if (pipefd2 == -1) {
        perror("fork failed");
        return 1;
    }
    if(pipefd2 == 0) {
        // In child, redirect input to the read end of the pipe
        dup2(pipefd[0], STDIN_FILENO);
        // Close the write end of the pipe on the child side.
        close(pipefd[1]);
        // Execute the second command.
        execvp(cmd2[0], cmd2);
    }

    // Reset the input and output file descriptors of the parent.
    close(pipefd[0]);
    close(pipefd[1]);
    return 0;
}
