#include "include/JSON/Element.h"

String toString(JSON::Element::Type type)
{
	using Type = JSON::Element::Type;
	switch(type) {
#define XX(tag)                                                                                                        \
	case Type::tag:                                                                                                    \
		return F(#tag);
		JSON_ELEMENT_TYPE_MAP(XX)
	}
	return nullptr;
}
