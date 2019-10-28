/**The MIT License (MIT)

Copyright (c) 2015 by Daniel Eichhorn

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

See more at http://blog.squix.ch and https://github.com/squix78/json-streaming-parser
*/

#pragma once

#include "Listener.h"
#include "Error.h"
#include "Stack.h"
#include <string.h>

namespace JSON
{
class StreamingParser
{
public:
	enum class State {
		START_DOCUMENT,
		DONE,
		IN_OBJECT,
		IN_ARRAY,
		END_KEY,
		AFTER_KEY,
		IN_STRING,
		START_ESCAPE,
		UNICODE,
		IN_NUMBER,
		IN_TRUE,
		IN_FALSE,
		IN_NULL,
		AFTER_VALUE,
		UNICODE_SURROGATE,
	};

	enum class Item {
		OBJECT,
		ARRAY,
		KEY,
		STRING,
	};

	StreamingParser(Listener& listener) : listener(listener)
	{
		reset();
	}

	Error parse(char c);
	Error parse(const char* data, unsigned length);
	void reset();

	State getState() const
	{
		return state;
	}

private:
	Error bufferChar(char c)
	{
		if(bufferPos >= BUFFER_MAX_LENGTH - 1) {
			return Error::BufferFull;
		}

		buffer[bufferPos] = c;
		++bufferPos;
		return Error::Ok;
	}

	void sendKey()
	{
		buffer[bufferPos] = '\0';
		listener.onKey(buffer, bufferPos);
		state = State::END_KEY;
		bufferPos = 0;
	}

	void sendValue()
	{
		buffer[bufferPos] = '\0';
		listener.onValue(buffer, bufferPos);
		state = State::AFTER_VALUE;
		bufferPos = 0;
	}

	Error endArray();

	Error startValue(char c);

	Error processEscapeCharacters(char c);

	char convertCodepointToCharacter(uint16_t num);

	Error endUnicodeCharacter(uint16_t codepoint);

	Error startObject()
	{
		listener.startObject();
		state = State::IN_OBJECT;
		return stackPush(Item::OBJECT);
	}

	Error startArray()
	{
		listener.startArray();
		state = State::IN_ARRAY;
		return stackPush(Item::ARRAY);
	}

	void endDocument()
	{
		listener.endDocument();
		state = State::DONE;
	}

	Error endUnicodeSurrogateInterstitial();

	bool bufferContains(char c)
	{
		return memchr(buffer, c, bufferPos) != nullptr;
	}

	unsigned getHexArrayAsDecimal(char hexArray[], unsigned length);

	Error processUnicodeCharacter(char c);

	Error endObject();

	Error stackPush(Item obj)
	{
		return stack.push(obj) ? Error::Ok : Error::StackFull;
	}

	unsigned getNestingLevel() const
	{
		return stack.getLevel();
	}

private:
	Listener& listener;
	State state = State::START_DOCUMENT;
	Stack<Item, 20> stack;

	bool doEmitWhitespace = false;
	// fixed length buffer array to prepare for c code
	static constexpr uint16_t BUFFER_MAX_LENGTH = 512;
	char buffer[BUFFER_MAX_LENGTH];
	uint16_t bufferPos = 0;

	char unicodeEscapeBuffer[10];
	uint8_t unicodeEscapeBufferPos = 0;

	char unicodeBuffer[10];
	uint8_t unicodeBufferPos = 0;
	int unicodeHighSurrogate = 0;
};

} // namespace JSON
