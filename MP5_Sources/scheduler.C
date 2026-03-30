/*
 File: scheduler.C
 
 Author: Prathiksha Madhusudan
 Date  : 24/03/2026
 
 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "scheduler.H"
#include "thread.H"
#include "console.H"
#include "utils.H"
#include "assert.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   S c h e d u l e r  */
/*--------------------------------------------------------------------------*/

Scheduler::Scheduler() {
  head = nullptr;
  tail = nullptr;
  Console::puts("Constructed Scheduler.\n");
}

void Scheduler::yield() {
  Machine::disable_interrupts();

  if (head == nullptr) {
      Machine::enable_interrupts();
      return;
  }

  QueueNode * node = head;
  Thread    * next = node->thread;
  head = head->next;
  if (head == nullptr) tail = nullptr;
  delete node;

  Machine::enable_interrupts();
  Thread::dispatch_to(next);
}

void Scheduler::resume(Thread * _thread) {
  Machine::disable_interrupts();

  QueueNode * node = new QueueNode;
  node->thread = _thread;
  node->next   = nullptr;

  if (tail == nullptr) {
      head = tail = node;
  } else {
      tail->next = node;
      tail = node;
  }

  Machine::enable_interrupts();
}

void Scheduler::add(Thread * _thread) {
  resume(_thread);  // same as resume for FIFO
}

void Scheduler::terminate(Thread * _thread) {
  assert(false);
}

void EOQTimer::handle_interrupt(REGS * _r) {
    SimpleTimer::handle_interrupt(_r);

    if (Thread::CurrentThread() != nullptr) {
        Console::puts("EOQ: preempting thread ");
        Console::puti(Thread::CurrentThread()->ThreadId());
        Console::puts("\n");
        scheduler->resume(Thread::CurrentThread());
        scheduler->yield();
    }
}
