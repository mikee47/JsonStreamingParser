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

#include "include/JSON/StreamingParser.h"

namespace JSON
{
Status StreamingParser::bufferChar(char c)
{
	if(bufferPos >= bufsize - 1) {
		return Status::BufferFull;
	}

	buffer[bufferPos] = c;
	++bufferPos;
	return Status::Ok;
}

Status StreamingParser::startObject()
{
	auto status = startElement(Element::Type::Object);
	if(status == Status::Ok) {
		state = State::IN_OBJECT;
		status = stack.push({true, 0}) ? Status::Ok : Status::StackFull;
	}
	return status;
}

Status StreamingParser::startArray()
{
	auto status = startElement(Element::Type::Array);
	if(status == Status::Ok) {
		state = State::IN_ARRAY;
		status = stack.push({false, 0}) ? Status::Ok : Status::StackFull;
	}
	return status;
}

bool StreamingParser::bufferContains(char c)
{
	return memchr(&buffer[keyLength], c, bufferPos - keyLength) != nullptr;
}

void StreamingParser::reset()
{
	state = State::START_DOCUMENT;
	stack.clear();
	keyLength = 0;
	bufferPos = 0;
	unicodeEscapeBufferPos = 0;
	unicodeBufferPos = 0;
}

Status StreamingParser::parse(const char* data, unsigned length)
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

Status StreamingParser::parse(Stream& stream)
{
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

Status StreamingParser::parse(char c)
{
	switch(state) {
	case State::IN_KEY:
	case State::IN_STRING:
		if(isWhiteSpace(c)) {
			bufferChar(c);
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

Status StreamingParser::startElement(Element::Type type)
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

Status StreamingParser::endElement(Element::Type type)
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

Status StreamingParser::startValue(char c)
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

Status StreamingParser::endArray()
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

Status StreamingParser::endObject()
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

Status StreamingParser::processEscapeCharacters(char c)
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

Status StreamingParser::processUnicodeCharacter(char c)
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

unsigned StreamingParser::getHexArrayAsDecimal(char hexArray[], unsigned length)
{
	unsigned result = 0;
	for(unsigned i = 0; i < length; i++) {
		result = (result << 4) | unhex(hexArray[i]);
	}
	return result;
}

Status StreamingParser::endUnicodeSurrogateInterstitial()
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

Status StreamingParser::endUnicodeCharacter(uint16_t codepoint)
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
