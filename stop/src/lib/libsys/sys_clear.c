#include "syslib.h"

/*===========================================================================*
 *                                sys_clear			     	     *
 *===========================================================================*/
PUBLIC int sys_clear(proc_ep)
endpoint_t proc_ep;			/* which process has exited */
{
    /* A process has exited. PM tells the kernel.
     */
    message m;

    m.PR_ENDPT = proc_ep;
    return(_kernel_call(SYS_CLEAR, &m));
}

int sys_stop_ex( endpoint_t proc_ep)
{
    message m;

    m.PR_ENDPT = proc_ep;
    return(_kernel_call(SYS_STOP_EX, &m));
}

int sys_cont_ex( endpoint_t proc_ep)
{
    message m;

    m.PR_ENDPT = proc_ep;
    return(_kernel_call(SYS_CONT_EX, &m));
}
