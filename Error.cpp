#include "include/JSON/Error.h"
#include <Data/CStringArray.h>

#define XX(t) #t "\0"
DEFINE_FSTR_LOCAL(errorStrings, JSON_ERROR_MAP(XX));
#undef XX

namespace JSON
{
String getErrorString(Error error)
{
	return CStringArray(errorStrings)[unsigned(error)];
}

} // namespace JSON
