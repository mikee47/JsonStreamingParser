#include <Data/Stream/FileStream.h>
#include <JSON/StreamingParser.h>

class TestListener : public JSON::Listener
{
public:
	using Element = JSON::Element;

	bool startElement(const Element& element) override
	{
		indentLine(element.level);
		m_puts(element.container.isObject ? "OBJ" : "ARR");
		m_printf("(%u) ", element.container.index);
		switch(element.type) {
		case Element::Type::Object:
		case Element::Type::Array:
			if(element.keyLength > 0) {
				m_nputs(element.key, element.keyLength);
				m_puts(": ");
			}
			m_putc(element.type == Element::Type::Object ? '{' : '[');
			m_puts("\r\n");
			break;

		default:
			m_printf("%s: %s\r\n", element.key, element.value);
		}

		return true;
	}

	bool endElement(const Element& element) override
	{
		indentLine(element.level);
		switch(element.type) {
		case Element::Type::Object:
			m_puts("}\r\n");
			break;

		case Element::Type::Array:
			m_puts("]\r\n");
			break;

		default:; //
		}

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
};

bool readTest(IDataSourceStream& stream)
{
	TestListener listener;
	JSON::StreamingParser<128> parser(&listener);
	auto status = parser.parse(stream);
	m_printf("Parser returned '%s'\r\n", JSON::getStatusString(status).c_str());
	return status == JSON::Status::EndOfDocument;
}
