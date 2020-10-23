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
#include <type_traits>

namespace JSON
{
/**
 * @brief Identifies type and position of item in a parent object or array
 */
struct Container {
	uint8_t isObject : 1; ///< Can only be an object or an array
	uint8_t index : 7;	///< Counts child items
};

static_assert(sizeof(Container) == 1, "Container size incorrect");

struct Element {
	enum class Type : uint8_t {
		Null,
		True,
		False,
		Number,
		String,
		Object,
		Array,
	};

	void* param{nullptr};
	Container container{true, 0};
	Type type = Type::Null;
	uint8_t level{0}; ///< Nesting level
	const char* key{nullptr};
	const char* value{nullptr};
	uint16_t keyLength{0};
	uint16_t valueLength{0};

	String getKey() const
	{
		return String(key, keyLength);
	}

	bool hasKey() const
	{
		return keyLength != 0;
	}

	bool keyIs(const char* key, unsigned length) const
	{
		if(length == keyLength) {
			return memcmp(this->key, key, length) == 0;
		} else {
			return false;
		}
	}

	bool keyIs(const char* key) const
	{
		return keyIs(key, strlen(key));
	}

	bool keyIs(const FlashString& key) const
	{
		return key.equals(this->key, keyLength);
	}

	template <typename T> inline typename std::enable_if<std::is_same<T, const char*>::value, T>::type as() const
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

	template <typename T> inline typename std::enable_if<std::is_same<T, String>::value, T>::type as() const
	{
		switch(type) {
		case Type::Number:
		case Type::String:
			return String(value, valueLength);
		default:
			return as<const char*>();
		}
	}

	template <typename T> inline typename std::enable_if<std::is_integral<T>::value, T>::type as() const
	{
		switch(type) {
		case Type::Null:
		case Type::Object:
		case Type::Array:
			return 0;
		case Type::True:
			return 1;
		case Type::False:
			return 0;
		case Type::Number:
		case Type::String:
		default:
			return value ? atoi(value) : 0;
		}
	}

	template <typename T> inline typename std::enable_if<std::is_floating_point<T>::value, T>::type as() const
	{
		switch(type) {
		case Type::Null:
		case Type::Object:
		case Type::Array:
			return 0;
		case Type::True:
			return 1;
		case Type::False:
			return 0;
		case Type::Number:
		case Type::String:
		default:
			return value ? atof(value) : 0;
		}
	}

	template <typename T> inline typename std::enable_if<std::is_same<T, bool>::value, T>::type as() const
	{
		switch(type) {
		case Type::Null:
		case Type::Object:
		case Type::Array:
			return false;
		case Type::True:
			return true;
		case Type::False:
			return false;
		case Type::Number:
			return as<int>() != 0;
		case Type::String:
		default:
			return valueLength != 0;
		}
	}
};

static_assert(sizeof(Element) == 20, "Element size incorrect");

} // namespace JSON
