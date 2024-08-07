/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 by Daniel Eichhorn
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * 	The above copyright notice and this permission notice shall be included in all
 * 	copies or substantial portions of the Software.
 *
 * 	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * 	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * 	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * 	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * 	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * 	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * 	SOFTWARE.
 *
 * 	See more at http://blog.squix.ch and https://github.com/squix78/json-streaming-parser
 *
 * @author Oct 2019 / 2024 mikee47 <mike@sillyhouse.net>
 *
 * Substantial rewrite
*/

#pragma once

#include "Listener.h"
#include "Status.h"
#include "Stack.h"
#include <Stream.h>

namespace JSON
{
/**
 * @brief Streaming parser
 * @tparam BUFSIZE Size of buffer must be large enough to contain the longest
 * key and value strings in the content being parsed.
 */
class StreamingParser
{
public:
	/**
	 * @brief Place a hard limit on nesting depth
	 */
	static constexpr uint8_t maxNesting{20};

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

	StreamingParser(char* buffer, uint16_t bufsize, Listener* listener, void* param = nullptr)
		: buffer(buffer), bufsize(bufsize), listener(listener), param(param)
	{
	}

	/**
	 * @brief Set the current listener
	 * @note Can change this at any time to redirect parsing output
	 */
	void setListener(Listener* listener)
	{
		this->listener = listener;
	}

	/**
	 * @brief Set parameter passed to listener
	 */
	void setParam(void* param)
	{
		this->param = param;
	}

	Status parse(const char* data, unsigned length);

	Status parse(Stream& stream);

	void reset();

	State getState() const
	{
		return state;
	}

private:
	Status parse(char c);

	// valid whitespace characters in JSON (from RFC4627 for JSON) include:
	// space, horizontal tab, line feed or new line, and carriage return.
	// thanks:
	// http://stackoverflow.com/questions/16042274/definition-of-whitespace-in-json
	static bool isWhiteSpace(char c)
	{
		return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
	}

	Status bufferChar(char c);

	Status startElement(Element::Type type);

	Status endElement(Element::Type type);

	Status endArray();

	Status startValue(char c);

	Status specialValue(char c, const char* tag, uint8_t taglen, Element::Type type, Status fail);

	Status processEscapeCharacters(char c);

	static char convertCodepointToCharacter(uint16_t num);

	Status endUnicodeCharacter(uint16_t codepoint);

	Status startObject();

	Status startArray();

	Status endUnicodeSurrogateInterstitial();

	bool bufferContains(char c);

	static unsigned getHexArrayAsDecimal(char hexArray[], unsigned length);

	Status processUnicodeCharacter(char c);

	Status endObject();

private:
	// Buffer contains key, followed by value data
	char* buffer;
	uint16_t bufsize;

	Listener* listener = nullptr;
	void* param = nullptr;
	State state = State::START_DOCUMENT;
	Stack<Container, maxNesting> stack;

	uint8_t keyLength = 0;  ///< Length of key, not including NUL terminator
	uint16_t bufferPos = 0; ///< Current write position in buffer

	char unicodeEscapeBuffer[10];
	uint8_t unicodeEscapeBufferPos = 0;

	char unicodeBuffer[10];
	uint8_t unicodeBufferPos = 0;
	int unicodeHighSurrogate = 0;
};

template <uint16_t BUFSIZE> class StaticStreamingParser : public StreamingParser
{
public:
	static_assert(BUFSIZE >= 32, "Buffer too small");

	StaticStreamingParser(Listener* listener, void* param = nullptr) : StreamingParser(buffer, BUFSIZE, listener, param)
	{
	}

private:
	char buffer[BUFSIZE];
};

} // namespace JSON
