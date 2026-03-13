#include "rect2i.h"
#include "rect2.h"
#include <string>

std::string Rect2i::to_string() const
{
	return "[P: " + math_helpers::vec_to_string(position) + ", S: " + math_helpers::vec_to_string(size) + "]";
}

Rect2i::operator Rect2() const {
	return Rect2(position, size);
}

