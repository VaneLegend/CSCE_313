#include <iostream>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <termios.h>

#include <vector>
#include <string>
#include <ctime>
#include <cstring>

#include "Tokenizer.h"

// all the basic colours for a shell prompt
#define RED "\033[1;31m"
#define GREEN "\033[1;32m"
#define YELLOW "\033[1;33m"
#define BLUE "\033[1;34m"
#define WHITE "\033[1;37m"
#define NC "\033[0m"
#define PATH_MAX 500

using namespace std;

/**
 * Return Shell prompt to display before every command as a string
 *
 * Prompt format is: `{MMM DD hh:mm:ss} {user}:{current_working_directory}$`
 */
void print_prompt()
{
        // Use time() to obtain the current system time, and localtime() to parse the raw time data
        time_t current_sys_time = time(NULL);
        char time[PATH_MAX];
        strftime(time, sizeof(time), "%b %e %H:%M:%S %Y", localtime(&current_sys_time));

        // Parse date time string from raw time data using strftime()
        // Date-Time format will be in the format (MMM DD hh:mm:ss), with a constant size of 15 characters requiring 16 bytes to output

        // Obtain current user and current working directory by calling getenv() and obtaining the "USER" and "PWD" environment variables
        char *user = getenv("USER");
        char current_directory[PATH_MAX];
        getcwd(current_directory, size(current_directory));

        // Can use colored #define's to change output color by inserting into output stream
        // MODIFY to output a prompt format that has at least the USER and CURRENT WORKING DIRECTORY
        std::cout << GREEN << time << BLUE << " " << user << ":" << current_directory << "$" << NC << " ";
}

int main () {
    vector<pid_t> backgroundProcesses;

    for (;;) {
        print_prompt();
        string input;
        getline(cin, input);
        
        if (input == "exit") {
            cout << RED << "Now exiting shell..." << endl << "Goodbye" << NC << endl;
            break;
        }

        Tokenizer tknr(input);
        if (tknr.hasError()) {
            continue;
        }

        for (auto cmd : tknr.commands) {
            for (auto str : cmd->args) {
                cerr << "|" << str << "| ";
            }

            if (cmd->hasInput()) {
                cerr << "in< " << cmd->in_file << " ";
            }

            if (cmd->hasOutput()) {
                cerr << "out> " << cmd->out_file << " ";
            }
            cerr << endl;
        }
       
        int previous_fds[2];  // Save the previous pipe for reading input
        char current_directory[PATH_MAX];
        getcwd(current_directory, size(current_directory));
        char prev_directory[PATH_MAX];

        if (tknr.commands[0]->args.at(0) == "cd") {
            if (tknr.commands[0]->args.at(1) == "-"){
                if (chdir(prev_directory) != 0){
                    char temp[PATH_MAX];
                    strcpy(temp, current_directory);
                    strcpy(current_directory, prev_directory);
                    strcpy(prev_directory, temp);
                }
            } else {
                strcpy(prev_directory, current_directory);
                chdir(tknr.commands[0]->args.at(1).c_str());
                getcwd(current_directory, sizeof(current_directory));
            }
        }

        for (size_t i = 0; i < tknr.commands.size(); ++i) {
            Command* cmd = tknr.commands[i];
            
            int next_fds[2];  // Pipe for the next command
            
            if (i < tknr.commands.size() - 1) {
                if (pipe(next_fds) < 0) {
                    perror("pipe error");
                    wait(nullptr);
                    exit(2);
                }
            }

            pid_t pid = fork();
            if (pid < 0) {
                perror("fork error");
                wait(nullptr);
                exit(2);
            }

            if (pid == 0) {  // Child process
                if (i > 0) {  // Not the first command, so get input from previous pipe
                    dup2(previous_fds[0], 0);  // Redirect stdin to the previous pipe
                    close(previous_fds[0]);
                    close(previous_fds[1]);
                }

                if (i < tknr.commands.size() - 1) {  // Not the last command, so output to the next pipe
                    dup2(next_fds[1], 1);  // Redirect stdout to the next pipe
                    close(next_fds[0]);
                    close(next_fds[1]);
                }

                if (cmd->hasInput()) {
                    int file = open((cmd->in_file).c_str(), O_RDONLY, S_IRUSR|S_IWUSR);
                    dup2(file, 0);
                    close(file);
                }

                if (cmd->hasOutput()) {
                    int file = open((cmd->out_file).c_str(), O_WRONLY|O_CREAT,S_IRUSR|S_IWUSR);
                    dup2(file, 1);
                    close(file);
                }

                vector<char *> args;
                for (auto& arg : cmd->args) {
                    args.push_back((char*)arg.c_str());
                }
                args.push_back(nullptr);

                if (execvp(args[0], args.data()) < 0) {
                    perror("execvp");
                    wait(nullptr);
                    exit(2);
                }
            } else {  // Parent process
                if (i > 0) {  // Close the previous pipe in the parent
                    close(previous_fds[0]);
                    close(previous_fds[1]);
                }
                
                if (i < tknr.commands.size() - 1) {
                    previous_fds[0] = next_fds[0];  // Move forward the file descriptors
                    previous_fds[1] = next_fds[1];
                }
                
                if (!tknr.commands[i]->isBackground()) {
                    waitpid(pid, NULL, 0);
                } else {
                    backgroundProcesses.push_back(pid);
                }
            }
        }
    }
}
