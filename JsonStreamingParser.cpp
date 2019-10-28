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

#include "JsonStreamingParser.h"
#include <ctype.h>
#include <stringutil.h>

void JsonStreamingParser::reset()
{
	state = State::START_DOCUMENT;
	bufferPos = 0;
	unicodeEscapeBufferPos = 0;
	unicodeBufferPos = 0;
	characterCounter = 0;
}

void JsonStreamingParser::parse(char c)
{
	// valid whitespace characters in JSON (from RFC4627 for JSON) include:
	// space, horizontal tab, line feed or new line, and carriage return.
	// thanks:
	// http://stackoverflow.com/questions/16042274/definition-of-whitespace-in-json
	if((c == ' ' || c == '\t' || c == '\n' || c == '\r') &&
	   !(state == State::IN_STRING || state == State::UNICODE || state == State::START_ESCAPE ||
		 state == State::IN_NUMBER || state == State::START_DOCUMENT)) {
		return;
	}
	switch(state) {
	case State::IN_STRING:
		if(c == '"') {
			auto popped = pop();
			if(popped == Obj::KEY) {
				sendKey();
			} else if(popped == Obj::STRING) {
				sendValue();
			} else {
				// throw new ParsingError($this->_line_number, $this->_char_number,
				// "Unexpected end of string.");
			}
		} else if(c == '\\') {
			state = State::START_ESCAPE;
		} else if((c < 0x1f) || (c == 0x7f)) {
			//throw new RuntimeException("Unescaped control character encountered: " + c + " at position" + characterCounter);
		} else {
			bufferChar(c);
		}
		break;
	case State::IN_ARRAY:
		if(c == ']') {
			endArray();
		} else {
			startValue(c);
		}
		break;
	case State::IN_OBJECT:
		if(c == '}') {
			endObject();
		} else if(c == '"') {
			push(Obj::KEY);
			state = State::IN_STRING;
		} else {
			//throw new RuntimeException("Start of string expected for object key. Instead got: " + c + " at position" + characterCounter);
		}
		break;
	case State::END_KEY:
		if(c != ':') {
			//throw new RuntimeException("Expected ':' after key. Instead got " + c + " at position" + characterCounter);
		}
		state = State::AFTER_KEY;
		break;
	case State::AFTER_KEY:
		startValue(c);
		break;
	case State::START_ESCAPE:
		processEscapeCharacters(c);
		break;
	case State::UNICODE:
		processUnicodeCharacter(c);
		break;
	case State::UNICODE_SURROGATE:
		unicodeEscapeBuffer[unicodeEscapeBufferPos] = c;
		unicodeEscapeBufferPos++;
		if(unicodeEscapeBufferPos == 2) {
			endUnicodeSurrogateInterstitial();
		}
		break;
	case State::AFTER_VALUE: {
		// not safe for size == 0!!!
		auto within = peek();
		if(within == Obj::OBJECT) {
			if(c == '}') {
				endObject();
			} else if(c == ',') {
				state = State::IN_OBJECT;
			} else {
				//throw new RuntimeException("Expected ',' or '}' while parsing object. Got: " + c + ". " + characterCounter);
			}
		} else if(within == Obj::ARRAY) {
			if(c == ']') {
				endArray();
			} else if(c == ',') {
				state = State::IN_ARRAY;
			} else {
				//throw new RuntimeException("Expected ',' or ']' while parsing array. Got: " + c + ". " + characterCounter);
			}
		} else {
			//throw new RuntimeException("Finished a literal, but unclear what state to move to. Last state: " + characterCounter);
		}
	} break;
	case State::IN_NUMBER:
		if(c >= '0' && c <= '9') {
			bufferChar(c);
		} else if(c == '.') {
			if(doesCharArrayContain(buffer, bufferPos, '.')) {
				//throw new RuntimeException("Cannot have multiple decimal points in a number. " + characterCounter);
			} else if(doesCharArrayContain(buffer, bufferPos, 'e')) {
				//throw new RuntimeException("Cannot have a decimal point in an exponent." + characterCounter);
			}
			bufferChar(c);
		} else if(c == 'e' || c == 'E') {
			if(doesCharArrayContain(buffer, bufferPos, 'e')) {
				//throw new RuntimeException("Cannot have multiple exponents in a number. " + characterCounter);
			}
			bufferChar(c);
		} else if(c == '+' || c == '-') {
			char last = buffer[bufferPos - 1];
			if(!(last == 'e' || last == 'E')) {
				//throw new RuntimeException("Can only have '+' or '-' after the 'e' or 'E' in a number." + characterCounter);
			}
			bufferChar(c);
		} else {
			//float result = 0.0;
			//if (doesCharArrayContain(buffer, bufferPos, '.')) {
			//  result = value.toFloat();
			//} else {
			// needed special treatment in php, maybe not in Java and c
			//  result = value.toFloat();
			//}
			sendValue();
			// we have consumed one beyond the end of the number
			parse(c);
		}
		break;
	case State::IN_TRUE:
		bufferChar(c);
		if(bufferPos == 4) {
			if(memcmp(buffer, "true", 4) == 0) {
				sendValue();
			} else {
				// throw new ParsingError($this->_line_number, $this->_char_number,
				// "Expected 'true'. Got: ".$true);
			}
		}
		break;
	case State::IN_FALSE:
		bufferChar(c);
		if(bufferPos == 5) {
			if(memcmp(buffer, "false", 5) == 0) {
				sendValue();
			} else {
				// throw new ParsingError($this->_line_number, $this->_char_number,
				// "Expected 'true'. Got: ".$true);
			}
		}
		break;
	case State::IN_NULL:
		bufferChar(c);
		if(bufferPos == 4) {
			if(memcmp(buffer, "null", 4) == 0) {
				sendValue();
			} else {
				// throw new ParsingError($this->_line_number, $this->_char_number,
				// "Expected 'true'. Got: ".$true);
			}
		}
		break;
	case State::START_DOCUMENT:
		listener.startDocument();
		if(c == '[') {
			startArray();
		} else if(c == '{') {
			startObject();
		} else {
			// throw new ParsingError($this->_line_number,
			// $this->_char_number,
			// "Document must start with object or array.");
		}
		break;
		//case State::DONE:
		// throw new ParsingError($this->_line_number, $this->_char_number,
		// "Expected end of document.");
		//default:
		// throw new ParsingError($this->_line_number, $this->_char_number,
		// "Internal error. Reached an unknown state: ".$this->_state);

	case State::DONE:; // Do nothing
	}
	characterCounter++;
}

void JsonStreamingParser::startValue(char c)
{
	if(c == '[') {
		startArray();
	} else if(c == '{') {
		startObject();
	} else if(c == '"') {
		push(Obj::STRING);
		state = State::IN_STRING;
	} else if(isdigit(c) || c == '-') {
		state = State::IN_NUMBER;
		bufferChar(c);
	} else if(c == 't') {
		state = State::IN_TRUE;
		bufferChar(c);
	} else if(c == 'f') {
		state = State::IN_FALSE;
		bufferChar(c);
	} else if(c == 'n') {
		state = State::IN_NULL;
		bufferChar(c);
	} else {
		// throw new ParsingError($this->_line_number, $this->_char_number,
		// "Unexpected character for value: ".$c);
	}
}

void JsonStreamingParser::endArray()
{
	auto popped = pop();
	if(popped != Obj::ARRAY) {
		// throw new ParsingError($this->_line_number, $this->_char_number,
		// "Unexpected end of array encountered.");
	}
	listener.endArray();
	state = State::AFTER_VALUE;
	if(stackPos == 0) {
		endDocument();
	}
}

void JsonStreamingParser::endObject()
{
	auto popped = pop();
	if(popped != Obj::OBJECT) {
		// throw new ParsingError($this->_line_number, $this->_char_number,
		// "Unexpected end of object encountered.");
	}
	listener.endObject();
	state = State::AFTER_VALUE;
	if(stackPos == 0) {
		endDocument();
	}
}

void JsonStreamingParser::processEscapeCharacters(char c)
{
	if(c == '"') {
		bufferChar('"');
	} else if(c == '\\') {
		bufferChar('\\');
	} else if(c == '/') {
		bufferChar('/');
	} else if(c == 'b') {
		bufferChar(0x08);
	} else if(c == 'f') {
		bufferChar('\f');
	} else if(c == 'n') {
		bufferChar('\n');
	} else if(c == 'r') {
		bufferChar('\r');
	} else if(c == 't') {
		bufferChar('\t');
	} else if(c == 'u') {
		state = State::UNICODE;
	} else {
		// throw new ParsingError($this->_line_number, $this->_char_number,
		// "Expected escaped character after backslash. Got: ".$c);
	}
	if(state != State::UNICODE) {
		state = State::IN_STRING;
	}
}

void JsonStreamingParser::processUnicodeCharacter(char c)
{
	if(!isxdigit(c)) {
		// throw new ParsingError($this->_line_number, $this->_char_number,
		// "Expected hex character for escaped Unicode character. Unicode parsed: "
		// . implode($this->_unicode_buffer) . " and got: ".$c);
	}

	unicodeBuffer[unicodeBufferPos] = c;
	unicodeBufferPos++;

	if(unicodeBufferPos == 4) {
		unsigned codepoint = getHexArrayAsDecimal(unicodeBuffer, unicodeBufferPos);
		endUnicodeCharacter(codepoint);
		return;
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
}

unsigned JsonStreamingParser::getHexArrayAsDecimal(char hexArray[], unsigned length)
{
	unsigned result = 0;
	for(unsigned i = 0; i < length; i++) {
		result = (result << 4) | unhex(hexArray[i]);
	}
	return result;
}

void JsonStreamingParser::endUnicodeSurrogateInterstitial()
{
	char unicodeEscape = unicodeEscapeBuffer[unicodeEscapeBufferPos - 1];
	if(unicodeEscape != 'u') {
		// throw new ParsingError($this->_line_number, $this->_char_number,
		// "Expected '\\u' following a Unicode high surrogate. Got: " .
		// $unicode_escape);
	}
	unicodeBufferPos = 0;
	unicodeEscapeBufferPos = 0;
	state = State::UNICODE;
}

void JsonStreamingParser::endDocument()
{
	listener.endDocument();
	state = State::DONE;
}

void JsonStreamingParser::startArray()
{
	listener.startArray();
	push(Obj::ARRAY);
	state = State::IN_ARRAY;
}

void JsonStreamingParser::startObject()
{
	listener.startObject();
	push(Obj::OBJECT);
	state = State::IN_OBJECT;
}

void JsonStreamingParser::endUnicodeCharacter(uint16_t codepoint)
{
	bufferChar(convertCodepointToCharacter(codepoint));
	unicodeBufferPos = 0;
	unicodeHighSurrogate = -1;
	state = State::IN_STRING;
}

char JsonStreamingParser::convertCodepointToCharacter(uint16_t num)
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
