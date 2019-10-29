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

#include "include/JSON/StreamingParser.h"
#include <ctype.h>
#include <stringutil.h>

namespace JSON
{
void StreamingParser::reset()
{
	state = State::START_DOCUMENT;
	stack.clear();
	keyLength = 0;
	bufferPos = 0;
	unicodeEscapeBufferPos = 0;
	unicodeBufferPos = 0;
}

Error StreamingParser::parse(const char* data, unsigned length)
{
	while(length--) {
		auto err = parse(*data++);
		if(err != Error::Ok) {
			return err;
		}
	}

	return Error::Ok;
}

Error StreamingParser::parse(char c)
{
	switch(state) {
	case State::IN_KEY:
	case State::IN_STRING:
		if(isWhiteSpace(c)) {
			return Error::Ok;
		}

		if(c == '"') {
			if(state == State::IN_KEY) {
				keyLength = bufferPos;
				buffer[bufferPos] = '\0';
				++bufferPos;
				state = State::END_KEY;
			} else {
				startElement(Element::Type::String);
			}
			return Error::Ok;
		}

		if(c == '\\') {
			state = State::START_ESCAPE;
			return Error::Ok;
		}

		if((c < 0x1f) || (c == 0x7f)) {
			// Unescaped control character encountered
			return Error::UnescapedControl;
		}

		bufferChar(c);
		return Error::Ok;

	case State::IN_ARRAY:
		if(c == ']') {
			return endArray();
		} else {
			return startValue(c);
		}

	case State::IN_OBJECT:
		if(c == '}') {
			return endObject();
		}

		if(c == '"') {
			state = State::IN_KEY;
			return Error::Ok;
		}

		// Start of string expected for object key
		return Error::StringStartExpected;

	case State::END_KEY:
		if(c != ':') {
			// Expected ':' after key
			return Error::ColonExpected;
		} else {
			state = State::AFTER_KEY;
			return Error::Ok;
		}

	case State::AFTER_KEY:
		return startValue(c);

	case State::START_ESCAPE:
		if(isWhiteSpace(c)) {
			return Error::Ok;
		} else {
			return processEscapeCharacters(c);
		}

	case State::UNICODE:
		if(isWhiteSpace(c)) {
			return Error::Ok;
		} else {
			return processUnicodeCharacter(c);
		}

	case State::UNICODE_SURROGATE:
		unicodeEscapeBuffer[unicodeEscapeBufferPos] = c;
		unicodeEscapeBufferPos++;
		if(unicodeEscapeBufferPos == 2) {
			return endUnicodeSurrogateInterstitial();
		} else {
			return Error::Ok;
		}

	case State::AFTER_VALUE:
		switch(stack.peek()) {
		case Item::OBJECT:
			if(c == '}') {
				return endObject();
			} else if(c == ',') {
				state = State::IN_OBJECT;
				return Error::Ok;
			} else {
				// Expected ',' or '}'
				return Error::CommaOrClosingBraceExpected;
			}

		case Item::ARRAY:
			if(c == ']') {
				return endArray();
			} else if(c == ',') {
				state = State::IN_ARRAY;
				return Error::Ok;
			} else {
				// Expected ',' or ']' while parsing array
				return Error::CommaOrClosingBracketExpected;
			}

		default:
			// Finished a literal, but unclear what state to move to
			return Error::InternalError;
		}

	case State::IN_NUMBER:
		if(isWhiteSpace(c)) {
			return Error::Ok;
		}

		if(c >= '0' && c <= '9') {
			return bufferChar(c);
		}

		if(c == '.') {
			if(bufferContains('.')) {
				// Cannot have multiple decimal points in a number
				return Error::MultipleDecimalPoints;
			} else if(bufferContains('e')) {
				// Cannot have a decimal point in an exponent
				return Error::DecimalPointInExponent;
			} else {
				return bufferChar(c);
			}
		}

		if(c == 'e' || c == 'E') {
			if(bufferContains('e')) {
				// Cannot have multiple exponents in a number
				return Error::MultipleExponents;
			} else {
				return bufferChar(c);
			}
		}

		if(c == '+' || c == '-') {
			char last = buffer[bufferPos - 1];
			if(last != 'e' && last != 'E') {
				// Can only have '+' or '-' after the 'e' or 'E' in a number
				return Error::BadExponent;
			} else {
				return bufferChar(c);
			}
		}

		//float result = 0.0;
		//if (doesCharArrayContain(buffer, bufferPos, '.')) {
		//  result = value.toFloat();
		//} else {
		// needed special treatment in php, maybe not in Java and c
		//  result = value.toFloat();
		//}
		startElement(Element::Type::Number);
		// we have consumed one beyond the end of the number
		return parse(c);

	case State::IN_TRUE:
		bufferChar(c);
		if(bufferPos == 4) {
			if(memcmp(buffer, "true", 4) != 0) {
				return Error::TrueExpected;
			}
			startElement(Element::Type::True);
		}
		return Error::Ok;

	case State::IN_FALSE:
		bufferChar(c);
		if(bufferPos == 5) {
			if(memcmp(buffer, "false", 5) != 0) {
				return Error::FalseExpected;
			}
			startElement(Element::Type::False);
		}
		return Error::Ok;

	case State::IN_NULL:
		bufferChar(c);
		if(bufferPos == 4) {
			if(memcmp(buffer, "null", 4) != 0) {
				return Error::NullExpected;
			}
			startElement(Element::Type::Null);
		}
		return Error::Ok;

	case State::START_DOCUMENT:
		if(isWhiteSpace(c)) {
			return Error::Ok;
		}

		listener.startElement(Element(Element::Type::Document, 0));
		if(c == '[') {
			return startArray();
		} else if(c == '{') {
			return startObject();
		} else {
			// Document must start with object or array
			return Error::OpeningBraceExpected;
		}

	case State::END_DOCUMENT:
		return Error::UnexpectedContentAfterDocument;

	default:
		// Reached an unknown state
		assert(false);
		return Error::InternalError;
	}

	return Error::Ok;
}

Error StreamingParser::startValue(char c)
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
		return Error::Ok;
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
		return Error::BadValue;
	}
}

Error StreamingParser::endArray()
{
	auto popped = stack.pop();
	if(popped != Item::ARRAY) {
		// "Unexpected end of array encountered.");
		return Error::NotInArray;
	}

	endElement(Element::Type::Array);
	state = State::AFTER_VALUE;
	if(stack.isEmpty()) {
		endDocument();
	}

	return Error::Ok;
}

Error StreamingParser::endObject()
{
	auto popped = stack.pop();
	if(popped != Item::OBJECT) {
		// Unexpected end of object encountered
		return Error::NotInObject;
	}

	endElement(Element::Type::Object);
	state = State::AFTER_VALUE;
	if(stack.isEmpty()) {
		endDocument();
	}

	return Error::Ok;
}

Error StreamingParser::processEscapeCharacters(char c)
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
		return Error::Ok;
	default:
		// Expected escaped character after backslash
		return Error::BadEscapeChar;
	}

	if(state != State::UNICODE) {
		state = State::IN_STRING;
	}

	return bufferChar(c);
}

Error StreamingParser::processUnicodeCharacter(char c)
{
	if(!isxdigit(c)) {
		// Expected hex character for escaped Unicode character
		return Error::HexExpected;
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

	return Error::Ok;
}

unsigned StreamingParser::getHexArrayAsDecimal(char hexArray[], unsigned length)
{
	unsigned result = 0;
	for(unsigned i = 0; i < length; i++) {
		result = (result << 4) | unhex(hexArray[i]);
	}
	return result;
}

Error StreamingParser::endUnicodeSurrogateInterstitial()
{
	char unicodeEscape = unicodeEscapeBuffer[unicodeEscapeBufferPos - 1];
	if(unicodeEscape != 'u') {
		// throw new ParsingError($this->_line_number, $this->_char_number,
		// "Expected '\\u' following a Unicode high surrogate. Got: " .
		// $unicode_escape);
		return Error::BadUnicodeEscapeChar;
	}
	unicodeBufferPos = 0;
	unicodeEscapeBufferPos = 0;
	state = State::UNICODE;
	return Error::Ok;
}

Error StreamingParser::endUnicodeCharacter(uint16_t codepoint)
{
	unicodeBufferPos = 0;
	unicodeHighSurrogate = -1;
	state = State::IN_STRING;
	return bufferChar(convertCodepointToCharacter(codepoint));
}

char StreamingParser::convertCodepointToCharacter(uint16_t num)
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
