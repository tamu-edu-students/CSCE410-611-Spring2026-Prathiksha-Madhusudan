/*
 File: scheduler.C
 
 Author: Prathiksha Madhusudan
 Date  :
 */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "scheduler.H"
#include "thread.H"
#include "console.H"
#include "utils.H"
#include "assert.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES -- simple fixed-size circular queue                      */
/*--------------------------------------------------------------------------*/

/* Maximum number of threads the scheduler can handle. Adjust if needed. */
#define MAX_THREADS 10

static Thread* ready_queue[MAX_THREADS];
static int queue_head = 0;   /* index of next thread to dequeue */
static int queue_tail = 0;   /* index where next thread will be enqueued */
static int queue_size = 0;   /* number of threads currently in the queue */

/*--------------------------------------------------------------------------*/
/* INTERNAL HELPERS */
/*--------------------------------------------------------------------------*/

static void enqueue(Thread* t) {
    assert(queue_size < MAX_THREADS);   /* queue must not be full */
    ready_queue[queue_tail] = t;
    queue_tail = (queue_tail + 1) % MAX_THREADS;
    queue_size++;
}

static Thread* dequeue() {
    assert(queue_size > 0);             /* queue must not be empty */
    Thread* t = ready_queue[queue_head];
    queue_head = (queue_head + 1) % MAX_THREADS;
    queue_size--;
    return t;
}

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   S c h e d u l e r  */
/*--------------------------------------------------------------------------*/

Scheduler::Scheduler() {
    /* Initialize the ready queue. */
    queue_head = 0;
    queue_tail = 0;
    queue_size = 0;
    Console::puts("Constructed Scheduler.\n");
}

void Scheduler::yield() {
    /* Current thread gives up the CPU.
       Pick the next thread from the ready queue and dispatch to it.
       If no thread is ready, we hang (shouldn't happen in normal operation). */

    if (queue_size == 0) {
        /* No runnable threads — this shouldn't happen in a correct program. */
        Console::puts("SCHEDULER: No threads in ready queue! Halting.\n");
        assert(false);
    }

    Thread* next = dequeue();
    Thread::dispatch_to(next);
    /* When we return here, this thread has been re-dispatched to. */
}

void Scheduler::resume(Thread* _thread) {
    /* Put the given thread back onto the ready queue. */
    enqueue(_thread);
}

void Scheduler::add(Thread* _thread) {
    /* A newly created thread is made runnable by adding it to the ready queue. */
    resume(_thread);
}

void Scheduler::terminate(Thread* _thread) {
    /* If the terminating thread is the current thread, yield to next. 
       Otherwise, we'd need to search the queue and remove it — 
       for simplicity (base requirement), we handle the self-terminate case. */
    if (_thread == Thread::CurrentThread()) {
        /* Don't put ourselves back; just yield to the next thread. */
        if (queue_size > 0) {
            Thread* next = dequeue();
            Thread::dispatch_to(next);
        }
        /* If queue is empty, just stop (infinite loop or halt). */
        for(;;);
    }
    /* NOTE: For threads other than current, a full implementation would
       search and remove from the queue. Omitted for the base requirement. */
}