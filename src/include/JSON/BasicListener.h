#pragma once

#include <Print.h>
#include "Listener.h"
#include <Data/Format/Standard.h>

/**
 * @brief Listener implementation to output formatted JSON to a stream
 */
class BasicListener : public JSON::Listener
{
public:
	using Element = JSON::Element;

	BasicListener(Print& output) : output(output)
	{
	}

	/* Listener methods */

	bool startElement(const Element& element) override
	{
		indentLine(element.level);
		output << (element.container.isObject ? "OBJ" : "ARR") << '(' << element.container.index << ") ";
		if(element.keyLength > 0) {
			output.write(element.key, element.keyLength);
			output.print(": ");
		}
		String value(element.value, element.valueLength);
		switch(element.type) {
		case Element::Type::Object:
			output.println('{');
			return true;
		case Element::Type::Array:
			output.println('[');
			return true;
		case Element::Type::String:
			Format::standard.quote(value);
			[[fallthrough]];
		default:
			output << toString(element.type) << " = " << value << endl;
		}

		// Continue parsing
		return true;
	}

	bool endElement(const Element& element) override
	{
		indentLine(element.level);
		switch(element.type) {
		case Element::Type::Object:
			output.println('}');
			break;

		case Element::Type::Array:
			output.println(']');
			break;

		default:; //
		}

		// Continue parsing
		return true;
	}

private:
	void indentLine(unsigned level)
	{
		output << String().pad(level * 2);
	}

	Print& output;
};
