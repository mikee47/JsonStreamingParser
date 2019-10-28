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

#include "JsonListener.h"
#include <string.h>
#include <assert.h>

class JsonStreamingParser
{
public:
	JsonStreamingParser(JsonListener& listener) : listener(listener)
	{
		reset();
	}

	void parse(char c);
	void setListener(JsonListener* listener);
	void reset();

private:
	enum class State {
		START_DOCUMENT,
		DONE,
		IN_ARRAY,
		IN_OBJECT,
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

	enum class Obj {
		OBJECT,
		ARRAY,
		KEY,
		STRING,
	};

	void bufferChar(char c)
	{
		if(bufferPos < BUFFER_MAX_LENGTH - 1) {
			buffer[bufferPos] = c;
			++bufferPos;
		}
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

	void endArray();

	void startValue(char c);

	void processEscapeCharacters(char c);

	char convertCodepointToCharacter(uint16_t num);

	void endUnicodeCharacter(uint16_t codepoint);

	void startObject();

	void startArray();

	void endDocument();

	void endUnicodeSurrogateInterstitial();

	bool doesCharArrayContain(const char myArray[], unsigned length, char c)
	{
		return memchr(myArray, c, length) != nullptr;
	}

	unsigned getHexArrayAsDecimal(char hexArray[], unsigned length);

	void processUnicodeCharacter(char c);

	void endObject();

	void push(Obj obj)
	{
		assert(stackPos < stackSize - 1);
		stack[stackPos] = obj;
		++stackPos;
	}

	Obj peek()
	{
		assert(stackPos > 0);
		return stack[stackPos - 1];
	}

	Obj pop()
	{
		assert(stackPos > 0);
		--stackPos;
		return stack[stackPos];
	}

private:
	JsonListener& listener;
	State state = State::START_DOCUMENT;
	static constexpr unsigned stackSize = 20;
	Obj stack[stackSize];
	uint8_t stackPos = 0;

	bool doEmitWhitespace = false;
	// fixed length buffer array to prepare for c code
	static constexpr uint16_t BUFFER_MAX_LENGTH = 512;
	char buffer[BUFFER_MAX_LENGTH];
	uint16_t bufferPos = 0;

	char unicodeEscapeBuffer[10];
	uint8_t unicodeEscapeBufferPos = 0;

	char unicodeBuffer[10];
	uint8_t unicodeBufferPos = 0;
	uint8_t characterCounter = 0;
	int unicodeHighSurrogate = 0;
};
