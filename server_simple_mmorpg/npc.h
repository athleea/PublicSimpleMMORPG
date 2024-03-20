#pragma once
#include "stdafx.h"
#include "object.h"

class NPC : public Object {
public:
	NPC(int id, short x, short y, NPC_TYPE t);

	short home_x;
	short home_y;

	std::atomic_bool is_active;
	std::atomic_bool has_target;
	
	NPC_TYPE type;
	
	std::shared_ptr<Object> target_player = nullptr;

	void respawn();
	void take_damage(const int power);
	void set_default();

	void do_attack();
	void random_move();
	void move_to_target();

	void do_move(const short, const short);
	void update_player_view_list(const std::unordered_set<int>& old_view_list);
};