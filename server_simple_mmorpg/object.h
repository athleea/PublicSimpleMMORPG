#pragma once
#include "stdafx.h"

enum class STATE { FREE, ALLOC, INGAME };

class Object : public std::enable_shared_from_this<Object> {
public:
	int id = -1;
	short	x = 0, y = 0;
	std::atomic_int hp = 100;
	int level = 1;
	char name[NAME_SIZE] = "";
	int power = 0;

	std::atomic<STATE> state = STATE::FREE;
	std::atomic_bool is_dead = false;
	std::atomic_bool can_attack = true;
	std::atomic_bool can_move = true;
	std::atomic_bool can_heal = true;

public:
	Object();
	Object(int id) : id{ id } {};
	Object(int id, short new_x, short new_y) : id{ id }, x{ new_x }, y{ new_y } {};

	void set_hp(int new_hp);
};