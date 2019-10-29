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
		END_DOCUMENT,
		IN_KEY,
		END_KEY,
		AFTER_KEY,
		IN_OBJECT,
		IN_ARRAY,
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
	// valid whitespace characters in JSON (from RFC4627 for JSON) include:
	// space, horizontal tab, line feed or new line, and carriage return.
	// thanks:
	// http://stackoverflow.com/questions/16042274/definition-of-whitespace-in-json
	static bool isWhiteSpace(char c)
	{
		return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
	}

	Error bufferChar(char c)
	{
		if(bufferPos >= BUFFER_MAX_LENGTH - 1) {
			return Error::BufferFull;
		}

		buffer[bufferPos] = c;
		++bufferPos;
		return Error::Ok;
	}

	void startElement(Element::Type type)
	{
		buffer[bufferPos] = '\0';
		auto level = stack.getLevel();
		Element elem(type, level);
		if(level > 0) {
			auto& item = stack.peek();
			elem.index = item.index;
			elem.container = item.isObject ? Element::Type::Object : Element::Type::Array;
			++item.index;
		} else {
			elem.container = Element::Type::Document;
			elem.index = 0;
		}
		elem.key = buffer;
		elem.keyLength = keyLength;
		elem.value = &buffer[keyLength + 1];
		elem.valueLength = bufferPos - keyLength - 1;
		listener.startElement(elem);
		state = State::AFTER_VALUE;
		elem.keyLength = 0;
		bufferPos = 0;
	}

	void endElement(Element::Type type)
	{
		listener.endElement(type, stack.getLevel());
	}

	Error endArray();

	Error startValue(char c);

	Error processEscapeCharacters(char c);

	static char convertCodepointToCharacter(uint16_t num);

	Error endUnicodeCharacter(uint16_t codepoint);

	Error startObject()
	{
		startElement(Element::Type::Object);
		state = State::IN_OBJECT;
		return stack.push({true, 0}) ? Error::Ok : Error::StackFull;
	}

	Error startArray()
	{
		startElement(Element::Type::Array);
		state = State::IN_ARRAY;
		return stack.push({false, 0}) ? Error::Ok : Error::StackFull;
	}

	void endDocument()
	{
		listener.endElement(Element::Type::Document, 0);
		state = State::END_DOCUMENT;
	}

	Error endUnicodeSurrogateInterstitial();

	bool bufferContains(char c)
	{
		return memchr(buffer, c, bufferPos) != nullptr;
	}

	static unsigned getHexArrayAsDecimal(char hexArray[], unsigned length);

	Error processUnicodeCharacter(char c);

	Error endObject();

private:
	struct StackItem {
		uint8_t isObject : 1; ///< Can only be an object or an array
		uint8_t index : 7;	///< Counts child items
	};

	static_assert(sizeof(StackItem) == 1);

	Listener& listener;
	State state = State::START_DOCUMENT;
	Stack<StackItem, 20> stack;

	bool doEmitWhitespace = false;
	static constexpr uint16_t BUFFER_MAX_LENGTH = 512;
	// Buffer contains key, followed by value data
	char buffer[BUFFER_MAX_LENGTH];
	uint8_t keyLength = 0;
	uint16_t bufferPos = 0; ///< Current write position in buffer

	char unicodeEscapeBuffer[10];
	uint8_t unicodeEscapeBufferPos = 0;

	char unicodeBuffer[10];
	uint8_t unicodeBufferPos = 0;
	int unicodeHighSurrogate = 0;
};

} // namespace JSON
