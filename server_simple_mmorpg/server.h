#pragma once
#include "stdafx.h"
#include "session.h"
#include "npc.h"

constexpr int VIEW_RANGE = 15;

enum class PLAYER_ACTION { MOVE, ATTACK, HEAL, DEFAULT };
enum class TIMER_TYPE {
	RESPAWN,
	PLAYER_ACTION,
	NPC_ACTION,
	DEFAULT,
};
struct TIMER_EVENT {
	int obj_id;
	std::chrono::steady_clock::time_point wakeup_time;
	TIMER_TYPE event_id;
	int target_id;
	PLAYER_ACTION skill_type;

	constexpr bool operator < (const TIMER_EVENT& evt) const
	{
		return (wakeup_time > evt.wakeup_time);
	}
};


class Server {
private:
	std::unordered_set<std::string> active_players;
	std::mutex ap_lock;

	std::vector<std::thread> worker_threads;
	HANDLE h_iocp;
	SOCKET s_socket, c_socket;
	OVER_EXP s_over;
	std::atomic_int user_id = 0;
public:
	std::array<std::shared_ptr<Object>, MAX_USER+MAX_NPC> objects;

	Server();
	~Server();
	void worker_thread();

	bool is_npc(int obj_id);
	bool is_pc(int obj_id);
	bool in_range(const std::shared_ptr<Object>& from, const std::shared_ptr<Object>& to, const int range = VIEW_RANGE);
	bool in_range(const int from, const int to, const int range = VIEW_RANGE);
	void wake_up_npc(const int npc_id, const int waker_id);
	
	int get_new_client_id();
	void disconnect(int c_id);
	void success_login(int c_id, char* packet);

	void do_npc_random_move(int npc_id);
	bool do_npc_target_move(std::shared_ptr<NPC>& npc, int target_id);
	bool do_npc_attack(std::shared_ptr<NPC>& npc, int target_id);

	void init_npcs();

	void start();
	void process_packet(int c_id, char* p);
	void post(OVER_EXP* ex_over, int key);
	void push_time_event(const int id, const int target, const int time, const TIMER_TYPE evt_type, const PLAYER_ACTION type);

	std::shared_ptr<NPC> cast_npc(int id);
	std::shared_ptr<Session> cast_session(int id);

	void npc_move(const int mover, const short x, const short y);
	void update_view_list(const auto& mover, const std::unordered_set<int>& old_view_list);
};