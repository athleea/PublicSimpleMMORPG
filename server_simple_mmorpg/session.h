#pragma once
#include "stdafx.h"
#include "object.h"
#include "over_exp.h"



constexpr int MAX_PLAYER_HP = 100;

class Session : public Object {
public:
	OVER_EXP _recv_over;
	SOCKET socket;
	std::vector<char> my_buf;
	int	prev_remain;
	
	std::atomic_int exp;

	std::unordered_set <int> view_list;
	std::mutex vl;
	int last_move_time;

	int move_speed = 100; //ms

public:
	Session(int id, SOCKET s) : Object{ id }, socket{ s }
	{
		state = STATE::FREE;
		exp = 0;
		power = 50;

		prev_remain = 0;
		last_move_time = 0;

	}

	~Session() {}

	void add_exp(int e);
	void set_dead();
	void do_heal();
	void do_move(const char);

	void do_recv();
	void do_send(void* packet);
	
	void send_login_info_packet();
	void send_move_packet(const std::shared_ptr<Object>& other);
	void send_add_player_packet(const std::shared_ptr<Object>& other);
	void send_chat_packet(int c_id, const char* msg);
	void send_remove_player_packet(int c_id);
	void send_login_fail_packet();
	void send_change_stat_packet();
	void send_hit_packet(int attacker, int victim, int damage);
	void send_obtain_packet(int obtainer, int exp);
};