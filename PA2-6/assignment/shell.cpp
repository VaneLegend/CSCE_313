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
#define RED     "\033[1;31m"
#define GREEN	"\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE	"\033[1;34m"
#define WHITE	"\033[1;37m"
#define NC      "\033[0m"
#define PATH_MAX 500


using namespace std;


/**
 * Return Shell prompt to display before every command as a string
 * 
 * Prompt format is: `{MMM DD hh:mm:ss} {user}:{current_working_directory}$`
 */
void print_prompt() {
    // Use time() to obtain the current system time, and localtime() to parse the raw time data
    time_t current_sys_time = time(NULL);
    char time[PATH_MAX];
    strftime(time, sizeof(time), "%b %e %H:%M:%S %Y", localtime(&current_sys_time));

    // Parse date time string from raw time data using strftime()
    // Date-Time format will be in the format (MMM DD hh:mm:ss), with a constant size of 15 characters requiring 16 bytes to output

    // Obtain current user and current working directory by calling getenv() and obtaining the "USER" and "PWD" environment variables
    char* user = getenv("USER");
    char current_directory[PATH_MAX];
    getcwd(current_directory, size(current_directory));

    // Can use colored #define's to change output color by inserting into output stream
    // MODIFY to output a prompt format that has at least the USER and CURRENT WORKING DIRECTORY
    std::cout << GREEN << time << BLUE << " " << user << ":" << current_directory << "$" << NC << " ";
}


/**
 * Process shell command line
 * 
 * Each command line from the shell is parsed by '|' pipe symbols, so command line must be iterated to execute each command
 * Example: `ls -l / | grep etc`    ::  This command will list (in detailed list form `-l`) the root directory and then pipe into a filter for `etc`
 * When parsed into the Tokenizer, this will split into two separate commands, `ls -l /` and `grep etc`.
 */
void process_commands(Tokenizer tknr) {
    // Declare file descriptor variables for storing unnamed pipe fd's
    // Maintain both a FORWARD and BACKWARDS pipe in the parent for command redirection
    int backwards_fds[2];
    int forwards_fds[2];
    char current_directory[PATH_MAX];
    getcwd(current_directory, size(current_directory));
    char prev_directory[PATH_MAX];

    // LOOP THROUGH COMMANDS FROM SHELL INPUT

    for (long unsigned int i = 0; i < tknr.commands.size(); i++) {
        Command* cmd = tknr.commands[i];
        // Check if the command is 'cd' first
        // This will not have any pipe logic and needs to be done manually since 'cd' is not an executable command
        // Use getenv(), setenv(), and chdir() here to implement this command
        if (tknr.commands[0]->args.at(0) == "cd") {
            // There are two tested inputs for 'cd':
            // (1) User provides "cd -", which directs to the previous directory that was opened
            if (tknr.commands[0]->args.at(1) == "-"){
                if (chdir(prev_directory) != 0){
                    char temp[PATH_MAX];
                    strcpy(temp, current_directory);
                    strcpy(current_directory, prev_directory);
                    strcpy(prev_directory, temp);
                }
            } else {
                // (2) User provides a directory to open    
                strcpy(prev_directory, current_directory);
                chdir(tknr.commands[0]->args.at(1).c_str());
                getcwd(current_directory, sizeof(current_directory));
            }
                            
            continue;
        }
        
        // Initialize backwards pipe to forward pipe
        if (pipe(backwards_fds) < 0){
                perror("pipe error");
                exit(2);
            }
        // If any other command, set up forward pipe IF the current command is not the last command to be executed
        if (cmd != tknr.commands.back()) {
            if (pipe(forwards_fds) < 0){
                perror("pipe error");
                exit(2);
            }
        }

        // fork to create child
        pid_t pid = fork();
        if (pid < 0) {  // error check
            perror("fork error");
            exit(2);
        }
        if (pid == 0) {  // if child, exec to run command
            if (cmd != tknr.commands.back()) {  // i.e. {current command} | {next command} ...
                dup2(forwards_fds[1], 1);     // Reidrect STDOUT to forward pipe
                close(forwards_fds[1]);    // Close respective pipe end
                close(forwards_fds[0]);
            }

            if (cmd != tknr.commands.front()) {    // i.e. {first command} | {current command} ...
                dup2(backwards_fds[0], 0);     // Redirect STDIN to backward pipe
                close(backwards_fds[0]);    // Close respective pipe end
                close(backwards_fds[1]);
            }
            
            if (cmd->hasInput()) {    // i.e. {command} < {input file}
                int file = open((cmd->in_file).c_str(), O_RDONLY, S_IRUSR|S_IWUSR); // Open input file
                dup2(file, 0); // Redirect STDIN from file
                close(file);
            }

            if (cmd->hasOutput()) {   // i.e. {command} > {output file}
            int file = open((cmd->in_file).c_str(), O_WRONLY|O_CREAT,S_IRUSR|S_IWUSR); // Open output file
                dup2(file, 1); // Redirect STDOUT to file
                close(file);
            }

            // Execute command after any redirection
            char* args[] = {(char*) tknr.commands.at(0)->args.at(0).c_str(), nullptr};
            if (execvp(args[0], args) < 0) {  // error check
                perror("execvp");
                exit(2);
            }
        }
        else {  // if parent, wait for child to finish    
            // Close pipes from parent so pipes receive `EOF`
            // Pipes will otherwise get stuck indefinitely waiting for parent input/output that will never occur
            if (cmd != tknr.commands.back()) {  // i.e. {current command} | {next command} ...
                backwards_fds[0] = forwards_fds[0];
                backwards_fds[1] = forwards_fds[1];
            } else {
                close(forwards_fds[0]);
            }

            if (cmd != tknr.commands.front()) {    // i.e. {first command} | {current command} ...
                close(backwards_fds[0]);
                close(backwards_fds[1]);
            }

            // If command is indicated as a background process, set up to ignore child signal to prevent zombie processes
            if (cmd->isBackground()) {
                signal(SIGCHLD, SIG_IGN);
            }
            else {
                int status = 0;
                waitpid(pid, &status, 0);
                
                if (status > 1) {  // exit if child didn't exec properly
                    exit(status);
                }
            }
        }
    }
}


int main () {
    // Use setenv() and getenv() to set an environment variable for the previous PWD from 'PWD'
    //setenv("PWD", getenv("PWD"), 0);
    
    // This is your process loop; Will run infinitely until given defined break case
    for (;;) {
        // need to print date/time, username, and absolute path to current dir into 'shell'
        // Might want to use a helper function like this here for code clarity
        print_prompt();
                
        // get user inputted command
        string input;
        getline(cin, input);
        
        if (input == "exit") {  // print exit message and break out of infinite loop
            cout << RED << "Now exiting shell..." << endl << "Goodbye" << NC << endl;
            break;
        }
        // Might want to catch empty input; Tokenizer will error and not recover on empty input

        // get tokenized commands from user input
        // This will parse your user input for you; Please refer the header files to understand how to use this class
        Tokenizer tknr(input);
        if (tknr.hasError()) {  // continue to next prompt if input had an error
            continue;
        }


        
        // print out every command token-by-token on individual lines
        // prints to cerr to avoid influencing autograder
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
        
       
        // Might want to use a helper function like this for processing the shell's command line 
        process_commands(tknr);
    }
}