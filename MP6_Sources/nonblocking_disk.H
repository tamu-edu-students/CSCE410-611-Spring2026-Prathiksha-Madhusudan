/*
     File        : nonblocking_disk.H

     Author      : Prathiksha Madhusudan

     Date        : 
     Description : 

*/

#ifndef _NONBLOCKING_DISK_H_
#define _NONBLOCKING_DISK_H_

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "simple_disk.H"
#include "thread.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */ 
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* N o n B l o c k i n g D i s k  */
/*--------------------------------------------------------------------------*/

class NonBlockingDisk : public SimpleDisk {
private:
    /* The thread that is currently blocked waiting for the disk.
       Since we assume single-threaded disk access (base requirement),
       we only need to track one waiting thread at a time.
       Set to nullptr when no thread is waiting. */
    Thread* waiting_thread;
 
protected:
    /* MODIFIED: Override wait_while_busy() to yield the CPU instead of 
       busy-looping. The current thread blocks until the disk is ready. */
    virtual void wait_while_busy();
 
public:
    NonBlockingDisk(unsigned int _size);
    /* Creates a NonBlockingDisk device with the given size connected to the 
       MASTER slot of the primary ATA controller.
       NOTE: Requires System::SCHEDULER to be initialized before disk I/O. */
 
    /* Called by the scheduler (or any thread) to check if the disk is done
       and wake up any waiting thread. Should be called frequently, e.g.,
       each time a thread gets CPU time via pass_on_CPU(). */
    void check_and_resume();
};

#endif
