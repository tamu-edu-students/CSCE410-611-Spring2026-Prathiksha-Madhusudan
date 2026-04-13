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
#include "interrupts.H" /* need interrup handler base class for IRQ 14 */

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */ 
/*--------------------------------------------------------------------------*/

/* One entry per blocked thread waiting for a disk operation to finish.
   Forms a singly-linked FIFO queue (Option 2). */
struct DiskRequest {
    Thread*      thread;   /* Thread to wake when this request completes */
    DiskRequest* next;     /* Next in queue; nullptr if last              */
};

/*--------------------------------------------------------------------------*/
/* N o n B l o c k i n g D i s k  */
/*--------------------------------------------------------------------------*/

class NonBlockingDisk : public SimpleDisk, public InterruptHandler {
private:
 
   /* ADDED: FIFO request queue (replaces single waiting_thread). */
    DiskRequest* queue_head;
    DiskRequest* queue_tail;
 
    void enqueue_request(DiskRequest* req);
    DiskRequest* dequeue_request();
 
protected:
 
    /*  yield CPU instead of busy-looping */
    virtual void wait_while_busy();
 
public:
 
    NonBlockingDisk(unsigned int _size);
    /* Constructor. Also registers self as IRQ 14 handler. */
 
    /* BASE: poll-based wake-up called from pass_on_CPU().
       This becomes a no-op since IRQ 14 drives wakeup. */
    void check_and_resume();
 
    /* IRQ 14 bottom-half handler */
    virtual void handle_interrupt(REGS* _regs);
};

#endif
