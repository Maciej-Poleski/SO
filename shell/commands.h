#include <signal.h>
#include <sys/types.h>

typedef struct {
	char* name;
	int (*fun)(char**,int,int);
} command_pair;

extern command_pair dispatch_table[];

extern int isInteractive(void);

struct pid_list_node_s
{
    struct pid_list_node_s *next;
    pid_t pid;
    int wait; /* czy chcemy czekać na ten proces */
};

typedef struct pid_list_node_s** pid_iterator;

typedef struct
{
    struct pid_list_node_s *head;
} pid_list;

extern void append_pid(pid_list *list,pid_t pid,int wait);
extern pid_list make_pid_list(void);
extern pid_iterator first_pid(pid_list *list);
extern pid_iterator next_pid(pid_iterator pid);
extern int is_empty(pid_list *list);
extern void remove_pid(pid_iterator pid);
extern void dump_list(pid_list *list);

typedef struct result_node_s
{
    pid_t pid;
    int result;
    int signal;
    struct result_node_s *next;
} result_node;