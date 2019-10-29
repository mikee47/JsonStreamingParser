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

#include <WString.h>

namespace JSON
{
/**
 * @brief Identifies type and position of item in a parent object or array
 */
struct Container {
	uint8_t isObject : 1; ///< Can only be an object or an array
	uint8_t index : 7;	///< Counts child items
};

static_assert(sizeof(Container) == 1);

struct Element {
	enum class Type : uint8_t {
		Object,
		Array,
		Null,
		True,
		False,
		Number,
		String,
	};

	Container container = {true, 0};
	Type type;
	uint8_t level = 0; ///< Nesting level
	const char* key = nullptr;
	const char* value = nullptr;
	uint16_t keyLength = 0;
	uint16_t valueLength = 0;

	Element(Container container, Type type, unsigned level) : container(container), type(type), level(level)
	{
	}

	String getKey() const
	{
		return String(key, keyLength);
	}

	const char* getValue() const
	{
		switch(type) {
		case Type::Null:
		case Type::Object:
		case Type::Array:
			return nullptr;
		case Type::True:
			return "true";
		case Type::False:
			return "false";
		case Type::Number:
		case Type::String:
		default:
			return value;
		}
	}

	String getValueString() const
	{
		switch(type) {
		case Type::Number:
		case Type::String:
			return String(value, valueLength);
		default:
			return getValue();
		}
	}
};

static_assert(sizeof(Element) == 16);

} // namespace JSON
