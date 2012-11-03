#define _POSIX_SOURCE 1

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "config.h"
#include "commands.h"
#include "cparse.h"

int main(int, char**);

char prompt[10] = PROMPT;

/*
 * main
 */

char input_buf[BUFF_SIZE];
static struct stat buf;
int result;

static const char* getNextLine(void)
{
    static int contentBegin = 0;
    static int contentEnd = 0;
    int iterator = contentBegin;
    assert(contentBegin <= contentEnd);
    for(;;)
    {
        while(iterator < contentEnd && input_buf[iterator] != '\n')
            ++iterator;
        if(iterator == contentEnd)
        {
            int i;
            ssize_t length;
            if(contentBegin == 0 && contentEnd == BUFF_SIZE)
            {
                perror("Input line is too long");
                exit(3);
            }
            if(contentBegin != 0)
            {
                for(i = 0; i < contentEnd - contentBegin; ++i)
                    input_buf[i] = input_buf[contentBegin + i];
                contentEnd -= contentBegin;
                iterator -= contentBegin;
                contentBegin = 0;
            }
            length = read(0, input_buf + contentEnd, BUFF_SIZE - contentEnd);
            if(length == 0)
            {
                if(S_ISCHR(buf.st_mode))
                    write(1, "\n", 1);
                /* assuming EOF */
                return NULL;
            }
            else if(length < 0)
            {
                perror("No input provided");
                exit(2);
            }
            else
            {
                contentEnd += length;
                continue;
            }
        }
        else
        {
            int oldBegin;
            assert(input_buf[iterator] == '\n');
            input_buf[iterator] = 0;
            oldBegin = contentBegin;
            contentBegin = iterator + 1;
            return &input_buf[oldBegin];
        }
    }
    assert(0);
}

static int tryShell(command_s* cmd)
{
    command_pair* cp;
    for(cp = dispatch_table; cp->name; ++cp)
    {
        if(strcmp(cmd->argv[0], cp->name) == 0)
        {
            result = cp->fun(cmd->argv);
            return 1;
        }
    }
    return 0;
}

static pid_t runProcess(command_s* cmd, int input, int output)
{
    pid_t childPid = fork();
    if(childPid < 0)
    {
        perror("Unable to create new process");
        result = -1;
        return -1;
    }
    else if(childPid == 0)
    {
        if(input != 0)
        {
            if(close(0))
            {
                perror("Unable to close STDIN");
                exit(-1);
            }
            if(fcntl(input, F_DUPFD) != 0)
            {
                perror("Unable to duplicate input file descriptor");
                exit(-1);
            }
        }
        if(output != 1)
        {
            if(close(1))
            {
                perror("Unable to close STDOUT");
                exit(-1);
            }
            if(fcntl(output, F_DUPFD) != 1)
            {
                perror("Unable to duplicate output file descriptor");
                exit(-1);
            }
        }
        if(cmd->in_file_name)
        {
            if(close(0))
            {
                perror("Unable to close STDIN");
                exit(-1);
            }
            if(open(cmd->in_file_name, O_RDONLY) != 0)
            {
                perror("Unable to open input file as STDIN");
                exit(-1);
            }
        }
        if(cmd->out_file_name)
        {
            int flags = O_WRONLY | O_CREAT;
            if(close(1))
            {
                perror("Unable to close STDOUT");
                exit(-1);
            }
            if(cmd->append_mode)
                flags |= O_APPEND;
            else
                flags |= O_TRUNC;
            if(open(cmd->out_file_name, flags, 0777) != 1)
            {
                perror("Unable to open output file as STDOUT");
                exit(-1);
            }
        }
        execvp(cmd->argv[0], cmd->argv);
        perror("Unable to execute");
        exit(-1);
    }
    else
    {
        return childPid;
    }
}

typedef struct pids_t
{
    pid_t pid;
    struct pids_t* next;
} Pids;

int main(int argc, char** argv)
{
    if(fstat(0, &buf))
    {
        perror("Unable to detect input type");
        return 2;
    }
    for(;;)
    {
        ssize_t length;
        command_s* cmd;
        Pids* pids = NULL;
        const char* command;
        int nextProcInput = 0;
        int nextProcOutput = 1;
        int pipes[2];
        if(S_ISCHR(buf.st_mode))
        {
            printf("%d", result);
            fflush(stdout);
            if(write(1, prompt, 10) < 0)
            {
                perror("Console is broken");
                return 1;
            }
        }
        command = getNextLine();
        if(command == NULL)
        {
            /* eof */
            return 0;
        }
        else if(*command == 0)
        {
            /* empty line */
            continue;
        }
        cmd = split_commands((char*)command);
        if(cmd[1].argv == NULL && tryShell(cmd))
            continue;

        while((cmd + 1)->argv)
        {
            pipe(pipes);
            Pids* oldPids = pids;
            pids = malloc(sizeof(Pids));
            pids->next = oldPids;
            pids->pid = runProcess(cmd, nextProcInput, pipes[1]);
            if(pids->pid < 0)
            {
                break;
            }
            if(nextProcInput != 0)
                close(nextProcInput);
            close(pipes[1]);
            nextProcInput = pipes[0];
            cmd = cmd + 1;
        }
        if(pids && pids->pid < 0)
            continue;
        {
            Pids* oldPids = pids;
            pids = malloc(sizeof(Pids));
            pids->next = oldPids;
            pids->pid = runProcess(cmd, nextProcInput, nextProcOutput);
            if(nextProcInput != 0)
                close(nextProcInput);
        }
        if(pids->pid < 0)
            continue;
        for(;;)
        {
            Pids* i;
            for(i = pids; i; i = i->next)
            {
                waitpid(i->pid, NULL, 0);
            }
        }
    }
    return 0;
}

