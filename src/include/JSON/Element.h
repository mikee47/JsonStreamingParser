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

#define JSON_ELEMENT_TYPE_MAP(XX)                                                                                      \
	XX(Null)                                                                                                           \
	XX(True)                                                                                                           \
	XX(False)                                                                                                          \
	XX(Integer)                                                                                                        \
	XX(Number)                                                                                                         \
	XX(String)                                                                                                         \
	XX(Object)                                                                                                         \
	XX(Array)

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
#define XX(t) t,
		JSON_ELEMENT_TYPE_MAP(XX)
#undef XX
	};

	void* param;
	Container container;
	Type type;
	uint8_t level; ///< Nesting level
	const char* key;
	const char* value;
	uint16_t keyLength;
	uint16_t valueLength;

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
		}
		return false;
	}

	bool keyIs(const char* key) const
	{
		return strcmp(this->key, key) == 0;
	}

	bool keyIs(const FlashString& key) const
	{
		return key.equals(this->key, size_t(keyLength));
	}

	bool isNull() const
	{
		return type == Type::Null;
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
		case Type::Integer:
		case Type::Number:
		case Type::String:
			return value;
		}
		return nullptr;
	}

	template <typename T> inline typename std::enable_if<std::is_same<T, String>::value, T>::type as() const
	{
		switch(type) {
		case Type::Integer:
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
		case Type::String:
			return 0;
		case Type::True:
			return 1;
		case Type::False:
			return 0;
		case Type::Integer:
			return atoll(value);
		case Type::Number:
			return atof(value);
		}
		return 0;
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
		case Type::Integer:
		case Type::Number:
		case Type::String:
			return value ? atof(value) : 0;
		}
		return 0;
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
		case Type::Integer:
			return as<int>() != 0;
		case Type::Number:
			return as<float>() != 0.0;
		case Type::String:
			return valueLength != 0;
		}
		return false;
	}
};

} // namespace JSON

String toString(JSON::Element::Type type);
