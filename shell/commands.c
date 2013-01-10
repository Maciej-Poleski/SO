#define _POSIX_SOURCE 1

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <assert.h>

#include "commands.h"

int echo(char*[],int,int);
int exit_(char**,int,int);
int cd_impl(char**,int,int);
int kill_impl(char**,int,int);
int lenv_impl(char**,int,int);

command_pair dispatch_table[] =
{
    {"echo", &echo},
    {"exit", &exit_},
    {"cd", &cd_impl},
    {"kill",&kill_impl},
    {"lenv",&lenv_impl},
    {NULL, NULL}
};

int echo(char** argv,int output,int append)
{
    FILE *out;
    int i = 1;
    fflush(stdout);
    out=fdopen(output,append?"a":"w");
    if(argv[i]) fprintf(out,"%s", argv[i++]);
    while(argv[i])
        fprintf(out," %s", argv[i++]);
    fprintf(out,"\n");
    if(output!=1)
        fclose(out);
    else
        fflush(stdout);
    return 0;
}

int exit_(char** argv,int output,int append)
{
    (void)argv;
    (void)append;
    if(output!=1)
        close(output);
    exit(0);
}

int cd_impl(char** argv,int output,int append)
{
    int r;
    (void)append;
    if(output!=1)
        close(output);
    r=chdir(argv[1]);
    if(r)
    {
        perror("Unable to change directory");
    }
    return r;
}

int kill_impl(char** argv,int output,int append)
{
    int sig=SIGTERM;
    pid_t pid;
    int r;
    (void)append;
    if(output!=1)
        close(output);
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

int lenv_impl(char** argv,int output,int append)
{
    FILE *out;
    char **i;
    (void)argv;
    fflush(stdout);
    out=fdopen(output,append?"a":"w");
    for(i=environ; *i; ++i)
        fprintf(out,"%s\n",*i);
    if(output!=1)
        fclose(out);
    else
        fflush(stdout);
    return 0;
}

/******* MISC */

void append_pid(pid_list *list,pid_t pid,int wait)
{
    struct pid_list_node_s *s=malloc(sizeof(struct pid_list_node_s));
    assert(list!=NULL);
    s->next=list->head;
    s->pid=pid;
    s->wait=wait;
    list->head=s;
}

pid_list make_pid_list(void)
{
    pid_list result;
    result.head=NULL;
    return result;
}

pid_iterator first_pid(pid_list* list)
{
    assert(list!=NULL);
    return &(list->head);
}

pid_iterator next_pid(pid_iterator pid)
{
    assert(pid!=NULL);
    return &((*pid)->next);
}

int is_empty(pid_list* list)
{
    assert(list!=NULL);
    return list->head==NULL;
}

void remove_pid(pid_iterator pid)
{
    struct pid_list_node_s *nn=*pid;
    *pid=nn->next;
    free(nn);
}

void dump_list(pid_list* list)
{
    pid_iterator i;
    for(i = first_pid(list); *i!=NULL; i = next_pid(i))
    {
        printf("lista: %d %d\n",(*i)->pid,(*i)->wait);
    }
    if(is_empty(list))
    {
        printf("lista jest pusta\n");
    }
}
