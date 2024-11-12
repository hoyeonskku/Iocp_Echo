#include "CRingBuffer.h"

#include <stdlib.h>
#include <memory.h>

#define FR_DATA_CONTINUOUS (p.front_ < p.rear_)
#define RF_DATA_CONTINUOUS1 (p.rear_ < p.front_ && p.rear_ == 0)
#define RF_DATA_CONTINUOUS2 (p.rear_ < p.front_ && p.front_ == size_ - 1)
#define RF_DATA_DIVIDED (p.rear_ < p.front_ && p.rear_ != 0 && p.front_ != size_ - 1)


ring_buffer::ring_buffer(size_t size) : size_(size + 1)
{
	if (size == 0)
	{
		__debugbreak();
	}

	buf_ = (char*)malloc(size_);
	if (buf_ == NULL)
	{
		__debugbreak();
	}

	ptr_.front_ = 0;
	ptr_.rear_ = 1;
}

ring_buffer::~ring_buffer(void)
{
	free(buf_);
}

bool ring_buffer::empty(void)
{
	ptr_t p;

	p.total_ = ptr_.total_;

	if ((p.front_ + 1) % size_ == p.rear_) return true;
	return false;
}

bool ring_buffer::full(void)
{
	ptr_t p;

	p.total_ = ptr_.total_;

	if (p.front_ == p.rear_) return true;
	return false;
}

size_t ring_buffer::buffer_size(void)
{
	return size_ - 1;
}

size_t ring_buffer::used_size(void)
{
	ptr_t p;

	p.total_ = ptr_.total_;

	if (empty())
	{
		return 0;
	}
	else if (full())
	{
		return size_ - 1;
	}
	else if (FR_DATA_CONTINUOUS)
	{
		return p.rear_ - p.front_ - 1;
	}
	else if (RF_DATA_CONTINUOUS1)
	{
		return size_ - p.front_ - 1;
	}
	else if (RF_DATA_CONTINUOUS2)
	{
		return p.rear_;
	}
	else if (RF_DATA_DIVIDED)
	{
		return size_ - p.front_ - 1 + p.rear_;
	}
}

size_t ring_buffer::free_size(void)
{
	return size_ - used_size() - 1;
}

size_t ring_buffer::enqueue(char* buf, size_t size)
{
	ptr_t p;
	size_t first_copy_size;
	size_t second_copy_size;

	p.total_ = ptr_.total_;

	if (full())
	{
		__debugbreak();
		return 0;
	}

	if (size > free_size())
	{
		//__debugbreak();
		return 0;
	}

	if (size <= direct_enqueue_size())
	{
		memcpy_s(buf_ + p.rear_, size, buf, size);
	}
	else
	{
		first_copy_size = direct_enqueue_size();
		second_copy_size = size - first_copy_size;

		memcpy_s(buf_ + p.rear_, first_copy_size, buf, first_copy_size);
		memcpy_s(buf_, second_copy_size, buf + first_copy_size, second_copy_size);
	}

	move_rear(size);

	return size;
}

size_t ring_buffer::dequeue(char* buf, size_t size)
{
	ptr_t p;
	size_t first_copy_size;
	size_t second_copy_size;
	size_t idx;

	p.total_ = ptr_.total_;

	if (empty())
	{
		__debugbreak();
		return 0;
	}

	if (size > used_size())
	{
		__debugbreak();
		return 0;
	}

	idx = (p.front_ + 1) % size_;

	if (size <= direct_dequeue_size())
	{
		memcpy_s(buf, size, buf_ + idx, size);
	}
	else
	{
		first_copy_size = direct_dequeue_size();
		second_copy_size = size - first_copy_size;

		memcpy_s(buf, first_copy_size, buf_ + idx, first_copy_size);
		memcpy_s(buf + first_copy_size, second_copy_size, buf_, second_copy_size);
	}

	move_front(size);

	return size;
}

size_t ring_buffer::peek(char* buf, size_t size)
{
	ptr_t p;
	size_t first_copy_size;
	size_t second_copy_size;
	size_t idx;

	p.total_ = ptr_.total_;

	if (empty())
	{
		__debugbreak();
		return 0;
	}

	if (size > used_size())
	{
		__debugbreak();
		return 0;
	}

	idx = (p.front_ + 1) % size_;

	if (size <= direct_dequeue_size())
	{
		memcpy_s(buf, size, buf_ + idx, size);
	}
	else
	{
		first_copy_size = direct_dequeue_size();
		second_copy_size = size - first_copy_size;

		memcpy_s(buf, first_copy_size, buf_ + idx, first_copy_size);
		memcpy_s(buf + first_copy_size, second_copy_size, buf_, second_copy_size);
	}

	return size;
}

void ring_buffer::clear(void)
{
	ptr_.front_ = 0;
	ptr_.rear_ = 1;
}

size_t ring_buffer::direct_enqueue_size(void)
{
	ptr_t p;

	p.total_ = ptr_.total_;

	if (full())
	{
		return 0;
	}
	else if (empty())
	{
		if (p.front_ < p.rear_)
		{
			return size_ - p.rear_;
		}
		else
		{
			return size_ - 1;
		}
	}
	else if (FR_DATA_CONTINUOUS)
	{
		return size_ - p.rear_;
	}
	else if (RF_DATA_CONTINUOUS1 || RF_DATA_CONTINUOUS2 || RF_DATA_DIVIDED)
	{
		return p.front_ - p.rear_;
	}
	else
	{
		__debugbreak();
	}
}

size_t ring_buffer::direct_dequeue_size(void)
{
	ptr_t p;

	p.total_ = ptr_.total_;

	if (empty())
	{
		return 0;
	}
	else if (full())
	{
		return size_ - p.front_ - 1;
	}
	else if (FR_DATA_CONTINUOUS)
	{
		return p.rear_ - p.front_ - 1;
	}
	else if (RF_DATA_CONTINUOUS1 || RF_DATA_DIVIDED)
	{
		return size_ - p.front_ - 1;
	}
	else if (RF_DATA_CONTINUOUS2)
	{
		return p.rear_;
	}
	else
	{
		__debugbreak();
	}
}

size_t ring_buffer::move_front(size_t offset)
{
	if (offset > used_size())
	{
		__debugbreak();
	}

	ptr_.front_ = (ptr_.front_ + offset) % size_;

	return offset;
}

size_t ring_buffer::move_rear(size_t offset)
{
	if (offset > free_size())
	{
		__debugbreak();
	}

	ptr_.rear_ = (ptr_.rear_ + offset) % size_;

	return offset;
}

char* ring_buffer::buffer_ptr(void)
{
	return buf_;
}

char* ring_buffer::front_ptr(void)
{
	return buf_ + ((ptr_.front_ + 1) % size_);
}

char* ring_buffer::rear_ptr(void)
{
	return buf_ + ptr_.rear_;
}
