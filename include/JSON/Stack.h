#pragma once

#include <stdint.h>
#include <assert.h>

template <typename T, unsigned size> class Stack
{
public:
	bool push(T value)
	{
		if(level == size) {
			return false;
		}
		stack[level] = value;
		++level;
		return true;
	}

	T peek()
	{
		assert(level > 0);
		return stack[level - 1];
	}

	T pop()
	{
		assert(level > 0);
		--level;
		return stack[level];
	}

	bool isEmpty() const
	{
		return level == 0;
	}

	uint8_t getLevel() const
	{
		return level;
	}

private:
	T stack[size];
	uint8_t level = 0;
};
