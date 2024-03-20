#include "object.h"

Object::Object()
{
}

void Object::set_hp(int new_hp)
{
	hp = std::clamp(new_hp, 0, 100);
}
