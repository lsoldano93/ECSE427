#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
// Needed for sig kill
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

typedef int bool;
#define true 1
#define false 0

// --------------------------------------------------------------

int getcmd(char *prompt, char *args[], int *background, char *cmdHistory[20][10], int cmdsEntered, int *historyIndex)
{
    int length = 0; 
    int i = 0; 
    int z = 0; 
    int nullIndex = 0;
    int arrayLimit = 0;

    char *token = NULL; 
    char *loc = NULL;
    char *line = NULL;

    // 16-bit Int used to represent the size of an object
    size_t linecap = 0;

    // repeatCmd set when format 'r x' or 'r' is entered; nullFound for i analysis
    bool repeatCmd = false;
    bool nullFound = false;
    bool cmdExists = true;

    printf("%s", prompt);

    // getLine() will read 'stdin' to 'line', store size in bytes in 'linecap',
    // This function will return # of chars read
    // ****** problem here in runs > 1st ***********
    length = getline(&line, &linecap, stdin);

    // If nothing read (length = 0) or error (length = -1) then exit shell
    if (length <= 0) {
        exit(-1);

    }
 
    // Check if background is specified..
    // AKA if there is an ambersand in the entered line, make the child run concurrently
    // If true, turn ambersand in to a space to not confuse later processes
    if ((loc = index(line, '&')) != NULL) {
        *background = 1;
        *loc = ' ';

    } 

    else {
        *background = 0;

    }
    
    // Tokens are delimited by spaces, tabs, and new lines
    // While loop runs as long as argument isn't null
    while ((token = strsep(&line, " \t\n")) != NULL) {
        
        // Go through every character of token string
        for (int j=0; j < strlen(token); j++){
            
            //  If token <= 32, set character to null
            if (token[j] <= 32){
                token[j] = '\0';

            }

            // Looking for first arguments of 'r' to initiate history process
            if (((strcmp(token, "r")) == 0) && (strlen(token) == 1) && (i == 0)){
                repeatCmd = true;

            }
        }

        // Insert token as argument if not empty space 
        if ((strlen(token) > 0) && (!repeatCmd)){
            args[i++] = token;

            // 99 is a bum value to indicate to main that command is not a history command
            *historyIndex = 99;

        }

        // If first argument and history repeatCmd set, input most recent command to args
        else if ((strlen(token) > 0) && (i == 0)){
            
            if (cmdsEntered > 0){
                for (int j=0; j < 20; j++){
                    args[j] = cmdHistory[j][0];
                    
                    // Save index of the first null so number of commands is known
                    if(args[j] == NULL && !nullFound){
                        nullIndex = j;
                        nullFound = true;

                    }
                }
                *historyIndex = 0;

            }
            else {
                cmdExists = false;
                printf("No command previously entered - no most recent command exists!");

            }
            // Make sure i is incremented so code still works in original format
            i++;

        }

        // Make token have to be 'r' or else fail
        // If more arguments exist (than just 'r') start 
        else if ((strlen(token) > 0) && (i == 1) && (repeatCmd)){
           
            if (cmdsEntered < 10){
                arrayLimit = cmdsEntered;

            }
            else{
                arrayLimit = 10;

            }

            // Search for most recently used command that starts with letter 'x' in command 'r x'
            while ( ((int) (index(cmdHistory[0][z], *token) - ((int)cmdHistory[0][z]))) != 0 && z < arrayLimit){
                z++;

                if (z == arrayLimit){
                    cmdExists = false;
                    printf("\nNo stored command matches the given first letter.\n");
                }

            }

            // Once proper index found, copy values into args
            // If index is most recent command then values have already been copied
            if (z != 0 && cmdExists){

                // Reset boolean to allow previous copy procedure to operate the same
                nullFound = false;

                for (int j=0; j < 20; j++){
                    args[j] = cmdHistory[j][z];

                    // Save index of the first null so number of commands is known
                    if(args[j] == NULL && !nullFound){
                        nullIndex = j;
                        nullFound = true;

                    }
                }
                *historyIndex = z;

            }
        }
    }

    // Return i at proper history value if command 'r' or 'r x'
    if(repeatCmd){
        i = nullIndex;
    }

    return i;
}

// --------------------------------------------------------------

int freecmd(char *args[], char *params[])
{
    // Nullify char arrays to avoid carryover from previous commands
    for (int i = 0; i < 20; i++){
        args[i] = NULL;
        params[i] = NULL;
    }

    return 0;
}

// --------------------------------------------------------------

int main()
{
    char *buff = NULL;

    // Args array is 20 characters long max
    char *args[20];
    char *params[20];
    char *jobNames[20];

    // cmdHistory will have most recent command at [0,0] and oldest at [0,10]
    char *cmdHistory[20][10];

    for(int i=0; i < 10; i++){
        for(int j=0; j < 20; j++)
        cmdHistory[j][i] = NULL;
    }

    int p = 0;
    int q = 1;
    int x = 0;
    int bg = 0; 
    int cnt = 0; 
    int status = 0;
    int cmdsEntered = 0; 
    int numJobs = 0;
    int historyIndex = 0;

    int bgHistory[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    bool builtInCmd = false;
    bool childToFg = false;
    bool erroneous = false;

    bool cmdSuccess[10];

    pid_t pid;

    pid_t jobPids[20];

    for(int i=0; i < 20; i++){
        jobNames[i] = NULL;
        jobPids[i] = NULL;
    }

    while(true){

        erroneous = false;

        // Intialize args and params with NULL values
        cnt = freecmd(args, params);

        // Returns number of arguments following the command
        cnt = getcmd("\nshell >>  ", args, &bg, cmdHistory, cmdsEntered, &historyIndex);

        // Run functions if something was actually entered
        if (cnt > 0){

            // If exit, back out before history, etc. entered
            if ((strcmp(args[0], "exit")) == 0){
                exit(EXIT_SUCCESS);
            }

            // Ping all children and read status
            if (numJobs > 0){

                for (int j=0; j < numJobs; j++){
                    pid = waitpid(jobPids[j], &status, WNOHANG);

                    // PID of child is returned if process has completed/changed state
                    // Second flag is for processes brought to foreground
                    if (pid == jobPids[j] || (childToFg && j == q - 1)){

                        // Kill zombie process
                        kill(jobPids[j], SIGKILL);

                        // If job is most recent one added to array, replace with NULL
                        if (j == numJobs - 1){
                            jobPids[j] = NULL;
                            jobNames[j] = NULL;
                            numJobs--;

                        }

                        else {
                            for (int i=j; i < numJobs; i++){

                                // If not at last logical index, move information up an index
                                if (i < numJobs - 1){
                                    jobPids[i] = jobPids[i+1];
                                    jobNames[i] = jobNames[i+1];

                                }

                                // Otherwise we are last logical place in array, write NULL
                                else {
                                    jobPids[i] = NULL;
                                    jobNames[i] = NULL;
                                    numJobs--;

                                }

                            }

                        }

                        // This boolean should be false no matter the scenario by this point
                        childToFg = false;

                    }

                }

            }

            // Prints all arguments in the argument array
            for (int i=0; i < cnt; i++){
                printf("\nArg[%d] = %s", i, args[i]);

                // Make arg array without command at [0] for easy pass in execvp
                if (i != 0){
                    params[i-1] = args[i];
                }
            }

            printf("\n");

            // If built in command, set builtInCmd flag true
            if ((strcmp(args[0], "history")) == 0 || (strcmp(args[0], "cd")) == 0 
                || (strcmp(args[0], "pwd")) == 0 || (strcmp(args[0], "fg")) == 0 
                || (strcmp(args[0], "jobs")) == 0){

                builtInCmd = true;
            }

            // Check repeat commands for being erroneous and check if they were concurrent
            if (historyIndex != 99){
                erroneous = cmdSuccess[historyIndex];
                bg = bgHistory[historyIndex];

            }

            // If an erroneous repeat command, print an error
            if (erroneous && historyIndex != 99){
                printf("\nCommand to be repeated was previously found to be erroneous\n");

            }

            else{
                // Store previous command arguments
                // Iterate through past commands
                for(int j=9; j > 0; j--){

                    cmdSuccess[j] = cmdSuccess[j-1];

                    for(int i=0; i < 20; i++){
                        // Pass command in to next "older" spot
                        cmdHistory[i][j] = cmdHistory[i][j-1];
                    }
                }

                // Write newest command in first place
                for(int i=0; i < 20; i++){
                    cmdHistory[i][0] = args[i];
                }
            }

            // If command not built in to shell, fork new process to run command
            if (!builtInCmd && !erroneous){

                // Command is valid enough to increment this counter by this point
                cmdsEntered++;

                // Push old bg commands forward only if command not erroneous
                for(int j=9; j > 0; j--){
                    bgHistory[j] = bgHistory[j-1];

                }

                bgHistory[0] = bg;

                // If bg=1 then run process concurrently
                if (bg){
                    printf("\nBackground enabled..\n");

                    // Create new process to run the command
                    pid = fork();

                    // If pid = 0, you are in the child process
                    if (pid == 0){
                        execvp(args[0], params);

                        // If child returns, failure occured, exit
                        printf("\nexecvp failed \n");
                        _exit (EXIT_FAILURE);
                    }

                    else if (pid < 0){
                        printf("\nFork failed \n");
                    }

                    // Have parent process store PID of child and process name
                    else {

                        jobNames[numJobs] = args[0];
                        jobPids[numJobs] = pid;
                        numJobs++;

                    }
                }
                
                // If bg=/=1 then wait for child to finish
                else{
                    printf("\nBackground not enabled \n");

                    // Create new process to run the command
                    pid = fork();

                    // If pid = 0, you are in the child process
                    if (pid == 0){
                        execvp(args[0], params);

                        // If child returns, failure occured, exit
                        printf("\nexecvp failed \n");
                        _exit (EXIT_FAILURE);
                    }

                    else if (pid < 0){
                        printf("\nFork failed \n");
                    }

                     // Otherwise (if not -1) you are in the parent process
                    else{
                        waitpid(pid, &status, 0);
                        
                        // If status is returned as a non-zero, an issue occurred with child
                        if (status != 0){
                            erroneous = true;
                        }

                        // Kill zombie process
                        kill(pid, SIGKILL);
                    }

                }

                // Store most recent successes
                cmdSuccess[0] = erroneous;

                printf("\n\n");

            }

            // Run built-in command
            else if (!erroneous){

                // Command is valid enough to increment this counter by this point
                cmdsEntered++;
                
                // Push old bg commands forward only if command not erroneous
                for(int j=9; j > 0; j--){
                    bgHistory[j] = bgHistory[j-1];

                }

                bgHistory[0] = bg;

                // Run history command
                if ((strcmp(args[0], "history")) == 0){
                
                    // Header
                    printf("\n\nPreviously entered commands - [command #]. [command args]:\n");

                    // Makes print out of history easier
                    p = 9;
                    q = 1;
                    x = 9;

                    // Start list from oldest stored command entered, end with newest command
                    for(int j = cmdsEntered - 10; j < cmdsEntered; j++){

                        // If command isn't null (this many commands have been entered) then print
                        if (j >= 0){

                            // Print command iteration and command
                            printf("%d. %s", (j+1), cmdHistory[0][x]);

                            // Print following arguments
                            while(cmdHistory[q][x] != NULL){
                                printf(" %s", cmdHistory[q][x]);
                                q++;
                            }

                            if (bgHistory[p]){
                                printf(" &");
                            }

                            // Print return after iteration
                            printf("\n");
                            q = 1;
                        
                        }

                        p--;
                        x--;

                    }

                    // Revert variables for future reuse
                    p = 0;
                    q = 1;
                    x = 0;

                // End of history command
                }

                // Run cd command
                else if ((strcmp(args[0], "cd")) == 0){

                    if ((q = chdir(args[1]) != 0)){
                        printf("\nError navigating to specified path\n");

                    }

                // End of cd command
                }

                // Run pwd command
                else if ((strcmp(args[0], "pwd")) == 0){

                    // Reset variable to avoid past command interference
                    buff = NULL;

                    printf("\nCurrent directory: %s\n", getcwd(buff, NULL));

                // End of pwd command
                }

                // Run jobs command
                else if ((strcmp(args[0], "jobs")) == 0){

                    if (numJobs > 0){
                        // Header
                        printf("\nCurrent running jobs - [job#]. [job name] [PID]: ");

                        for(int j=0; j < numJobs; j++){
                            printf("\n%d. %s %d", (j+1), jobNames[j], jobPids[j]);

                        }
                    }

                    else {
                        printf("\nNo jobs running in present shell!");

                    }

                    printf("\n");

                // End of jobs command
                }

                // Run fg command (TODO)
                else if ((strcmp(args[0], "fg")) == 0){

                    // Header
                        //printf("Waiting for job %s...", args[1]);

                    if (args[1] != NULL){
                        q = atoi(args[1]);

                    }

                    // Wait for process to complete if valid
                    if ((q - 1) < numJobs && (q -1) > -1 && args[1] != NULL){
                        
                        waitpid(jobPids[q-1], &status, 0);

                        // Flags earlier function to remove and kill process from job list
                        childToFg = true;

                    }

                    else{
                        printf("\nInvalid job number - first argument should be job number printed via the 'jobs' command.\n");

                    }

                // End of fg command
                }

                // Store most recent successes
                cmdSuccess[0] = erroneous;

                builtInCmd = false;

            }
        }
    }

    return 0;
}

// --------------------------------------------------------------





