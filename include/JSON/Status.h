#pragma once

#include <WString.h>

#define JSON_STATUS_MAP(XX)                                                                                            \
	XX(Ok)                                                                                                             \
	XX(EndOfDocument)                                                                                                  \
	XX(ColonExpected)                                                                                                  \
	XX(OpeningBraceExpected)                                                                                           \
	XX(StringStartExpected)                                                                                            \
	XX(CommaOrClosingBraceExpected)                                                                                    \
	XX(CommaOrClosingBracketExpected)                                                                                  \
	XX(TrueExpected)                                                                                                   \
	XX(FalseExpected)                                                                                                  \
	XX(NullExpected)                                                                                                   \
	XX(HexExpected)                                                                                                    \
	XX(UnexpectedContentAfterDocument)                                                                                 \
	XX(UnexpectedEndOfString)                                                                                          \
	XX(NotInObject)                                                                                                    \
	XX(NotInArray)                                                                                                     \
	XX(UnescapedControl)                                                                                               \
	XX(MultipleDecimalPoints)                                                                                          \
	XX(MultipleExponents)                                                                                              \
	XX(DecimalPointInExponent)                                                                                         \
	XX(BadExponent)                                                                                                    \
	XX(BadValue)                                                                                                       \
	XX(BadEscapeChar)                                                                                                  \
	XX(BadUnicodeEscapeChar)                                                                                           \
	XX(BufferFull)                                                                                                     \
	XX(StackFull)                                                                                                      \
	XX(InternalError)

namespace JSON
{
enum class Status {
#define XX(t) t,
	JSON_STATUS_MAP(XX)
#undef XX
};

String getStatusString(Status status);

} // namespace JSON
