/*
     File        : nonblocking_disk.c

     Author      : Prathiksha Madhusudan
     Modified    : 

     Description : 

*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

    /* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "assert.H"
#include "utils.H"
#include "console.H"
#include "nonblocking_disk.H"
#include "system.H"       /* For System::SCHEDULER */
#include "thread.H"

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
/*--------------------------------------------------------------------------*/

NonBlockingDisk::NonBlockingDisk(unsigned int _size) 
  : SimpleDisk(_size), waiting_thread(nullptr) {
    /* Nothing extra to initialize beyond SimpleDisk and zeroing waiting_thread. */
    Console::puts("Constructed NonBlockingDisk.\n");
}
 
/*--------------------------------------------------------------------------*/
/* MODIFIED: wait_while_busy -- the core change for this MP                 */
/*--------------------------------------------------------------------------*/
 
void NonBlockingDisk::wait_while_busy() {
    /* Instead of busy-looping like SimpleDisk does:
       while (is_busy()) { }    <-- WE DO NOT DO THIS
 
       We store the current thread and yield the CPU.
       The thread will be resumed later via check_and_resume()
       once the disk signals it is ready (is_busy() returns false). */
 
    if (is_busy()) {
        /* -- BLOCK: record who is waiting and yield the CPU -- */
        waiting_thread = Thread::CurrentThread();
 
        /* Yield to the scheduler; this thread will sleep until resumed. */
        System::SCHEDULER->yield();
 
        /* When we return here, check_and_resume() has put us back on the 
           ready queue AND we've been dispatched back. The disk should be
           ready now. If (by some chance) it's still busy in the emulator, 
           yield again — this loop is safe but rarely iterates more than once. */
        while (is_busy()) {
            waiting_thread = Thread::CurrentThread();
            System::SCHEDULER->yield();
        }
    }
    /* Disk is ready — return to caller (ide_polling) to proceed with I/O. */
}
 
/*--------------------------------------------------------------------------*/
/* check_and_resume                                                          */
/* Called from pass_on_CPU() in kernel.C each time a thread yields.         */
/* Checks if the disk is done and wakes up any waiting thread.              */
/*--------------------------------------------------------------------------*/
 
void NonBlockingDisk::check_and_resume() {
    if (waiting_thread != nullptr && !is_busy()) {
        /* Disk is ready and someone is waiting — wake them up. */
        Thread* t = waiting_thread;
        waiting_thread = nullptr;          /* clear before resume to avoid re-entry */
        System::SCHEDULER->resume(t);      /* put the waiting thread back on ready queue */
    }
}
