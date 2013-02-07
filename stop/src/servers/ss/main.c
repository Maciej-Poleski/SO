/* Data Store Server.
 * This service implements a little publish/subscribe data store that is
 * crucial for the system's fault tolerance. Components that require state
 * can store it here, for later retrieval, e.g., after a crash and subsequent
 * restart by the reincarnation server.
 *
 * Created:
 *   Oct 19, 2005	by Jorrit N. Herder
 */




#include "inc.h"	/* include master header file */
#include <minix/endpoint.h>
#include <minix/sef.h>

#include <assert.h>

/* Allocate space for the global variables. */
PRIVATE endpoint_t who_e;	/* caller's proc number */
PRIVATE int callnr;		/* system call number */

/* Declare some local functions. */
FORWARD _PROTOTYPE(void get_work, (message *m_ptr)			);
FORWARD _PROTOTYPE(void reply, (endpoint_t whom, message *m_ptr)	);

/* SEF functions and variables. */
FORWARD _PROTOTYPE( void sef_local_startup, (void) );

int nextSemafor;

typedef struct proc_info_s
{
    endpoint_t who_e;
    struct proc_info_s *next;
} proc_info;

typedef struct semafor_property_s
{
    int id;
    int min;
    int max;
    int current;
    struct semafor_property_s *next;
    proc_info *up_wait;
    proc_info *down_wait;
} semafor_property;

semafor_property *semafors;

PRIVATE semafor_property * find_semafor(int sem_nb)
{
    for(semafor_property *i=semafors; i; i=i->next)
    {
        if(i->id==sem_nb)
            return i;
    }
    return NULL;
}

PRIVATE semafor_property ** find_semafor_pp(int sem_nb)
{
    for(semafor_property **i=&semafors; *i; i=&((*i)->next))
    {
        if((*i)->id==sem_nb)
            return i;
    }
    return NULL;
}

PRIVATE int reply_status(const proc_info *proc,int status)
{
    message m;
    m.m7_i2=status;
    int s = send(proc->who_e, &m);    /* send the message */
    return s==OK;
}

PRIVATE int unblock_one_up(semafor_property *sem)
{
    int s;
    do
    {
        s=reply_status(sem->up_wait,0); // odblokuj up
        proc_info *proc=sem->up_wait;
        sem->up_wait=sem->up_wait->next;
        free(proc);
    } while(!s && sem->up_wait);
    return s;
}

PRIVATE int unblock_one_down(semafor_property *sem)
{
    int s;
    do
    {
        s=reply_status(sem->down_wait,0); // odblokuj down
        proc_info *proc=sem->down_wait;
        sem->down_wait=sem->down_wait->next;
        free(proc);
    } while(!s && sem->down_wait);
    return s;
}

/*===========================================================================*
 *                          sef_cb_init_fresh                                *
 *===========================================================================*/
PUBLIC int sef_cb_init_fresh(int UNUSED(type), sef_init_info_t *info)
{
    nextSemafor=1;
    semafors=NULL;
    return(OK);
}

/*===========================================================================*
 *				main                                         *
 *===========================================================================*/
PUBLIC int main(int argc, char **argv)
{
    /* This is the main routine of this service. The main loop consists of
     * three major activities: getting new work, processing the work, and
     * sending the reply. The loop never terminates, unless a panic occurs.
     */
    message m;
    int result;

    /* SEF local startup. */
    env_setargs(argc, argv);
    sef_local_startup();

    /* Main loop - get work and do it, forever. */
    while (TRUE) {
        semafor_property *sem;
        semafor_property **sem_pp;

        /* Wait for incoming message, sets 'callnr' and 'who'. */
        get_work(&m);

        if (is_notify(callnr)) {
            printf("SS: warning, got illegal notify from: %d\n", m.m_source);
            result = EINVAL;
            goto send_reply;
        }
        // result -> message.m_type
        result=OK;
        switch (callnr) {
        case 1:
            // get_sem
            if(m.m7_i1<=m.m7_i3 && m.m7_i3<=m.m7_i2)
            {
                semafor_property *newSemafor=malloc(sizeof(semafor_property));
                newSemafor->next=semafors;
                semafors=newSemafor;
                newSemafor->id=nextSemafor++;
                newSemafor->min=m.m7_i1;
                newSemafor->max=m.m7_i2;
                newSemafor->current=m.m7_i3;
                newSemafor->up_wait=NULL;
                newSemafor->down_wait=NULL;
                m.m7_i4=newSemafor->id;
            }
            else
            {
                m.m7_i4=-1;
            }
            break;

        case 2:
            // up
            sem=find_semafor(m.m7_i1);
            if(sem!=NULL && sem->min!=sem->max)
            {
                if(sem->down_wait!=NULL)
                {
                    assert(sem->current==sem->min);
                    if(!unblock_one_down(sem))
                        ++sem->current;
                    m.m7_i2=0; // OK
                }
                else if(sem->current<sem->max)
                {
                    ++sem->current;
                    m.m7_i2=0; // OK
                }
                else
                {
                    assert(sem->current==sem->max);
                    proc_info *newProcInfo=malloc(sizeof(proc_info));
                    newProcInfo->next=sem->up_wait;
                    sem->up_wait=newProcInfo;
                    newProcInfo->who_e=who_e;
                    result=EDONTREPLY; // blokuje proces
                }
            }
            else
            {
                m.m7_i2=-1;
            }
            break;

        case 3:
            // nb_up
            sem=find_semafor(m.m7_i1);
            if(sem!=NULL)
            {
                if(sem->down_wait!=NULL)
                {
                    assert(sem->min!=sem->max);
                    assert(sem->current==sem->min);
                    if(!unblock_one_down(sem))
                        ++sem->current;
                    m.m7_i2=0; // OK
                }
                else if(sem->current<sem->max)
                {
                    ++sem->current;
                    m.m7_i2=0; // OK
                }
                else
                {
                    assert(sem->current==sem->max);
                    // tym razem nie wolno nam zablokować prcesu - niepowodzenie
                    m.m7_i2=-1;
                }
            }
            else
            {
                m.m7_i2=-1;
            }
            break;

        case 4:
            // down
            sem=find_semafor(m.m7_i1);
            if(sem!=NULL && sem->min!=sem->max)
            {
                if(sem->up_wait!=NULL)
                {
                    assert(sem->current==sem->max);
                    if(!unblock_one_up(sem))
                        --sem->current;
                    m.m7_i2=0; // OK
                }
                else if(sem->current>sem->min)
                {
                    --sem->current;
                    m.m7_i2=0; // OK
                }
                else
                {
                    assert(sem->current==sem->min);
                    proc_info *newProcInfo=malloc(sizeof(proc_info));
                    newProcInfo->next=sem->down_wait;
                    sem->down_wait=newProcInfo;
                    newProcInfo->who_e=who_e;
                    result=EDONTREPLY; // blokuje proces
                }
            }
            else
            {
                m.m7_i2=-1;
            }
            break;

        case 5:
            // nb_down
            sem=find_semafor(m.m7_i1);
            if(sem!=NULL)
            {
                if(sem->up_wait!=NULL)
                {
                    assert(sem->current==sem->max);
                    assert(sem->min!=sem->max);
                    if(!unblock_one_up(sem))
                        --sem->current;
                    m.m7_i2=0; // OK
                }
                else if(sem->current>sem->min)
                {
                    --sem->current;
                    m.m7_i2=0; // OK
                }
                else
                {
                    assert(sem->current==sem->min);
                    // tym razem nie wolno nam zablokować prcesu - niepowodzenie
                    m.m7_i2=-1;
                }
            }
            else
            {
                m.m7_i2=-1;
            }
            break;

        case 6:
            //put_sem
            sem_pp=find_semafor_pp(m.m7_i1);
            if(sem_pp!=NULL)
            {
                sem=*sem_pp;
                while(sem->up_wait!=NULL)
                {
                    assert(sem->current==sem->max);
                    reply_status(sem->up_wait,-1); // odblokuj up
                    proc_info *proc=sem->up_wait;
                    sem->up_wait=sem->up_wait->next;
                    free(proc);
                }
                while(sem->down_wait!=NULL)
                {
                    assert(sem->current==sem->min);
                    reply_status(sem->down_wait,-1); // odblokuj down
                    proc_info *proc=sem->down_wait;
                    sem->down_wait=sem->down_wait->next;
                    free(proc);
                }
                *sem_pp=sem->next;
                free(sem);
                m.m7_i2=0;
            }
            else
            {
                m.m7_i2=-1;
            }
            break;

        default:
            printf("SS: warning, got illegal request from %d\n", m.m_source);
            result = EINVAL;
        }

send_reply:
        /* Finally send reply message, unless disabled. */
        if (result != EDONTREPLY) {
            m.m_type = result;  		/* build reply message */
            reply(who_e, &m);		/* send it away */
        }
    }
    return(OK);				/* shouldn't come here */
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
PRIVATE void sef_local_startup()
{
    /* Register init callbacks. */
    sef_setcb_init_fresh(sef_cb_init_fresh);
    sef_setcb_init_restart(sef_cb_init_fail);

    /* No live update support for now. */

    /* Let SEF perform startup. */
    sef_startup();
}

/*===========================================================================*
 *				get_work                                     *
 *===========================================================================*/
PRIVATE void get_work(
    message *m_ptr			/* message buffer */
)
{
    int status = sef_receive(ANY, m_ptr);   /* blocks until message arrives */
    if (OK != status)
        panic("failed to receive message!: %d", status);
    who_e = m_ptr->m_source;        /* message arrived! set sender */
    callnr = m_ptr->m_type;       /* set function call number */
}

/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
PRIVATE void reply(
    endpoint_t who_e,			/* destination */
    message *m_ptr			/* message buffer */
)
{
    int s = send(who_e, m_ptr);    /* send the message */
    if (OK != s)
        printf("SS: unable to send reply to %d: %d\n", who_e, s);
}

