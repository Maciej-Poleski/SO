#ifndef SEM_H
#define SEM_H

#include <minix/ansi.h>
#include <minix/com.h>
#include <minix/ipc.h>

static int get_sem(int min,int max,int init)
{
    message m;
    m.m_type=1; // get_sem
    m.m7_i1=min;
    m.m7_i2=max;
    m.m7_i3=init;
    sendrec(SS_PROC_NR,&m);
    return m.m7_i4;
}

static int up(int sem_nb)
{
    message m;
    m.m_type=2; // up
    m.m7_i1=sem_nb;
    sendrec(SS_PROC_NR,&m);
    return m.m7_i2;
}

static int nb_up(int sem_nb)
{
    message m;
    m.m_type=3; // nb_up
    m.m7_i1=sem_nb;
    sendrec(SS_PROC_NR,&m);
    return m.m7_i2;
}

static int down(int sem_nb)
{
    message m;
    m.m_type=4; // down
    m.m7_i1=sem_nb;
    sendrec(SS_PROC_NR,&m);
    return m.m7_i2;
}

static int nb_down(int sem_nb)
{
    message m;
    m.m_type=5; // nb_down
    m.m7_i1=sem_nb;
    sendrec(SS_PROC_NR,&m);
    return m.m7_i2;
}

static int put_sem(int sem_nb)
{
    message m;
    m.m_type=6; // put_sem
    m.m7_i1=sem_nb;
    sendrec(SS_PROC_NR,&m);
    return m.m7_i2;
}

#endif // SEM_H