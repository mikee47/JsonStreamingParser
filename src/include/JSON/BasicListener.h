#pragma once

#include <Print.h>
#include <JSON/StreamingParser.h>

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
		m_puts(element.container.isObject ? "OBJ" : "ARR");
		m_printf("(%u) ", element.container.index);
		switch(element.type) {
		case Element::Type::Object:
		case Element::Type::Array:
			if(element.keyLength > 0) {
				output.write(element.key, element.keyLength);
				output.print(": ");
			}
			output.println(element.type == Element::Type::Object ? '{' : '[');
			break;

		default:
			output << element.key << ": " << toString(element.type) << " = " << element.value << endl;
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
		auto n = level * 2;
		char ws[n];
		memset(ws, ' ', n);
		m_nputs(ws, n);
	}

	Print& output;
};
