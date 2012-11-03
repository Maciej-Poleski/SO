#define _POSIX_SOURCE 1

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#include "commands.h"

int echo(char*[]);
int exit_(char**);
int cd_impl(char**);
int kill_impl(char**);
int lenv_impl(char**);

command_pair dispatch_table[] =
{
    {"echo", &echo},
    {"exit", &exit_},
    {"cd", &cd_impl},
    {"kill",&kill_impl},
    {"lenv",&lenv_impl},
    {NULL, NULL}
};

int echo(char** argv)
{
    int i = 1;
    if(argv[i]) printf("%s", argv[i++]);
    while(argv[i])
        printf(" %s", argv[i++]);

    fflush(stdout);
    return 0;
}

int exit_(char** argv)
{
    exit(0);
}

int cd_impl(char** argv)
{
    int r=chdir(argv[1]);
    if(r)
    {
        perror("Unable to change directory");
    }
    return r;
}

int kill_impl(char** argv)
{
    int sig=SIGTERM;
    pid_t pid;
    int r;
    if(argv[1][0]=='-')
    {
        sig=strtol(&argv[1][1],NULL,0);
        pid=strtol(argv[2],NULL,0);
    }
    else
    {
        pid=strtol(argv[1],NULL,0);
    }
    r=kill(pid,sig);
    if(r)
    {
        perror("Unable to kill");
    }
    return r;
}
extern char **environ;

int lenv_impl(char** argv)
{
    char **i;
    for(i=environ;*i;++i)
        printf("%s\n",*i);
    return 0;
}