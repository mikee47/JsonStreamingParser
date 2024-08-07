#include "include/JSON/Status.h"

namespace JSON
{
String toString(Status status)
{
	switch(status) {
#define XX(tag)                                                                                                        \
	case Status::tag:                                                                                                  \
		return F(#tag);

		JSON_STATUS_MAP(XX)

#undef XX
	}
	return nullptr;
}

} // namespace JSON
