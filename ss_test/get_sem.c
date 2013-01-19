#define _POSIX_SOURCE      1    /* tell headers to include POSIX stuff */

#include <stdio.h>
#include <stdlib.h>
#include <sem.h>

int main(int argc,char**argv)
{
    if(argc!=4)
    {
        fprintf(stderr,"Potrzeba dokładnie trzech argumentów: %s [min] [max] [init]\n",argv[0]);
        return 1;
    }
    int min=atoi(argv[1]);
    int max=atoi(argv[2]);
    int init=atoi(argv[3]);
    printf("%d\n",get_sem(min,max,init));
}
