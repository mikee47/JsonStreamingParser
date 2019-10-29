#pragma once

namespace JSON
{
enum class Error {
	Ok,
	ColonExpected,
	OpeningBraceExpected,
	StringStartExpected,
	CommaOrClosingBraceExpected,
	CommaOrClosingBracketExpected,
	TrueExpected,
	FalseExpected,
	NullExpected,
	HexExpected,
	UnexpectedContentAfterDocument,
	UnexpectedEndOfString,
	NotInObject,
	NotInArray,
	UnescapedControl,
	MultipleDecimalPoints,
	MultipleExponents,
	DecimalPointInExponent,
	BadExponent,
	BadValue,
	BadEscapeChar,
	BadUnicodeEscapeChar,
	BufferFull,
	StackFull,
	InternalError,
};
}
