#pragma once

#include "util/list.h"

#include "proc/sched.h"
#include "proc/context.h"

typedef context_func_t kthread_func_t;

/* thread states */
typedef enum
{
        KT_NO_STATE,           /* illegal state */
        KT_RUN,                /* currently running, or on runq */
        KT_SLEEP,              /* blocked for an indefinite amount of time */
        KT_SLEEP_CANCELLABLE,  /* blocked, but this sleep can be cancelled */
        KT_EXITED              /* has exited, waiting to be joined */
} kthread_state_t;

struct proc;
typedef struct kthread {
        context_t       kt_ctx;         /* this thread's context */
        char           *kt_kstack;      /* the kernel stack */
        void           *kt_retval;      /* this thread's return value */
        int             kt_errno;       /* error no. of most recent syscall */
        struct proc    *kt_proc;        /* the thread's process */

        int             kt_cancelled;   /* 1 if this thread has been cancelled */
        ktqueue_t      *kt_wchan;       /* The queue that this thread is blocked on */
        kthread_state_t kt_state;       /* this thread's state */

        /*
         * This is the thread's link on a queue. Every thread must
         * either be on a queue, or running, or else the thread will be lost
         * forever.
         */
        list_link_t     kt_qlink;       /* link on ktqueue */
        list_link_t     kt_plink;       /* link on proc thread list, p_threads */
#ifdef __MTP__
        int             kt_detached;    /* if the thread has been detached */
        ktqueue_t       kt_joinq;       /* thread waiting to join with this thread */
#endif
} kthread_t;

void kthread_init(void);

/**
 * Allocates and initializes a kernel thread.
 *
 * @param p the process in which the thread will run
 * @param func the function that will be called when the newly created
 * thread starts executing
 * @param arg1 the first argument to func
 * @param arg2 the second argument to func
 * @return the newly created thread
 */
kthread_t *kthread_create(struct proc *p, kthread_func_t func, long arg1, void *arg2);

/**
 * Free resources associated with a thread.
 *
 * @param t the thread to free
 */
void kthread_destroy(kthread_t *t);

/**
 * Cancel a thread.
 *
 * @param kthr the thread to be cancelled
 * @param retval the return value for the thread
 */
void kthread_cancel(kthread_t *kthr, void *retval);

/**
 * Exits the current thread.
 *
 * @param retval the return value for the thread
 */
void kthread_exit(void *retval);

/**
 * Allocates a new thread that is a copy of a specified thread.
 *
 * @param thr the thread to clone
 * @return a cloned version of thr
 */
kthread_t *kthread_clone(kthread_t *thr);

#ifdef __MTP__
/**
 * Shuts down the reaper daemon.
 */
void kthread_reapd_shutdown(void);

/**
 * Put a thread in the detached state.
 *
 * @param kthr the thread to put in the detached state
 * @return 0 on sucess and <0 on error
 */
int kthread_detach(kthread_t *kthr);

/**
 * Wait for the termination of another thread.
 *
 * @param kthr the thread to wait for
 * @param retval if retval is not NULL, the return value for kthr is
 * stored in the location pointed to by retval
 * @return 0 on sucess and <0 on error
 */
int kthread_join(kthread_t *kthr, void **retval);
#endif
