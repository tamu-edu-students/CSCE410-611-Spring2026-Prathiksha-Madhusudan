/*
 File: ContFramePool.C
 
 Author: Prathiksha Madhusudan
 Date  : 02/20/2026
 
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

/* Define static members declared in cont_frame_pool.H */
ContFramePool * ContFramePool::pool_list[ContFramePool::MAX_POOLS];
unsigned int    ContFramePool::num_pools = 0;

ContFramePool::FrameState ContFramePool::get_state(unsigned long _frame_no)
{
    unsigned long idx   = _frame_no - base_frame_no; /* offset of this frame relative to pool start */
    unsigned long byte  = idx / 4;                   /* each byte holds 4 frames (2 bits each) */
    unsigned long shift = (3 - (idx % 4)) * 2;       /* bit position within the byte, MSB-first */
    unsigned char bits  = (bitmap[byte] >> shift) & 0x3; /* extract the 2-bit state */
    if (bits == 0x0) return FrameState::Free;
    if (bits == 0x1) return FrameState::Used;
    return FrameState::HoS;
}

void ContFramePool::set_state(unsigned long _frame_no, FrameState _state)
{
    unsigned long idx   = _frame_no - base_frame_no;
    unsigned long byte  = idx / 4;
    unsigned long shift = (3 - (idx % 4)) * 2;
    unsigned char val;
    if      (_state == FrameState::Free) val = 0x0;
    else if (_state == FrameState::Used) val = 0x1;
    else                                  val = 0x2;
    bitmap[byte] = (bitmap[byte] & ~(0x3 << shift)) | (val << shift);
}

ContFramePool::ContFramePool(unsigned long _base_frame_no,
                             unsigned long _n_frames,
                             unsigned long _info_frame_no)
{
    base_frame_no = _base_frame_no;
    n_frames      = _n_frames;
    n_info_frames = needed_info_frames(_n_frames); /* compute how many frames the bitmap itself needs */

    /* If _info_frame_no is 0, store the bitmap at the start of this pool's
       own memory. Otherwise use the externally provided frame (e.g. a frame
       borrowed from the kernel pool to hold the process pool's bitmap). */
    if (_info_frame_no == 0) {
        bitmap = (unsigned char *)(base_frame_no * FRAME_SIZE);
    } else {
        bitmap = (unsigned char *)(_info_frame_no * FRAME_SIZE);
    }

    /* Mark every frame in the pool as Free to start with. */
    for (unsigned long i = 0; i < _n_frames; i++) {
        set_state(base_frame_no + i, FrameState::Free);
    }

    /* If the bitmap lives inside the pool itself, reserve those info frames
       so they are never handed out as usable memory. */
    if (_info_frame_no == 0) {
        set_state(base_frame_no, FrameState::HoS); /* first info frame is the head of the reserved sequence */
        for (unsigned long i = 1; i < n_info_frames; i++) {
            set_state(base_frame_no + i, FrameState::Used); /* remaining info frames marked Used */
        }
    }

    /* Register this pool in the global list so the static release_frames()
       can search across all pools to find the owner of any frame number. */
    assert(num_pools < MAX_POOLS);
    pool_list[num_pools++] = this;

    Console::puts("ContframePool::Constructor implemented!\n");
}

unsigned long ContFramePool::get_frames(unsigned int _n_frames)
{
    unsigned long run_start = 0; /* frame number where the current free run begins */
    unsigned long run_len   = 0; /* length of the current consecutive free run */

    /* Scan every frame in this pool looking for a run of _n_frames free frames. */
    for (unsigned long i = base_frame_no; i < base_frame_no + n_frames; i++) {
        if (get_state(i) == FrameState::Free) {
            if (run_len == 0) run_start = i; /* record where this run starts */
            run_len++;
            if (run_len == _n_frames) {
                /* Found a long enough run — mark it and return the first frame number. */
                set_state(run_start, FrameState::HoS); /* first frame is the head of sequence */
                for (unsigned long j = run_start + 1; j < run_start + _n_frames; j++) {
                    set_state(j, FrameState::Used); /* remaining frames in the sequence marked Used */
                }
                return run_start;
            }
        } else {
            run_len = 0; /* hit a non-free frame, reset the run counter */
        }
    }

    Console::puts("ContframePool::get_frames implemented!\n");
    return 0; // keep compiler happy
}

void ContFramePool::mark_inaccessible(unsigned long _base_frame_no,
                                      unsigned long _n_frames)
{
    set_state(_base_frame_no, FrameState::HoS);
    for (unsigned long i = _base_frame_no + 1; i < _base_frame_no + _n_frames; i++) {
        set_state(i, FrameState::Used);
    }
    Console::puts("ContframePool::mark_inaccessible implemented!\n");
}

void ContFramePool::release_frames(unsigned long _first_frame_no)
{
    /* Search all registered pools to find the one that owns _first_frame_no. */
    ContFramePool * pool = nullptr;
    for (unsigned int i = 0; i < num_pools; i++) {
        unsigned long lo = pool_list[i]->base_frame_no;
        unsigned long hi = lo + pool_list[i]->n_frames;
        if (_first_frame_no >= lo && _first_frame_no < hi) {
            pool = pool_list[i];
            break;
        }
    }
    assert(pool != nullptr); /* frame number must belong to some pool */
    assert(pool->get_state(_first_frame_no) == FrameState::HoS); /* must be the start of a sequence */

    /* Free the head frame, then walk forward freeing every Used frame that
       belongs to the same sequence, stopping at the next Free or HoS frame. */
    pool->set_state(_first_frame_no, FrameState::Free);
    unsigned long i = _first_frame_no + 1;
    while (i < pool->base_frame_no + pool->n_frames &&
           pool->get_state(i) == FrameState::Used)
    {
        pool->set_state(i, FrameState::Free);
        i++;
    }
    Console::puts("ContframePool::release_frames implemented!\n");
}

unsigned long ContFramePool::needed_info_frames(unsigned long _n_frames)
{
    unsigned long frames_per_info_frame = FRAME_SIZE * 4;
    Console::puts("ContframePool::need_info_frames implemented!\n");
    return _n_frames / frames_per_info_frame +
           (_n_frames % frames_per_info_frame > 0 ? 1 : 0);
}