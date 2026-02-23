#include "assert.H"
#include "exceptions.H"
#include "console.H"
#include "paging_low.H"
#include "page_table.H"

PageTable * PageTable::current_page_table = nullptr;
unsigned int PageTable::paging_enabled = 0;
ContFramePool * PageTable::kernel_mem_pool = nullptr;
ContFramePool * PageTable::process_mem_pool = nullptr;
unsigned long PageTable::shared_size = 0;



void PageTable::init_paging(ContFramePool * _kernel_mem_pool,
                            ContFramePool * _process_mem_pool,
                            const unsigned long _shared_size)
{
   /* Store the global parameters for the paging subsystem. */
   kernel_mem_pool  = _kernel_mem_pool;
   process_mem_pool = _process_mem_pool;
   shared_size      = _shared_size;
   Console::puts("Initialized Paging System\n");
}

PageTable::PageTable()
{
   /* Allocate a frame from the kernel pool to hold the page directory. */
   page_directory = (unsigned long *)(kernel_mem_pool->get_frames(1) * PAGE_SIZE);

   /* Allocate a frame from the kernel pool to hold the page table that
      covers the first 4MB (the direct-mapped shared region). */
   unsigned long * page_table = (unsigned long *)(kernel_mem_pool->get_frames(1) * PAGE_SIZE);

   /* Fill in the page table entries for the direct-mapped region.
      Each entry maps virtual frame i to physical frame i (identity mapping).
      Bits: present=1, read/write=1 -> flags = 0x3 */
   unsigned long addr = 0;
   for (unsigned int i = 0; i < ENTRIES_PER_PAGE; i++) {
      page_table[i] = addr | 0x3; /* mark present and read/write */
      addr += PAGE_SIZE;
   }

   /* Install the page table for the first 4MB into directory entry 0.
      Mark it present and read/write. */
   page_directory[0] = (unsigned long)page_table | 0x3;

   /* Mark all remaining directory entries as not present.
      They will be filled in on demand by the page fault handler. */
   for (unsigned int i = 1; i < ENTRIES_PER_PAGE; i++) {
      page_directory[i] = 0x0;
   }

   Console::puts("Constructed Page Table object\n");
}


void PageTable::load()
{
   /* Write the physical address of the page directory into CR3.
      The CPU uses CR3 to locate the page directory on every address translation. */
   write_cr3((unsigned long)page_directory);
   current_page_table = this;
   Console::puts("Loaded page table\n");
}

void PageTable::enable_paging()
{
   /* Set bit 31 (PG) of CR0 to switch the CPU into paged addressing mode. */
   write_cr0(read_cr0() | 0x80000000);
   paging_enabled = 1;
   Console::puts("Enabled paging\n");
}

void PageTable::handle_fault(REGS * _r)
{
  /* Read the faulting virtual address from CR2. */
  unsigned long fault_addr = read_cr2();

  /* Extract the page directory index (top 10 bits) and
     the page table index (next 10 bits) from the fault address. */
  unsigned long pd_index = fault_addr >> 22;
  unsigned long pt_index = (fault_addr >> 12) & 0x3FF;

  /* Get a pointer to the current page directory. */
  unsigned long * pd = current_page_table->page_directory;

  /* If the page directory entry is not present, we need to allocate a new
     page table page and install it in the directory. */
  if ((pd[pd_index] & 0x1) == 0) {
     unsigned long * new_pt = (unsigned long *)(kernel_mem_pool->get_frames(1) * PAGE_SIZE);
     /* Initialize all entries in the new page table as not present. */
     for (unsigned int i = 0; i < ENTRIES_PER_PAGE; i++) {
        new_pt[i] = 0x0;
     }
     /* Install the new page table into the directory, marked present and read/write. */
     pd[pd_index] = (unsigned long)new_pt | 0x3;
  }

  /* Get a pointer to the page table for this directory entry. */
  unsigned long * pt = (unsigned long *)(pd[pd_index] & 0xFFFFF000);

  /* Allocate a fresh frame from the process pool for the faulting page and
     install it in the page table, marked present and read/write. */
  unsigned long new_frame = process_mem_pool->get_frames(1) * PAGE_SIZE;
  pt[pt_index] = new_frame | 0x3;

  Console::puts("handled page fault\n");
}