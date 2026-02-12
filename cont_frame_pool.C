/*
 File: ContFramePool.C
 
 Author: Prathiksha Madhusudan
 Date : 02 /02/2026
 
 */

/*--------------------------------------------------------------------------*/
/* 
 POSSIBLE IMPLEMENTATION
 -----------------------

 The class SimpleFramePool in file "simple_frame_pool.H/C" describes an
 incomplete vanilla implementation of a frame pool that allocates 
 *single* frames at a time. Because it does allocate one frame at a time, 
 it does not guarantee that a sequence of frames is allocated contiguously.
 This can cause problems.
 
 The class ContFramePool has the ability to allocate either single frames,
 or sequences of contiguous frames. This affects how we manage the
 free frames. In SimpleFramePool it is sufficient to maintain the free 
 frames.
 In ContFramePool we need to maintain free *sequences* of frames.
 
 This can be done in many ways, ranging from extensions to bitmaps to 
 free-lists of frames etc.
 
 IMPLEMENTATION:
 
 One simple way to manage sequences of free frames is to add a minor
 extension to the bitmap idea of SimpleFramePool: Instead of maintaining
 whether a frame is FREE or ALLOCATED, which requires one bit per frame, 
 we maintain whether the frame is FREE, or ALLOCATED, or HEAD-OF-SEQUENCE.
 The meaning of FREE is the same as in SimpleFramePool. 
 If a frame is marked as HEAD-OF-SEQUENCE, this means that it is allocated
 and that it is the first such frame in a sequence of frames. Allocated
 frames that are not first in a sequence are marked as ALLOCATED.
 
 NOTE: If we use this scheme to allocate only single frames, then all 
 frames are marked as either FREE or HEAD-OF-SEQUENCE.
 
 NOTE: In SimpleFramePool we needed only one bit to store the state of 
 each frame. Now we need two bits. In a first implementation you can choose
 to use one char per frame. This will allow you to check for a given status
 without having to do bit manipulations. Once you get this to work, 
 revisit the implementation and change it to using two bits. You will get 
 an efficiency penalty if you use one char (i.e., 8 bits) per frame when
 two bits do the trick.
 
 DETAILED IMPLEMENTATION:
 
 How can we use the HEAD-OF-SEQUENCE state to implement a contiguous
 allocator? Let's look a the individual functions:
 
 Constructor: Initialize all frames to FREE, except for any frames that you 
 need for the management of the frame pool, if any.
 
 get_frames(_n_frames): Traverse the "bitmap" of states and look for a 
 sequence of at least _n_frames entries that are FREE. If you find one, 
 mark the first one as HEAD-OF-SEQUENCE and the remaining _n_frames-1 as
 ALLOCATED.

 release_frames(_first_frame_no): Check whether the first frame is marked as
 HEAD-OF-SEQUENCE. If not, something went wrong. If it is, mark it as FREE.
 Traverse the subsequent frames until you reach one that is FREE or 
 HEAD-OF-SEQUENCE. Until then, mark the frames that you traverse as FREE.
 
 mark_inaccessible(_base_frame_no, _n_frames): This is no different than
 get_frames, without having to search for the free sequence. You tell the
 allocator exactly which frame to mark as HEAD-OF-SEQUENCE and how many
 frames after that to mark as ALLOCATED.
 
 needed_info_frames(_n_frames): This depends on how many bits you need 
 to store the state of each frame. If you use a char to represent the state
 of a frame, then you need one info frame for each FRAME_SIZE frames.
 
 A WORD ABOUT RELEASE_FRAMES():
 
 When we releae a frame, we only know its frame number. At the time
 of a frame's release, we don't know necessarily which pool it came
 from. Therefore, the function "release_frame" is static, i.e., 
 not associated with a particular frame pool.
 
 This problem is related to the lack of a so-called "placement delete" in
 C++. For a discussion of this see Stroustrup's FAQ:
 http://www.stroustrup.com/bs_faq2.html#placement-delete
 
 */
/*--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "cont_frame_pool.H"
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
/* METHODS FOR CLASS   C o n t F r a m e P o o l */
/*--------------------------------------------------------------------------*/



//Frame Pool list head
ContFramePool* ContFramePool::head = nullptr;

unsigned int ContFramePool::get_state(unsigned long _frame_no) const 
{
    unsigned long bit_pos = (_frame_no - base_frame_no) * BITS_PER_FRAME;
    unsigned long int_index = bit_pos / 8;
    unsigned long bit_offset = bit_pos % 8;

    return (bitmap[int_index] >> bit_offset) & 0x3; //extracting 2 bits
}

void ContFramePool::set_state(unsigned long _frame_no, FrameState _state) 
{
    unsigned long bit_pos = (_frame_no - base_frame_no) * BITS_PER_FRAME;
    unsigned long int_index = bit_pos / 8;
    unsigned long bit_offset = bit_pos % 8;
    bitmap[int_index] &= ~(0x3 << bit_offset); // Clear the bits
    bitmap[int_index] |= ((unsigned int)(_state) << bit_offset); // Set the new state
}

ContFramePool::ContFramePool(unsigned long _base_frame_no,
                             unsigned long _n_frames,
                             unsigned long _info_frame_no)
{
    base_frame_no = _base_frame_no;
    n_frames = _n_frames;
    info_frame_no = _info_frame_no;
    next = nullptr;
    prev = nullptr;
    
    //Ensure the number of frames does not exceed the static array size
    assert(_n_frames <= MAX_FRAMES);

    //Initialize all frames to FREE state
    for(unsigned long i = 0; i < (n_frames * BITS_PER_FRAME + 7) / 8; i++)
        bitmap[i] = (unsigned char)ContFramePool::FrameState::Free;

    //Register this pool into the doubly linked list
    if(!head)
        head = this;
    else
    {
        ContFramePool* current = head;
        while (current->next)
        {
            current = current->next;
        }
        current->next = this;
        this->prev = current;
    }
}

unsigned long ContFramePool::get_frames(unsigned int _n_frames)
{
    // going through the frames inisde the pool and finding contiguous
    // free frames by reading the state of each frame
    for (unsigned long i = 0; i <= n_frames - _n_frames; i++) 
    {
        bool found = true;
        for (unsigned int j = 0; j < _n_frames; j++) 
        {
            if (get_state(base_frame_no + i + j) != (unsigned int)(FrameState::Free)) 
            {
                found = false;
                break;
            }
        }
        if (found) 
        {
            set_state(base_frame_no + i, FrameState::HoS);
            for (unsigned int j = 1; j < _n_frames; ++j) {
                set_state(base_frame_no + i + j, FrameState::Used);
            }
            return base_frame_no + i;
        }
    }
    
    Console::puts("ContFramePool::get_frames failed to allocate\n");
    return 0;
}

void ContFramePool::mark_inaccessible(unsigned long _base_frame_no,
                                      unsigned long _n_frames)
{
    unsigned long start = _base_frame_no - base_frame_no;
    set_state(base_frame_no + start, FrameState::HoS);
    for (unsigned long i = start + 1; i < start + _n_frames; ++i) {
        set_state(base_frame_no + i, FrameState::Used);
    }
}

//Helper function for release frames()
ContFramePool* ContFramePool::find_pool_by_frame(unsigned long frame_no) 
{
    ContFramePool* current = head;
    while(current != nullptr)
    {
        if((frame_no >= current->base_frame_no) && (frame_no < (current->base_frame_no + current->n_frames)))
            return current;
        current = current->next;
    }
    return nullptr;
}

void ContFramePool::release_frames(unsigned long _first_frame_no)
{
    ContFramePool* pool = find_pool_by_frame(_first_frame_no);
    if (pool == nullptr) {
        Console::puts("Error: Frame does not belong to any pool\n");
        assert(false);
    }

    unsigned long start = _first_frame_no - pool->base_frame_no;
    if (pool->get_state(pool->base_frame_no + start) != static_cast<unsigned int>(FrameState::HoS)) {
        Console::puts("Error: Attempting to release non-head of sequence frame\n");
        assert(false);
    }

    pool->set_state(pool->base_frame_no + start, FrameState::Free);
    for (unsigned long i = start + 1; i < pool->n_frames && pool->get_state(pool->base_frame_no + i) == static_cast<unsigned int>(FrameState::Used); ++i) {
        pool->set_state(pool->base_frame_no + i, FrameState::Free);
    }

}

unsigned long ContFramePool::needed_info_frames(unsigned long _n_frames)
{
    return (_n_frames * BITS_PER_FRAME + Machine::PAGE_SIZE - 1) / Machine::PAGE_SIZE; // Number of 32-bit integers needed
}
