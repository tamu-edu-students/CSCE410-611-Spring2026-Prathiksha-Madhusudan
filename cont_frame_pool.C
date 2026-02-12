/*
 File: ContFramePool.C
 
 Author: Prathiksha Madhusudan
 Date  : 02/02/2026
 
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

ContFramePool * ContFramePool::head = NULL;

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   C o n t F r a m e P o o l */
/*--------------------------------------------------------------------------*/

ContFramePool::ContFramePool(unsigned long _base_frame_no,
                             unsigned long _n_frames,
                             unsigned long _info_frame_no)
{
	base_frame_no = _base_frame_no;
	nframes = _n_frames;
	info_frame_no = _info_frame_no;
	nFreeFrames = _n_frames;
	
	// If _info_frame_no is zero then we keep management info in the first
    // frame, else we use the provided frame to keep management info
	if(info_frame_no == 0)
	{
        bitmap = (unsigned char *) (base_frame_no * FRAME_SIZE);
    }
	else
	{
        bitmap = (unsigned char *) (info_frame_no * FRAME_SIZE);
    }
	
	assert( (nframes%8) == 0 );
	
	// Initializing all bits in bitmap to zero
	for(int fno = 0; fno < _n_frames; fno++)
	{
        set_state(fno, FrameState::Free);
    }
	
	// Mark the first frame as being used if it is being used
    if( _info_frame_no == 0 )
	{
		set_state(0, FrameState::Used);
        nFreeFrames = nFreeFrames - 1;
    }
	
	// Creating a linked list and adding a new frame pool
	if( head == NULL )
	{
		head = this;
		head->next = NULL;
	}
	else
	{
		// Adding new frame pool to existing linked list
		ContFramePool * temp = NULL;
		for(temp = head; temp->next != NULL; temp = temp->next);
		temp->next = this;
		temp = this;
		temp->next = NULL;
	}
	
	Console::puts("Frame Pool initialized\n");
}


ContFramePool::FrameState ContFramePool::get_state(unsigned long _frame_no)
{
	unsigned int bitmap_row_index = (_frame_no / 4);
	unsigned int bitmap_col_index = ((_frame_no % 4)*2);
	unsigned char mask_result = (bitmap[bitmap_row_index] >> (bitmap_col_index)) & 0b11;
	FrameState state_output = FrameState::Used;

#if DEBUG
		Console::puts("get_state row index ="); Console::puti(bitmap_row_index); Console::puts("\n");
		Console::puts("get_state col index ="); Console::puti(bitmap_col_index); Console::puts("\n");
		Console::puts("get_state bitmap value = "); Console::puti(bitmap[bitmap_row_index]); Console::puts("\n");
		Console::puts("get_state mask result ="); Console::puti(mask_result); Console::puts("\n");
#endif

	if( mask_result == 0b00 )
	{
		state_output = FrameState::Free;
#if DEBUG
		Console::puts("get_state state_output = Free\n");
#endif
	
	}
	else if( mask_result == 0b01 )
	{
		state_output = FrameState::Used;
#if DEBUG
		Console::puts("get_state state_output = Used\n");
#endif
	}
	else if( mask_result == 0b11 )
	{
		state_output = FrameState::HoS;
#if DEBUG
		Console::puts("get_state state_output = HoS\n");
#endif
	}

	return state_output;
}


void ContFramePool::set_state(unsigned long _frame_no, FrameState _state)
{	
	unsigned int bitmap_row_index = (_frame_no / 4);
	unsigned int bitmap_col_index = ((_frame_no % 4)*2);
	
#if DEBUG
		Console::puts("set_state row index ="); Console::puti(bitmap_row_index); Console::puts("\n");
		Console::puts("set_state col index ="); Console::puti(bitmap_col_index); Console::puts("\n");
		Console::puts("set_state bitmap value before = "); Console::puti(bitmap[bitmap_row_index]); Console::puts("\n");
#endif
	
	switch(_state)
	{
		case FrameState::Free:
			bitmap[bitmap_row_index] &= ~(3<<bitmap_col_index);
			break;
		case FrameState::Used:
			bitmap[bitmap_row_index] ^= (1<<bitmap_col_index);
			break;
		case FrameState::HoS:
			bitmap[bitmap_row_index] ^= (3<<bitmap_col_index);
			break;
    }

#if DEBUG
		Console::puts("set_state bitmap value after = "); Console::puti(bitmap[bitmap_row_index]); Console::puts("\n");
#endif

	return;
}


unsigned long ContFramePool::get_frames(unsigned int _n_frames)
{	
	if( (_n_frames > nFreeFrames) || (_n_frames > nframes) )
	{
		Console::puts("ContFramePool::get_frames Invalid Request - Not enough free frames available!\n ");
		assert(false);
		return 0;
	}
	
	unsigned int index = 0;
	unsigned int free_frames_start = 0;
	unsigned int available_flag = 0;
	unsigned int free_frames_count = 0;
	unsigned int output = 0;
	
	for( index = 0; index < nframes; index++)
	{
		if( get_state(index) == FrameState::Free )
		{
			if(free_frames_count == 0)
			{
				// Save free frames start frame_no
				free_frames_start = index;
			}
			
			free_frames_count = free_frames_count + 1;
			
			// If free_frames_count is equal to the required num of frames
			if( free_frames_count == _n_frames )
			{
				available_flag = 1;
				break;
			}
		}
		else
		{
			free_frames_count = 0;
		}
	}
	
	if( available_flag == 1 )	
	{
		// Contiguous frames are available from free_frames_start
		for( index = free_frames_start; index < (free_frames_start + _n_frames); index++ )
		{
			if( index == free_frames_start )
			{
#if DEBUG
		Console::puts("get_frames Operation = HoS\n");
#endif
				set_state( index, FrameState::HoS);
			}
			else
			{
#if DEBUG
		Console::puts("get_frames Operation = Used\n");
#endif
				set_state( index, FrameState::Used );
			}
		}
		
		nFreeFrames = nFreeFrames - _n_frames;
		output = free_frames_start + base_frame_no;
	}
	else
	{
		output = 0;
		Console::puts("ContframePool::get_frames - Continuous free frames not available\n");
		assert(false);
	}
	
	return output;
}


void ContFramePool::mark_inaccessible(unsigned long _base_frame_no,
                                      unsigned long _n_frames)
{	
	if(	(_base_frame_no + _n_frames ) > (base_frame_no + nframes) || (_base_frame_no < base_frame_no) )
	{
		Console::puts("ContframePool::mark_inaccessible - Range out of bounds. Cannot mark inacessible.\n");
		assert(false);
		return;
	}
	
#if DEBUG
	Console::puts("Mark Inaccessible: _base_frame_no = "); Console::puti(_base_frame_no);
	Console::puts(" _n_frames ="); Console::puti(_n_frames);Console::puts("\n");
#endif

	unsigned int index = 0;
	for( index = _base_frame_no; index < (_base_frame_no + _n_frames); index++ )
	{
		if( get_state(index - base_frame_no) == FrameState::Free )
		{
			if( index == _base_frame_no )
			{
				set_state( (index - base_frame_no), FrameState::HoS);
			}
			else
			{
				set_state( (index - base_frame_no), FrameState::Used );
			}
			
			nFreeFrames = nFreeFrames - 1;
		}
#if DEBUG
		else
		{
			Console::puts("ContframePool::mark_inaccessible - Frame = "); Console::puti(index); Console::puts(" already marked inaccessible.\n");
			assert(false);
		}
#endif
	}
	
	return;
}


void ContFramePool::release_frames(unsigned long _first_frame_no)
{
	unsigned int found_frame = 0;
	ContFramePool * temp = head;

#if DEBUG
	Console::puts("In release_frames: First frame no ="); Console::puti(_first_frame_no); Console::puts("\n");
#endif

	// To find which pool the frame belongs to
	while( temp != NULL )
	{

#if DEBUG
		Console::puts("In release_frames: Base frame lower ="); Console::puti(temp->base_frame_no); Console::puts("\n");
		Console::puts("In release_frames: Base frame upper ="); Console::puti(temp->base_frame_no+temp->nframes); Console::puts("\n");
#endif
		if( (_first_frame_no >= temp->base_frame_no) && (_first_frame_no < (temp->base_frame_no + temp->nframes)) )
		{
			found_frame = 1;
			temp->release_frames_in_pool(_first_frame_no);
			break;
		}
		
		temp = temp->next;
	}
	
	if( found_frame == 0 )
	{
		Console::puts("ContframePool::release_frames - Cannot release frame. Frame not found in frame pools.\n");
		assert(false);
	}
	
	return;
}

void ContFramePool::release_frames_in_pool(unsigned long _first_frame_no)
{
	unsigned int index = 0;
	
	// Get the state of frame
	if( get_state(_first_frame_no - base_frame_no) == FrameState::HoS )
	{
		for( index = _first_frame_no; index < (_first_frame_no + nframes); index++)
		{
			set_state((index - base_frame_no), FrameState::Free);
			nFreeFrames = nFreeFrames + 1;
		}
	}
	else
	{
		Console::puts("ContframePool::release_frames_in_pool - Cannot release frame. Frame state is not HoS.\n");
		assert(false);
	}
}


unsigned long ContFramePool::needed_info_frames(unsigned long _n_frames)
{	
	// Since we use 2 bits per frame
	return ( (_n_frames*2) / (4*1024*8) ) + ( ( (_n_frames*2) % (4*1024*8) ) > 0 ? 1 : 0 );
}