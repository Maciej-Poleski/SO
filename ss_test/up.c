#define _POSIX_SOURCE      1    /* tell headers to include POSIX stuff */

#include <stdio.h>
#include <stdlib.h>
#include <sem.h>

int main(int argc,char**argv)
{
    if(argc!=2)
    {
        fprintf(stderr,"Potrzeba dokładnie jednego argumentu: %s [id]\n",argv[0]);
        return 1;
    }
    int a=atoi(argv[1]);
    printf("%d\n",up(a));
}
