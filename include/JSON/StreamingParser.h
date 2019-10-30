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
 * @author Oct 2019 mikee47 <mike@sillyhouse.net>
 *
 * Substantial rewrite and implement as class template
*/

#pragma once

#include "Listener.h"
#include "Status.h"
#include "Stack.h"
#include <Data/Stream/DataSourceStream.h>
#include <string.h>
#include <ctype.h>
#include <stringutil.h>

namespace JSON
{
/**
 * @brief Streaming parser
 * @tparam BUFSIZE Size of buffer must be large enough to contain the longest
 * key and value strings in the content being parsed.
 */
template <size_t BUFSIZE> class StreamingParser
{
public:
	// Arbitrary minimum size
	static_assert(BUFSIZE >= 32, "StreamingParser buffer needs to be larger");

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

	StreamingParser() = default;

	StreamingParser(Listener* listener, void* param = nullptr) : listener(listener), param(param)
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

	Status parse(IDataSourceStream& stream);

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

	Status bufferChar(char c)
	{
		if(bufferPos >= BUFSIZE - 1) {
			return Status::BufferFull;
		}

		buffer[bufferPos] = c;
		++bufferPos;
		return Status::Ok;
	}

	Status startElement(Element::Type type);

	Status endElement(Element::Type type);

	Status endArray();

	Status startValue(char c);

	Status processEscapeCharacters(char c);

	static char convertCodepointToCharacter(uint16_t num);

	Status endUnicodeCharacter(uint16_t codepoint);

	Status startObject()
	{
		auto status = startElement(Element::Type::Object);
		if(status == Status::Ok) {
			state = State::IN_OBJECT;
			status = stack.push({true, 0}) ? Status::Ok : Status::StackFull;
		}
		return status;
	}

	Status startArray()
	{
		auto status = startElement(Element::Type::Array);
		if(status == Status::Ok) {
			state = State::IN_ARRAY;
			status = stack.push({false, 0}) ? Status::Ok : Status::StackFull;
		}
		return status;
	}

	Status endUnicodeSurrogateInterstitial();

	bool bufferContains(char c)
	{
		return memchr(buffer, c, bufferPos) != nullptr;
	}

	static unsigned getHexArrayAsDecimal(char hexArray[], unsigned length);

	Status processUnicodeCharacter(char c);

	Status endObject();

private:
	Listener* listener = nullptr;
	void* param = nullptr;
	State state = State::START_DOCUMENT;
	Stack<Container, 20> stack;

	bool doEmitWhitespace = false;
	// Buffer contains key, followed by value data
	char buffer[BUFSIZE];
	uint8_t keyLength = 0;
	uint16_t bufferPos = 0; ///< Current write position in buffer

	char unicodeEscapeBuffer[10];
	uint8_t unicodeEscapeBufferPos = 0;

	char unicodeBuffer[10];
	uint8_t unicodeBufferPos = 0;
	int unicodeHighSurrogate = 0;
};

template <size_t BUFSIZE> void StreamingParser<BUFSIZE>::reset()
{
	state = State::START_DOCUMENT;
	stack.clear();
	keyLength = 0;
	bufferPos = 0;
	unicodeEscapeBufferPos = 0;
	unicodeBufferPos = 0;
}

template <size_t BUFSIZE> Status StreamingParser<BUFSIZE>::parse(const char* data, unsigned length)
{
	while(length != 0) {
		auto status = parse(*data++);
		--length;
		if(status != Status::Ok) {
			return status;
		}
	}

	return Status::Ok;
}

template <size_t BUFSIZE> Status StreamingParser<BUFSIZE>::parse(IDataSourceStream& stream)
{
	if(!stream.isValid()) {
		return Status::InvalidStream;
	}

	char buffer[64];
	int len;
	while((len = stream.readBytes(buffer, sizeof(buffer))) != 0) {
		auto status = parse(buffer, len);
		if(status != Status::Ok) {
			return status;
		}
	}

	return Status::NoMoreData;
}

template <size_t BUFSIZE> Status StreamingParser<BUFSIZE>::parse(char c)
{
	switch(state) {
	case State::IN_KEY:
	case State::IN_STRING:
		if(isWhiteSpace(c)) {
			return Status::Ok;
		} else if(c == '"') {
			if(state == State::IN_KEY) {
				keyLength = bufferPos;
				buffer[bufferPos] = '\0';
				++bufferPos;
				state = State::END_KEY;
				return Status::Ok;
			} else {
				return startElement(Element::Type::String);
			}
		} else if(c == '\\') {
			state = State::START_ESCAPE;
			return Status::Ok;
		} else if((c < 0x1f) || (c == 0x7f)) {
			// Unescaped control character encountered
			return Status::UnescapedControl;
		} else {
			bufferChar(c);
			return Status::Ok;
		}

	case State::IN_ARRAY:
		if(isWhiteSpace(c)) {
			return Status::Ok;
		} else if(c == ']') {
			return endArray();
		} else {
			return startValue(c);
		}

	case State::IN_OBJECT:
		if(isWhiteSpace(c)) {
			return Status::Ok;
		} else if(c == '}') {
			return endObject();
		} else if(c == '"') {
			state = State::IN_KEY;
			return Status::Ok;
		} else {
			// Start of string expected for object key
			return Status::StringStartExpected;
		}

	case State::END_KEY:
		if(isWhiteSpace(c)) {
			return Status::Ok;
		} else if(c != ':') {
			// Expected ':' after key
			return Status::ColonExpected;
		} else {
			state = State::AFTER_KEY;
			return Status::Ok;
		}

	case State::AFTER_KEY:
		if(isWhiteSpace(c)) {
			return Status::Ok;
		} else {
			return startValue(c);
		}

	case State::START_ESCAPE:
		return processEscapeCharacters(c);

	case State::UNICODE:
		return processUnicodeCharacter(c);

	case State::UNICODE_SURROGATE:
		unicodeEscapeBuffer[unicodeEscapeBufferPos] = c;
		unicodeEscapeBufferPos++;
		if(unicodeEscapeBufferPos == 2) {
			return endUnicodeSurrogateInterstitial();
		} else {
			return Status::Ok;
		}

	case State::AFTER_VALUE:
		if(isWhiteSpace(c)) {
			return Status::Ok;
		} else if(stack.peek().isObject) {
			if(c == '}') {
				return endObject();
			} else if(c == ',') {
				state = State::IN_OBJECT;
				return Status::Ok;
			} else {
				// Expected ',' or '}'
				return Status::CommaOrClosingBraceExpected;
			}
		} else {
			if(c == ']') {
				return endArray();
			} else if(c == ',') {
				state = State::IN_ARRAY;
				return Status::Ok;
			} else {
				// Expected ',' or ']' while parsing array
				return Status::CommaOrClosingBracketExpected;
			}
		}

	case State::IN_NUMBER:
		if(c >= '0' && c <= '9') {
			return bufferChar(c);
		} else if(c == '.') {
			if(bufferContains('.')) {
				// Cannot have multiple decimal points in a number
				return Status::MultipleDecimalPoints;
			} else if(bufferContains('e')) {
				// Cannot have a decimal point in an exponent
				return Status::DecimalPointInExponent;
			} else {
				return bufferChar(c);
			}
		} else if(c == 'e' || c == 'E') {
			if(bufferContains('e')) {
				// Cannot have multiple exponents in a number
				return Status::MultipleExponents;
			} else {
				return bufferChar(c);
			}
		} else if(c == '+' || c == '-') {
			char last = buffer[bufferPos - 1];
			if(last != 'e' && last != 'E') {
				// Can only have '+' or '-' after the 'e' or 'E' in a number
				return Status::BadExponent;
			} else {
				return bufferChar(c);
			}
		} else {
			//float result = 0.0;
			//if (doesCharArrayContain(buffer, bufferPos, '.')) {
			//  result = value.toFloat();
			//} else {
			// needed special treatment in php, maybe not in Java and c
			//  result = value.toFloat();
			//}
			auto status = startElement(Element::Type::Number);
			if(status == Status::Ok) {
				// we have consumed one beyond the end of the number
				status = parse(c);
			}
			return status;
		}

	case State::IN_TRUE:
		if(!isWhiteSpace(c)) {
			bufferChar(c);
			if(bufferPos == 4) {
				if(memcmp(buffer, "true", 4) != 0) {
					return Status::TrueExpected;
				}
				return startElement(Element::Type::True);
			}
		}
		return Status::Ok;

	case State::IN_FALSE:
		if(!isWhiteSpace(c)) {
			bufferChar(c);
			if(bufferPos == 5) {
				if(memcmp(buffer, "false", 5) != 0) {
					return Status::FalseExpected;
				}
				startElement(Element::Type::False);
			}
		}
		return Status::Ok;

	case State::IN_NULL:
		if(!isWhiteSpace(c)) {
			bufferChar(c);
			if(bufferPos == 4) {
				if(memcmp(buffer, "null", 4) != 0) {
					return Status::NullExpected;
				}
				startElement(Element::Type::Null);
			}
		}
		return Status::Ok;

	case State::START_DOCUMENT:
		if(isWhiteSpace(c)) {
			return Status::Ok;
		} else if(c == '[') {
			return startArray();
		} else if(c == '{') {
			return startObject();
		} else {
			// Document must start with object or array
			return Status::OpeningBraceExpected;
		}

	case State::END_DOCUMENT:
		if(isWhiteSpace(c)) {
			return Status::Ok;
		} else {
			return Status::UnexpectedContentAfterDocument;
		}

	default:
		// Reached an unknown state
		assert(false);
		return Status::InternalError;
	}

	return Status::Ok;
}

template <size_t BUFSIZE> Status StreamingParser<BUFSIZE>::startElement(Element::Type type)
{
	if(listener != nullptr) {
		buffer[bufferPos] = '\0';
		Element elem;
		elem.param = param;
		elem.type = type;
		elem.level = stack.getLevel();
		if(elem.level > 0) {
			auto& c = stack.peek();
			elem.container = c;
			++c.index;
		}
		elem.key = buffer;
		elem.keyLength = keyLength;
		elem.value = &buffer[keyLength + 1];
		elem.valueLength = bufferPos - keyLength - 1;
		if(!listener->startElement(elem)) {
			return Status::Cancelled;
		}
	}
	state = State::AFTER_VALUE;
	keyLength = 0;
	bufferPos = 0;
	return Status::Ok;
}

template <size_t BUFSIZE> Status StreamingParser<BUFSIZE>::endElement(Element::Type type)
{
	Status status = Status::Ok;
	if(listener != nullptr) {
		Element elem;
		elem.param = param;
		elem.type = type;
		elem.level = stack.getLevel();
		if(!listener->endElement(elem)) {
			status = Status::Cancelled;
		}
	}
	return status;
}

template <size_t BUFSIZE> Status StreamingParser<BUFSIZE>::startValue(char c)
{
	// Add an empty key if one wasn't provided
	if(bufferPos == 0) {
		keyLength = 0;
		buffer[bufferPos++] = '\0';
	}

	if(c == '[') {
		return startArray();
	} else if(c == '{') {
		return startObject();
	} else if(c == '"') {
		state = State::IN_STRING;
		return Status::Ok;
	} else if(isdigit(c) || c == '-') {
		state = State::IN_NUMBER;
		return bufferChar(c);
	} else if(c == 't') {
		state = State::IN_TRUE;
		return bufferChar(c);
	} else if(c == 'f') {
		state = State::IN_FALSE;
		return bufferChar(c);
	} else if(c == 'n') {
		state = State::IN_NULL;
		return bufferChar(c);
	} else {
		// Unexpected character for value
		return Status::BadValue;
	}
}

template <size_t BUFSIZE> Status StreamingParser<BUFSIZE>::endArray()
{
	if(stack.pop().isObject) {
		// "Unexpected end of array encountered.");
		return Status::NotInArray;
	}

	endElement(Element::Type::Array);
	state = State::AFTER_VALUE;
	if(stack.isEmpty()) {
		state = State::END_DOCUMENT;
		return Status::EndOfDocument;
	}

	return Status::Ok;
}

template <size_t BUFSIZE> Status StreamingParser<BUFSIZE>::endObject()
{
	if(!stack.pop().isObject) {
		// Unexpected end of object encountered
		return Status::NotInObject;
	}

	endElement(Element::Type::Object);
	state = State::AFTER_VALUE;
	if(stack.isEmpty()) {
		state = State::END_DOCUMENT;
		return Status::EndOfDocument;
	}

	return Status::Ok;
}

template <size_t BUFSIZE> Status StreamingParser<BUFSIZE>::processEscapeCharacters(char c)
{
	switch(c) {
	case '"':
	case '\\':
	case '/':
		break;
	case 'b':
		c = 0x08;
		break;
	case 'f':
		c = '\f';
		break;
	case 'n':
		c = '\n';
		break;
	case 'r':
		c = '\r';
		break;
	case 't':
		c = '\t';
		break;
	case 'u':
		state = State::UNICODE;
		return Status::Ok;
	default:
		// Expected escaped character after backslash
		return Status::BadEscapeChar;
	}

	if(state != State::UNICODE) {
		state = State::IN_STRING;
	}

	return bufferChar(c);
}

template <size_t BUFSIZE> Status StreamingParser<BUFSIZE>::processUnicodeCharacter(char c)
{
	if(!isxdigit(c)) {
		// Expected hex character for escaped Unicode character
		return Status::HexExpected;
	}

	unicodeBuffer[unicodeBufferPos] = c;
	unicodeBufferPos++;

	if(unicodeBufferPos == 4) {
		unsigned codepoint = getHexArrayAsDecimal(unicodeBuffer, unicodeBufferPos);
		return endUnicodeCharacter(codepoint);

		/*if (codepoint >= 0xD800 && codepoint < 0xDC00) {
        unicodeHighSurrogate = codepoint;
        unicodeBufferPos = 0;
        state = State::UNICODE_SURROGATE;
      } else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF) {
        if (unicodeHighSurrogate == -1) {
          // throw new ParsingError($this->_line_number,
          // $this->_char_number,
          // "Missing high surrogate for Unicode low surrogate.");
        }
        int combinedCodePoint = ((unicodeHighSurrogate - 0xD800) * 0x400) + (codepoint - 0xDC00) + 0x10000;
        endUnicodeCharacter(combinedCodePoint);
      } else if (unicodeHighSurrogate != -1) {
        // throw new ParsingError($this->_line_number,
        // $this->_char_number,
        // "Invalid low surrogate following Unicode high surrogate.");
        endUnicodeCharacter(codepoint);
      } else {
        endUnicodeCharacter(codepoint);
      }*/
	}

	return Status::Ok;
}

template <size_t BUFSIZE> unsigned StreamingParser<BUFSIZE>::getHexArrayAsDecimal(char hexArray[], unsigned length)
{
	unsigned result = 0;
	for(unsigned i = 0; i < length; i++) {
		result = (result << 4) | unhex(hexArray[i]);
	}
	return result;
}

template <size_t BUFSIZE> Status StreamingParser<BUFSIZE>::endUnicodeSurrogateInterstitial()
{
	char unicodeEscape = unicodeEscapeBuffer[unicodeEscapeBufferPos - 1];
	if(unicodeEscape != 'u') {
		// throw new ParsingError($this->_line_number, $this->_char_number,
		// "Expected '\\u' following a Unicode high surrogate. Got: " .
		// $unicode_escape);
		return Status::BadUnicodeEscapeChar;
	}
	unicodeBufferPos = 0;
	unicodeEscapeBufferPos = 0;
	state = State::UNICODE;
	return Status::Ok;
}

template <size_t BUFSIZE> Status StreamingParser<BUFSIZE>::endUnicodeCharacter(uint16_t codepoint)
{
	unicodeBufferPos = 0;
	unicodeHighSurrogate = -1;
	state = State::IN_STRING;
	return bufferChar(convertCodepointToCharacter(codepoint));
}

template <size_t BUFSIZE> char StreamingParser<BUFSIZE>::convertCodepointToCharacter(uint16_t num)
{
	if(num <= 0x7F) {
		return char(num);
	}
	// if(num<=0x7FF) return (char)((num>>6)+192) + (char)((num&63)+128);
	// if(num<=0xFFFF) return
	// chr((num>>12)+224).chr(((num>>6)&63)+128).chr((num&63)+128);
	// if(num<=0x1FFFFF) return
	// chr((num>>18)+240).chr(((num>>12)&63)+128).chr(((num>>6)&63)+128).chr((num&63)+128);
	return ' ';
}

} // namespace JSON
