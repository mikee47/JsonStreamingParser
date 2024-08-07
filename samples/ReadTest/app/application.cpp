#include <SmingCore.h>
#include <JSON/StreamingParser.h>
#include <JSON/BasicListener.h>
#include <FlashString/Stream.hpp>

IMPORT_FSTR(testFile, PROJECT_DIR "/files/test.json")

bool readTest(Stream& input, Print& output)
{
	BasicListener listener(output);
	JSON::StaticStreamingParser<128> parser(&listener);
	auto status = parser.parse(input);
	debug_i("Parser returned '%s'", JSON::toString(status).c_str());
	return status == JSON::Status::EndOfDocument;
}

void init()
{
	Serial.begin(SERIAL_BAUD_RATE);
	Serial.systemDebugOutput(true);

	FSTR::Stream fs(testFile);
	readTest(fs, Serial);

#ifdef ARCH_HOST
	System.restart();
#endif
}
