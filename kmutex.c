/* Author: Suki Sahota */
#include "globals.h"
#include "errno.h"

#include "util/debug.h"

#include "proc/kthread.h"
#include "proc/kmutex.h"

extern void ktqueue_enqueue(ktqueue_t *q, kthread_t *thr);
extern kthread_t *ktqueue_dequeue(ktqueue_t *q);

/*
 * IMPORTANT: Mutexes can _NEVER_ be locked or unlocked from an
 * interrupt context. Mutexes are _ONLY_ lock or unlocked from a
 * thread context.
 */

void
kmutex_init(kmutex_t *mtx)
{
  //Initialize mutex list
  list_init(&mtx->km_waitq.tq_list);
  mtx->km_waitq.tq_size = 0;

  //No thread should have mutex at initialization
  mtx->km_holder = NULL;
}

/*
 * This should block the current thread (by sleeping on the mutex's
 * wait queue) if the mutex is already taken.
 *
 * No thread should ever try to lock a mutex it already has locked.
 */
void
kmutex_lock(kmutex_t *mtx)
{
  /* Precondition KASSERT statements begin */
  KASSERT(curthr && (curthr != mtx->km_holder)); /* curthr must be valid and it must not be holding the mutex (mtx) already */
  /* Precondition KASSERT statements end */

  if (mtx->km_holder == NULL) {
    //If mutex is free, claim it
    mtx->km_holder = curthr;
  } else {
    //Otherwise, current thread should sleep in mutex queue
    curthr->kt_state = KT_SLEEP;
    ktqueue_enqueue(&mtx->km_waitq, curthr);
    sched_switch();
  }

  //In either case, curthr should be mutex holder at function resolution
  KASSERT(curthr && (curthr == mtx->km_holder));
}

/*
 * This should do the same as kmutex_lock, but use a cancellable sleep
 * instead. Also, if you are cancelled while holding mtx, you should unlock mtx.
 */
int
kmutex_lock_cancellable(kmutex_t *mtx)
{
  /* Precondition KASSERT statements begin */
  KASSERT(curthr && (curthr != mtx->km_holder)); /* curthr must be valid and it must not be holding the mutex (mtx) already */
  /* Precondition KASSERT statemends end */

  if (mtx->km_holder == NULL) {
    //If mutex is free, claim it
    mtx->km_holder = curthr;
  } else {
    //Otherwise, current thread should sleep (cancellable) in mutex queue
    curthr->kt_state = KT_SLEEP_CANCELLABLE;
    ktqueue_enqueue(&mtx->km_waitq, curthr);
    sched_switch();
  }

  //In either case, curthr should be mutex holder at function resolution
  KASSERT(curthr && (curthr == mtx->km_holder));
  
  //If thread was cancelled in queue, immediately unlock 
  if(curthr->kt_cancelled) {
    kmutex_unlock(mtx);
    return -EINTR;
  }

  return 0;
}

/*
 * If there are any threads waiting to take a lock on the mutex, one
 * should be woken up and given the lock.
 *
 * Note: This should _NOT_ be a blocking operation!
 *
 * Note: Ensure the new owner of the mutex enters the run queue.
 *
 * Note: Make sure that the thread on the head of the mutex's wait
 * queue becomes the new owner of the mutex.
 *
 * @param mtx the mutex to unlock
 */
void
kmutex_unlock(kmutex_t *mtx)
{
  /* Precondition KASSERT statements begin */
  KASSERT(curthr && (curthr == mtx->km_holder)); /* curthr must be valid and it must currently holding the mutex (mtx) */
  /* Precondition KASSERT statements end */

  if (0 == mtx->km_waitq.tq_size) {
    //If nobody is waiting for the mutex, we unlock it fully. 
    mtx->km_holder = NULL;
  } else {
    //Otherwise, pass the mutex to the front element
    kthread_t *next_in_queue = ktqueue_dequeue(&mtx->km_waitq);
    mtx->km_holder = next_in_queue;
    //Wake up element (since it has been waiting in the mutex queue)
    next_in_queue->kt_state = KT_RUN;
    sched_make_runnable(next_in_queue);
  }

  /* Postcondition KASSERT statements begin */
  KASSERT(curthr != mtx->km_holder); /* on return, curthr must not be the mutex (mtx) holder */
  /* Postcondition KASSERT statements end */
}
