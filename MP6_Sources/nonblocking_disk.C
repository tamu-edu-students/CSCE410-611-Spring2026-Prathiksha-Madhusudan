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
    : SimpleDisk(_size), queue_head(nullptr), queue_tail(nullptr)
{
    /* Register as IRQ 14 handler (primary IDE interrupt). */
    InterruptHandler::register_handler(14, this);
    Console::puts("Constructed NonBlockingDisk.\n");
}
/*--------------------------------------------------------------------------*/
/* Queue helpers added                                                      */
/*--------------------------------------------------------------------------*/
 
void NonBlockingDisk::enqueue_request(DiskRequest* req) {
    req->next = nullptr;
    if (queue_tail == nullptr) {
        queue_head = req;
        queue_tail = req;
    } else {
        queue_tail->next = req;
        queue_tail = req;
    }
}
 
DiskRequest* NonBlockingDisk::dequeue_request() {
    if (queue_head == nullptr) return nullptr;
    DiskRequest* req = queue_head;
    queue_head = queue_head->next;
    if (queue_head == nullptr) queue_tail = nullptr;
    return req;
}

/*--------------------------------------------------------------------------*/
/* MODIFIED: wait_while_busy -- the core change for this MP                 */
/*--------------------------------------------------------------------------*/
 
void NonBlockingDisk::wait_while_busy() {
    /* Enqueue current thread before yielding so any of
       multiple waiting threads can be woken in order. */
    DiskRequest req;
    req.thread = Thread::CurrentThread();
    enqueue_request(&req);
 
    /* Yield CPU -- we will be resumed by check_and_resume() (Option 2)
       or handle_interrupt() (Option 3) once the disk is ready. */
    System::SCHEDULER->yield();
 
    /* Safety: in the emulator the disk may still briefly report busy. */
    while (is_busy()) {}
}
 
/*--------------------------------------------------------------------------*/
/* check_and_resume                                                         */
/* Called from pass_on_CPU() in kernel.C each time a thread yields.         */
/* Checks if the disk is done and wakes up any waiting thread.              */
/*--------------------------------------------------------------------------*/
 
void NonBlockingDisk::check_and_resume() {
    /* IRQ 14 fires before we even get here, so queue is usually
       empty by this point. Kept as a harmless fallback. */
    if (queue_head != nullptr && !is_busy()) {
        DiskRequest* req = dequeue_request();
        if (req != nullptr) {
            System::SCHEDULER->resume(req->thread);
        }
    }
}

/*--------------------------------------------------------------------------*/
/* IRQ 14 bottom-half handler                                               */
/*--------------------------------------------------------------------------*/
 
void NonBlockingDisk::handle_interrupt(REGS* _regs) {
    /* Disk signals it is done. Wake the thread at the head of the queue. */
    if (queue_head != nullptr) {
        DiskRequest* req = dequeue_request();
        if (req != nullptr) {
            System::SCHEDULER->resume(req->thread);
        }
    }
    /* EOI is sent automatically by InterruptHandler::dispatch_interrupt(). */
}