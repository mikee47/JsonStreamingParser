#include "include/JSON/Status.h"

#include <Data/CStringArray.h>

#define XX(t) #t "\0"
DEFINE_FSTR_LOCAL(statusStrings, JSON_STATUS_MAP(XX));
#undef XX

namespace JSON
{
String getStatusString(Status status)
{
	return CStringArray(statusStrings)[unsigned(status)];
}

} // namespace JSON
