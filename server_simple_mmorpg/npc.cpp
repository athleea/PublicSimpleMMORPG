#include "npc.h"
#include "global.h"

using namespace std;

NPC::NPC(int id, short x, short y, NPC_TYPE t) : Object(id, x, y)
{
	type = t;
	home_x = x;
	home_y = y;
	is_active = false;
	state = STATE::INGAME;
}

void NPC::respawn()
{
	is_active = false;
	is_dead = false;
	hp = 100;
}

void NPC::take_damage(const int power)
{
	hp -= power;
	if (hp <= 0) {
		is_dead = true;
	}
}

void NPC::set_default()
{
	is_active = false;
	can_move = true;
	can_attack = true;
	is_dead = false;
	hp = 100;
	target_player = nullptr;

	unordered_set<int> old_vl;
	for (auto& obj : g_server.objects | views::take(MAX_USER)) {
		if (obj == nullptr) continue;
		if (STATE::INGAME != obj->state) continue;
		if (true == g_server.in_range(shared_from_this(), obj))
			old_vl.insert(obj->id);
	}

	x = home_x;
	y = home_y;

	update_player_view_list(old_vl);
}

void NPC::random_move()
{
	int old_x = x;
	int old_y = y;
	switch (rand() % 4) {
		case 0: old_x++; break;
		case 1: old_x--; break;
		case 2: old_y++; break;
		case 3: old_y--; break;
	}

	int nx = clamp(old_x, home_x - 10, x + home_x + 10 - 1);
	int ny = clamp(old_y, home_y - 10, y + home_y + 10 - 1);

	do_move(nx, ny);
}

void NPC::move_to_target()
{
	int nx = x;
	int ny = y;
	int tx = target_player->x;
	int ty = target_player->y;

	// a* ±Ê√£±‚
	if (nx < tx) {
		nx++;
	}
	else if (nx > tx) {
		nx--;
	}
	else if (ny < ty) {
		ny++;
	}
	else if (ny > ty) {
		ny--;
	}

	do_move(nx, ny);
}

void NPC::do_move(const short target_x, const short target_y)
{
	unordered_set<int> old_vl;
	for (auto& obj : g_server.objects | views::take(MAX_USER)) {
		if (obj == nullptr) continue;
		if (STATE::INGAME != obj->state) continue;
		if (true == g_server.in_range(shared_from_this(), obj))
			old_vl.insert(obj->id);
	}

	x = target_x;
	y = target_y;

	update_player_view_list(old_vl);

	TIMER_EVENT ev{ id, chrono::steady_clock::now() + 1s, TIMER_TYPE::NPC_ACTION, -1 };
	g_timer.push_event(ev);
}

void NPC::do_attack()
{
	auto player = g_server.cast_session(target_player->id);
	if (player == nullptr) return;

	player->set_hp(player->hp - power);

	bool old_state = true;
	if (true == atomic_compare_exchange_strong(&player->can_heal, &old_state, false)) {
		TIMER_EVENT c_ev{ player->id, chrono::steady_clock::now() + 5s, TIMER_TYPE::PLAYER_ACTION, 0 };
		c_ev.skill_type = PLAYER_ACTION::HEAL;
		g_timer.push_event(c_ev);
	}


	if (player->hp <= 0) {
		target_player = nullptr;
		player->set_dead();

		player->vl.lock();
		auto cur_view_list = player->view_list;
		player->view_list.clear();
		player->vl.unlock();

		for (const int obj_id : cur_view_list) {
			if (true == g_server.is_npc(obj_id)) continue;
			if (nullptr == g_server.objects[obj_id]) continue;
			if (STATE::INGAME != g_server.objects[obj_id]->state) continue;
			auto cl = g_server.cast_session(obj_id);
			if (cl == nullptr) continue;

			cl->send_remove_player_packet(player->id);
			player->send_remove_player_packet(obj_id);
		}

		TIMER_EVENT ev{ player->id, chrono::steady_clock::now() + 5s, TIMER_TYPE::RESPAWN, 0 };
		g_timer.push_event(ev);
	}
	else {
		for (auto& obj : g_server.objects | views::take(MAX_USER)) {
			if (nullptr == obj) continue;
			if (obj->state != STATE::INGAME) continue;
			if (false == g_server.in_range(obj, shared_from_this())) continue;

			auto cl = reinterpret_pointer_cast<Session>(obj);

			cl->send_hit_packet(id, player->id, power);
		}
	}

	player->send_change_stat_packet();
}

void NPC::update_player_view_list(const std::unordered_set<int>& old_vl)
{
	unordered_set<int> new_vl;
	for (auto& obj : g_server.objects | views::take(MAX_USER)) {
		if (obj == nullptr) continue;
		if (STATE::INGAME != obj->state) continue;
		if (true == g_server.in_range(shared_from_this(), obj))
			new_vl.insert(obj->id);
	}

	for (auto pl : new_vl) {
		auto player = g_server.cast_session(pl);
		if (player == nullptr) continue;
		if (0 == old_vl.count(pl)) {
			player->send_add_player_packet(shared_from_this());
		}
		else {
			player->send_move_packet(shared_from_this());
		}
	}

	for (const int pl : old_vl) {
		if (0 == new_vl.count(pl)) {
			auto player = g_server.cast_session(pl);
			if (player == nullptr) continue;

			player->vl.lock();
			if (0 != player->view_list.count(id)) {
				player->vl.unlock();
				player->send_remove_player_packet(id);
			}
			else {
				player->vl.unlock();
			}
		}
	}
}