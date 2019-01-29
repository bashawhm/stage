#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <readline/readline.h>
#include <signal.h>

#include "parse.h"

#define MAX_BUFF_SIZE 512
#define HISTORY_SIZE 10

typedef enum BuiltinCommand {
    None,
    Exit,
    Cd,
    History,
    Call,
    Jobs,
    Kill,
    Help
} BuiltinCommand;

// BackgroundJob is a node in a linked list of jobs
typedef struct BackgroundJob {
    parseInfo *info;
    unsigned int pid;
    struct BackgroundJob *next;
    struct BackgroundJob *prev;
} BackgroundJob;

BackgroundJob *makeJob(parseInfo *info, unsigned int pid) {
    BackgroundJob *newJob = (BackgroundJob*)malloc(sizeof(BackgroundJob));
    newJob -> info = info;
    newJob -> pid = pid;
    newJob -> next = NULL;
    newJob -> prev = NULL;
    return newJob;
}

// Adds job to linked list
void addJob(BackgroundJob *jobList, BackgroundJob *newJob) {
    if (jobList -> next == NULL) {
        jobList -> next = newJob;
        newJob -> prev = jobList;
        return;
    }
    addJob(jobList -> next, newJob);
}

// Removed job fron linked list of jobs
BackgroundJob *removeJob(BackgroundJob *jobList, BackgroundJob *staleJob) {
    if (jobList != NULL) {
        BackgroundJob *job = jobList;
        do {
            if (job -> pid == staleJob -> pid) {
                if (job -> prev != NULL) {
                    //If there is more than one job
                    job -> prev -> next = job -> next;
                    if (job -> next != NULL) {
                        job -> next -> prev = job -> prev;
                    }
                    free_info(job -> info);
                    free(staleJob);
                    return jobList;
                } else {
                    if (job -> next != NULL) {
                        BackgroundJob *next = job -> next;
                        job -> next -> prev = NULL;
                        free_info(staleJob -> info);
                        free(staleJob);
                        return next;
                    }
                    free_info(staleJob -> info);
                    free(staleJob);
                    return NULL;
                }
            }
            job = job -> next;
        } while (job != NULL);
    }
    return jobList;
}

void printHelp(void) {
    printf("Stage built in commands\n");
    printf("exit           -  Exits the shell\n");
    printf("cd <filepath>  -  Changes directory into filepath\n");
    printf("history        -  Lists the last %d commands run\n", HISTORY_SIZE);
    printf("!<num>         -  Runs the command labeld num in the history output\n");
    printf("jobs           -  Shows a list of all background jobs currently running\n");
    printf("kill <num>     -  Sends SIGKILL to the process with PID num\n");
    printf("kill %%<num>    -  Sends SIGKILL to the process labeled num in jobs output\n"); //Yes, this does need one extra space because of %
    printf("help           -  Shows the help page\n");
    printf("\n");
}

void printHistory(char **history) {
    for (int i = 0; i < HISTORY_SIZE; ++i) {
        fprintf(stderr, "%d %s\n", i, history[i]);
    }
}

BuiltinCommand builtIn(char *cmd) {
    if (cmd == NULL) {
        return None;
    }
    if (strncmp(cmd, "exit", strlen("exit")) == 0) {
        return Exit;
    } else if (strncmp(cmd, "cd", strlen("cd")) == 0) {
        return Cd;
    } else if (strncmp(cmd, "history", strlen("history")) == 0) {
        return History;
    } else if (cmd[0] == '!') {
        return Call;
    } else if (strncmp(cmd, "jobs", strlen("jobs")) == 0) {
        return Jobs;
    } else if (strncmp(cmd, "help", strlen("help")) == 0) {
        return Help;
    } else if (strncmp(cmd, "kill", strlen("kill")) == 0) {
        return Kill;
    }
    return None;
}

char *buildPrompt(void) {
    char buff[MAX_BUFF_SIZE];
    getcwd(buff, MAX_BUFF_SIZE);
    // Handle coloring
    strncat(buff, "\x1b[36m:> \x1b[0m", MAX_BUFF_SIZE);
    char colorBuff[MAX_BUFF_SIZE] = "\x1b[32m";
    return strncat(colorBuff, buff, MAX_BUFF_SIZE);
}

int main(void) {
    char *cmdLine = NULL;
    parseInfo *info = NULL;
    char **history;
    BackgroundJob *jobs = NULL;
    history = (char**)malloc(HISTORY_SIZE * sizeof(char*));
    for (int i = 0; i < HISTORY_SIZE; ++i) {
        history[i] = strdup("\0");
    }

    printf("Run help for help\n");
    for(;;) {
        //Check the status of background jobs
        if (jobs != NULL) {
            BackgroundJob *job = jobs;
            do {
                int status;
                if (waitpid(job -> pid, &status, WNOHANG)) {
                    printf("Process %d has ended\n", status);
                    jobs = removeJob(jobs, job);
                }
                job = job -> next;
            } while (job != NULL);
        }

        //Parse command
        cmdLine = readline(buildPrompt());
        if (cmdLine == NULL) {
            printf("\n");
            for (int i = 0; i < HISTORY_SIZE; ++i) {
                free(history[i]);
            }
            exit(-1);
        }

reparse:
        if (cmdLine[0] != '!') {
            //Free last line
            free(history[HISTORY_SIZE-1]);
            //Move history up
            for (int i = HISTORY_SIZE-1; i > 0; --i) {
                history[i] = history[i-1];
            }
            //Append new line to history
            history[0] = strdup(cmdLine);
        }

        //Call Parser
        info = parse(cmdLine);
        if (info == NULL) {
            fprintf(stderr, "Failed to parse command\n");
            free(cmdLine);
            continue;
        }
        // print_info(info);

        BuiltinCommand cmd = builtIn((info -> CommArray[0].command));
        switch (cmd) {
        case Exit: {
            if (jobs != NULL) {
                printf("There are background processes running\n");
                free_info(info);
            } else {
                free_info(info);
                free(cmdLine);
                //Clears history
                for (int i = 0; i < HISTORY_SIZE; ++i) {
                    free(history[i]);
                }
                exit(1);
            }
            break;
        }
        case Cd: {
            if (info -> CommArray[0].VarList[1] != NULL) {
                if (chdir(info -> CommArray[0].VarList[1]) == -1) {
                    fprintf(stderr, "No such file or directory\n");
                }
            } else {
                // `cd` takes you to your home dir
                if (chdir(getenv("HOME")) == -1) {
                    fprintf(stderr, "No such file or directory\n"); 
                }
            }
            free_info(info);
            break;
        }
        case History: {
            printHistory(history);
            free_info(info);
            break;
        }
        case Call: {
            // Calls command in the history
            int num = atoi(cmdLine+1);
            free(cmdLine);
            if (num >= 0) {
                cmdLine = strdup(history[num]);
            } else {
                cmdLine = strdup(history[0]);
            }
            free_info(info);
            printf("%s\n", cmdLine);
            goto reparse;
        }
        case Jobs: {
            if (jobs == NULL) {
                printf("There are no background jobs running\n");
                free_info(info);
                break;
            }
            BackgroundJob *job = jobs;
            int i = 0;
            do {
                printf("[%d]\tpid: %d\t", i, job -> pid);
                for (int j = 0; j < job -> info -> CommArray[0].VarNum; ++j) {
                    printf(" %s", job -> info -> CommArray[0].VarList[j]);
                }
                printf("\n");
                ++i;
                job = job -> next;
            } while (job != NULL);
            free_info(info);
            break;
        }
        case Kill: {
            int num = 0;
            if (info -> CommArray[0].VarList[1][0] == '%') {
                num = atoi(info -> CommArray[0].VarList[1]+1); 
                //Kill process in the numbered list of jobs
                BackgroundJob *job = jobs;
                for (int i = 0; job != NULL; job = job -> next, ++i) {
                    if (i == num) {
                        kill(job -> pid, SIGKILL);
                    }
                }
            } else {
                num = atoi(info -> CommArray[0].VarList[1]);
                kill(num, SIGKILL);
            }
            free_info(info);
            break;
        }
        case Help: {
            printHelp();
            free_info(info);
            break;
        }
        case None: {
            if (info -> CommArray[0].command == NULL) {
                free_info(info);
                free(cmdLine);
                continue;
            }

            int childPID = fork();
            if (childPID == 0) {
                //Is child
                if (info -> boolInfile) {
                    FILE *ifd = fopen(info -> inFile, "r");
                    dup2(fileno(ifd), fileno(stdin));
                }
                if (info -> boolOutfile) {
                    FILE *ofd = fopen(info -> outFile, "w");
                    dup2(fileno(ofd), fileno(stdout));
                }
                exit(execvp(info -> CommArray[0].command, info -> CommArray[0].VarList));
            }
            //Is parent
            if (info -> boolBackground) {
                if (jobs == NULL) {
                    jobs = makeJob(info, childPID);
                } else {
                    addJob(jobs, makeJob(info, childPID));
                }
            } else {
                int code;
                while(wait(&code) != childPID);
                free_info(info);
                break;
            }
        }
        default: {}
        }

        free(cmdLine);
    }

    return 0;
}
