#define _POSIX_SOURCE 1

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>

#include "config.h"
#include "commands.h"
#include "cparse.h"

static char prompt[10] = PROMPT;

/*
 * main
 */

char input_buf[BUFF_SIZE];
static struct stat buf;
int result;

int isInteractive(void)
{
    return S_ISCHR(buf.st_mode);
}

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
                if(isInteractive())
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

/**
 * Próbuje wykonać polecenie shell'a.
 *
 * @return 1 - podane polecenie istotnie jest poleceniem shell'a
 *         0 - podane polecenie nie jest poleceniem shell'a
 */
static int tryShell(command_s* cmd)
{
    command_pair* cp;
    for(cp = dispatch_table; cp->name; ++cp)
    {
        if(strcmp(cmd->argv[0], cp->name) == 0)
        {
            int output=1;
            if(cmd->out_file_name)
            {
                int flags = O_WRONLY | O_CREAT;
                if(cmd->append_mode)
                    flags |= O_APPEND;
                else
                    flags |= O_TRUNC;
                if((output=open(cmd->out_file_name, flags, 0777)) <0)
                {
                    perror("Unable to open output file");
                    result=-1;
                    return 1;
                }
            }
            result = cp->fun(cmd->argv,output,cmd->append_mode);
            /* Jeżeli output != 1 to został już zamknięty */
            return 1;
        }
    }
    return 0;
}

/**
 * Tworzy nowy proces o podanym wejściu i wyjściu (mogą być pipe'y).
 *
 * @param cmd polecenie które zostanie wykonane w procesie potomnym
 * @param input deskryptor pliku wejściowego
 * @param output deskryptor pliku wyjściowego
 * @return PID procesu potomnego lub -1 gdy nie udało się
 */
static pid_t runProcess(command_s* cmd, int input, int output,int fg)
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
        /* Jeżeli proces przygotowania procesu potomnego nie powiedzie się
         * jako kod błędu zostanie zwrócone -1.
         */
        if(fg)
        {
            struct sigaction sa;
            sa.sa_handler=SIG_DFL;
            sigemptyset(&(sa.sa_mask));
            sa.sa_flags=SA_RESTART;
            sigaction(SIGINT,&sa,NULL);
        }
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
            close(input); /* Ten deskryptor nie jest już potrzebny */
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
            close(output); /* Ten deskryptor nie jest już potrzebny */
        }
        /* Pliki mają priorytet nad potokami */
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

result_node *result_list=NULL;

int main(int argc, char** argv)
{
    pid_list pid_list;
    int count_of_children_to_wait=0;
    if(fstat(0, &buf))
    {
        perror("Unable to detect input type");
        return 2;
    }
    pid_list=make_pid_list();
    {
        struct sigaction sa;
        sa.sa_handler=SIG_IGN;
        sigemptyset(&(sa.sa_mask));
        sa.sa_flags=SA_RESTART;
        sigaction(SIGINT,&sa,NULL);
    }
    {
        struct sigaction sa;
        sa.sa_handler=SIG_IGN;
        sigemptyset(&(sa.sa_mask));
        sa.sa_flags=SA_RESTART | SA_NOCLDSTOP;
        sigaction(SIGCHLD,&sa,NULL);
    }
    for(;;)
    {
        command_s* cmd;
        const char* command;
        pid_t np;
        int nextProcInput = 0;
        int nextProcOutput = 1;
        int pipes[2];
        int run_in_foreground=1;
        {
            pid_iterator i;
            for(i = first_pid(&pid_list); *i!=NULL; )
            {
                int status=0;
                int do_next=1;
                if(waitpid((*i)->pid, &status, WNOHANG)==0)
                {
                    i = next_pid(i);
                    continue;
                }
                assert((*i)->wait==0);
                if(isInteractive())
                {
                    if(WIFEXITED(status) || WIFSIGNALED(status))
                    {
                        result_node *n=malloc(sizeof(result_node));
                        n->next=result_list;
                        n->pid=(*i)->pid;
                        if(WIFEXITED(status))
                        {
                            n->result=WEXITSTATUS(status);
                            n->signal=0;
                        }
                        else
                        {
                            assert(WIFSIGNALED(status));
                            n->result=0;
                            n->signal=WTERMSIG(status);
                        }
                        result_list=n;
                    }
                }
                if(WIFEXITED(status) || WIFSIGNALED(status))
                {
                    do_next=0;
                    remove_pid(i);
                }
                if(do_next)
                    i = next_pid(i);
            }
        }
        if(isInteractive())
        {
            {
                result_node *i=result_list;
                while(i!=NULL)
                {
                    result_node *next=i->next;
                    printf("Done: %d exit: %d signal: %d\n",i->pid,i->result,i->signal);
                    free(i);
                    i=next;
                }
                result_list=NULL;
            }
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
        if(in_background((char*)command))
            run_in_foreground=0;
        cmd = split_commands((char*)command);
        /* Jeżeli podano tylko jedno polecenie to być może jest to polecenie
         * shell'a */
        if(cmd[1].argv == NULL && tryShell(cmd))
            continue;

        /* Jeżeli nie to tworzę i ustawiam odpowiednie procesy potomne */

        while((cmd + 1)->argv)
        {
            pipe(pipes);
            np=runProcess(cmd, nextProcInput, pipes[1],run_in_foreground);
            append_pid(&pid_list,np,run_in_foreground);
            if(run_in_foreground)
                ++count_of_children_to_wait;
            if(np < 0)
            {
                break; /* fail */
            }
            if(nextProcInput != 0)
                close(nextProcInput);
            close(pipes[1]);
            nextProcInput = pipes[0];
            cmd = cmd + 1;
        }
        if(!is_empty(&pid_list) && np < 0)
            continue; /* give up */
        {
            np=runProcess(cmd, nextProcInput, nextProcOutput,run_in_foreground);
            append_pid(&pid_list,np,run_in_foreground);
            if(run_in_foreground)
                ++count_of_children_to_wait;
            if(nextProcInput != 0)
                close(nextProcInput);
        }
        /* Powinien powstać przynajmniej jeden proces */
        if(np < 0)
            continue; /* give up */
        while(count_of_children_to_wait>0)
        {
            pid_iterator i;
            int status;
            pid_t rpid=wait(&status);
            for(i = first_pid(&pid_list); *i!=NULL; i = next_pid(i))
            {
                if((*i)->pid==rpid)
                    break;
            }
            assert(*i!=NULL);
            if(isInteractive())
            {
                if((WIFEXITED(status) || WIFSIGNALED(status)) && (*i)->wait==0)
                {
                    result_node *n=malloc(sizeof(result_node));
                    n->next=result_list;
                    n->pid=(*i)->pid;
                    if(WIFEXITED(status))
                    {
                        n->result=WEXITSTATUS(status);
                        n->signal=0;
                    }
                    else
                    {
                        assert(WIFSIGNALED(status));
                        n->result=0;
                        n->signal=WTERMSIG(status);
                    }
                    result_list=n;
                }
            }
            if(WIFEXITED(status) || WIFSIGNALED(status))
            {
                if((*i)->wait)
                    --count_of_children_to_wait;
                remove_pid(i);
            }
        }
    }
    return 0;
}

