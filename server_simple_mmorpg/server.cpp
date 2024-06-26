#include "server.h"
#include "global.h"

using namespace std;

Server::Server()
{
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);

	SOCKADDR_IN server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT_NUM);
	server_addr.sin_addr.S_un.S_addr = INADDR_ANY;

	s_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	bind(s_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
	listen(s_socket, SOMAXCONN);

	h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(s_socket), h_iocp, 9999, 0);
	c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	s_over._comp_type = OP_TYPE::ACCEPT;
}

Server::~Server()
{
	for (auto& obj : objects) {
		if (obj == nullptr) continue;
		if (is_pc(obj->id)) disconnect(obj->id);
		objects[obj->id] = nullptr;
	}

	closesocket(s_socket);
	WSACleanup();
}

void Server::start()
{
	SOCKADDR_IN cl_addr{};
	int addr_size = sizeof(cl_addr);

	AcceptEx(s_socket, c_socket, s_over._send_buf, 0, addr_size + 16, addr_size + 16, 0, &s_over._over);

	int num_threads = std::thread::hardware_concurrency();
	for (int i = 0; i < num_threads; ++i)
		worker_threads.emplace_back( [this]() { this->worker_thread(); } );

	worker_threads.emplace_back( [this]() { g_timer.timer_thread(); });
	worker_threads.emplace_back( [this]() { g_db.database_thread(); });

	for (auto& th : worker_threads)
		th.join();

}

void Server::init_npcs()
{
	std::cout << "NPC intialize begin.\n";
	
	int NPC_ID_START = MAX_USER;

	for (int i = NPC_ID_START; i < NPC_ID_START + NPC_TORTOISE; ++i) {
		int x{}, y{};
		do {
			x = rand() % W_WIDTH - 1;
			y = rand() % W_HEIGHT - 1;
		} while ( x <= 975 && x >= 1024 && y <= 975 && y >= 1024);

		auto npc = std::make_shared<NPC>(i, x, y, NPC_TYPE::TORTOISE);
		sprintf_s(npc->name, "TORTO");
		objects[i] = npc;
	}
	NPC_ID_START += NPC_TORTOISE;

	std::cout << "NPC initialize end.\n";
}

bool Server::is_pc(int object_id)
{
	return object_id < MAX_USER;
}

bool Server::is_npc(int object_id)
{
	return !is_pc(object_id);
}

bool Server::in_range(const shared_ptr<Object>& from, const shared_ptr<Object>& to, const int range)
{
	if (abs(from->x - to->x) > range) return false;
	return abs(from->y - to->y) <= range;
}

bool Server::in_range(const int from, const int to, const int range)
{
	auto& a = objects[from];
	if (a == nullptr) return false;

	auto& b = objects[to];
	if (b == nullptr) return false;

	if (abs(a->x - b->x) > range) return false;
	return abs(a->y - b->y) <= range;
}

int Server::get_new_client_id()
{
	if (user_id >= MAX_USER) {
		cout << "MAX USER FULL\n";
		return -1;
	}
	return user_id++;
}

void Server::disconnect(int c_id)
{
	auto cur_player = cast_session(c_id);
	if (nullptr == cur_player) {
		for (const auto& other_player : objects | views::take(MAX_USER)) {
			if (nullptr == other_player) continue;
			if (other_player->id == c_id) continue;
			if (other_player->state != STATE::INGAME) continue;

			auto o_pl = reinterpret_pointer_cast<Session>(other_player);
			o_pl->send_remove_player_packet(c_id);
		}
		return;
	}

	// DB Logout 데이터 전달
	{
		SD_LOGOUT dv{};
		dv.type = SQL_TYPE::LOGOUT;
		strcpy_s(dv.name, cur_player->name);
		dv.x = cur_player->x;
		dv.y = cur_player->y;
		dv.hp = cur_player->hp;
		dv.level = cur_player->level;
		dv.exp = cur_player->exp;
		g_db.push_event(&dv, sizeof(SD_LOGOUT));
	}
	
	cur_player->vl.lock();
	unordered_set <int> vl = cur_player->view_list;
	cur_player->vl.unlock();

	for (const int other_id : vl) {
		if (is_npc(other_id)) continue;
		if (other_id == c_id) continue;

		auto other_player = cast_session(other_id);
		if (nullptr == other_player) continue;
		if (STATE::INGAME != other_player->state) continue;

		other_player->send_remove_player_packet(c_id);
	}

	closesocket(cur_player->socket);

	ap_lock.lock();
	active_players.erase(cur_player->name);
	ap_lock.unlock();
	
	cur_player->state = STATE::FREE;
	//objects[c_id] = nullptr;
}

void Server::wake_up_npc(const int npc_id, const int waker_id)
{
	auto npc = cast_npc(npc_id);

	bool old_state = false;
	if (false == atomic_compare_exchange_strong(&npc->is_active, &old_state, true)) return;

	TIMER_EVENT ev{ npc_id, chrono::steady_clock::now(), TIMER_TYPE::NPC_ACTION, waker_id };
	g_timer.push_event(ev);
}

void Server::success_login(int c_id, char* pk)
{
	DS_LOGIN_INFO* p = reinterpret_cast<DS_LOGIN_INFO*>(pk);
	auto cur_player = cast_session(c_id);
	if (cur_player == nullptr) return;

	strcpy_s(cur_player->name, p->name);
	cur_player->x = p->x;
	cur_player->y = p->y;
	cur_player->set_hp(p->hp);
	cur_player->level = p->level;
	cur_player->exp = p->exp;
	cur_player->state = STATE::INGAME;

	cur_player->send_login_info_packet();
	cur_player->do_heal();
	
	for (auto& obj : objects) {
		if (nullptr == obj) continue;
		if (c_id == obj->id) continue;
		if (false == in_range(cur_player, obj)) continue;
		if (true == obj->is_dead) continue;

		if (true == is_pc(obj->id)) {
			auto other_player = cast_session(obj->id);
			if (STATE::INGAME != other_player->state) continue;

			other_player->send_add_player_packet(cur_player);
		}
		else {
			wake_up_npc(obj->id, c_id);
		}
		cur_player->send_add_player_packet(obj);
	}
}

void Server::push_time_event(const int id, const int target, const int time, const TIMER_TYPE evt_type, const PLAYER_ACTION type)
{
	TIMER_EVENT evt {};

	evt.wakeup_time = chrono::steady_clock::now() + chrono::milliseconds(time);
	evt.obj_id = id;
	evt.event_id = evt_type;
	evt.skill_type = type;
	evt.target_id = target;

	g_timer.push_event(evt);
}

void Server::process_packet(int c_id, char* packet)
{
	switch (packet[2]) {
		case CS_LOGIN: {
			CS_LOGIN_PACKET* p = reinterpret_cast<CS_LOGIN_PACKET*>(packet);
			
			// 중복 로그인 체크
			string cl_name{ p->name };
			ap_lock.lock();
			if (active_players.contains(cl_name)) {
				ap_lock.unlock();
				auto cur_player = cast_session(c_id);
				if (nullptr == cur_player) break;

				cur_player->send_login_fail_packet();
			}
			else {
				active_players.insert(cl_name);
				ap_lock.unlock();
			}
			
			// DB로 데이터 조회 (닉네임, 레벨 등)
			SD_LOGIN_INFO dv{};
			dv.type = SQL_TYPE::LOGIN;
			dv.c_id = c_id;
			strcpy_s(dv.name, p->name);
			g_db.push_event(&dv, sizeof(SD_LOGIN_INFO));
			break;
		}
		case CS_MOVE: {
			CS_MOVE_PACKET* p = reinterpret_cast<CS_MOVE_PACKET*>(packet);
			auto cur_player = cast_session(c_id);
			if (cur_player == nullptr) break;

			bool old_state = true;
			if (false == atomic_compare_exchange_strong(&cur_player->can_move, &old_state, false)) {
				break;
			}

			cur_player->do_move(p->direction);
			cur_player->last_move_time = p->move_time;

			unordered_set<int> near_list;
			cur_player->vl.lock();
			unordered_set<int> old_vlist = cur_player->view_list;
			cur_player->vl.unlock();

			for (auto& obj : objects) {
				if (obj == nullptr) continue;
				if (obj->is_dead == true) continue;
				if (obj->state != STATE::INGAME) continue;
				if (obj->id == cur_player->id) continue;
				if (in_range(cur_player, obj))
					near_list.insert(obj->id);
			}

			cur_player->send_move_packet(cur_player);

			for (const int pl : near_list) {
				auto& obj = objects[pl];
				if (obj == nullptr) continue;
				if (is_pc(pl)) {
					auto other_player = reinterpret_pointer_cast<Session>(obj);
					if (other_player == nullptr) continue;

					other_player->vl.lock();
					if (other_player->view_list.count(c_id)) {
						other_player->vl.unlock();
						other_player->send_move_packet(cur_player);
					}
					else {
						other_player->vl.unlock();
						other_player->send_add_player_packet(cur_player);
					}
				}
				else {
					wake_up_npc(pl, c_id);
				}

				if (old_vlist.count(pl) == 0) {
					cur_player->send_add_player_packet(obj);
				}
			}

			for (const int pl : old_vlist) {
				if (0 == near_list.count(pl)) {
					cur_player->send_remove_player_packet(pl);
					if (is_pc(pl)) {
						auto other = cast_session(pl);
						if (other == nullptr) continue;
						other->send_remove_player_packet(c_id);
					}
				}
			}

			break;
		}
		case CS_ATTACK: {
			auto cur_player = cast_session(c_id);
			if (cur_player == nullptr) break;

			bool can_attack = true;
			if (false == atomic_compare_exchange_strong(&cur_player->can_attack, &can_attack, false))
				break;
			
			// 공격 쿨타임 적용
			push_time_event(cur_player->id, 0, 1000, TIMER_TYPE::PLAYER_ACTION, PLAYER_ACTION::ATTACK);

			cur_player->vl.lock();
			const unordered_set<int> cur_view_list = cur_player->view_list;
			cur_player->vl.unlock();

			// 데미지 적용
			for (const int obj_id : cur_view_list) {
				if (true == is_pc(obj_id)) continue;

				auto npc = cast_npc(obj_id);
				if (npc->is_dead == true) continue;

				if (true == in_range(cur_player, npc, 1)) {
					// 데미지 적용 broadcast
					npc->hp -= cur_player->power;
					npc->target_player = cur_player;

					for (auto& obj : objects | views::take(MAX_USER)) {
						if (nullptr == obj) continue;
						if (false == in_range(obj, npc)) continue;

						auto cl = cast_session(obj->id);
						if (cl->state != STATE::INGAME) continue;

						cl->send_hit_packet(c_id, obj_id, cur_player->power);
					}

					// NPC 사망 처리
					if (npc->hp <= 0) {
						bool old_state = false;
						if (false == atomic_compare_exchange_strong(&npc->is_dead, &old_state, true)) continue;

						// 아이템 드랍

						// 플레이어 경험치 추가
						int exp_size = ((npc->level * npc->level) << 1);
						if (npc->type == NPC_TYPE::TIGER || npc->type == NPC_TYPE::BIRD)
							exp_size = exp_size << 1;
						cur_player->add_exp(exp_size);
						cur_player->send_change_stat_packet();

						// NPC 시야의 플레이어들
						for (auto& pl : objects | views::take(MAX_USER)) {	
							if (nullptr == pl) continue;
							auto cl = cast_session(pl->id);
							if (cl->state != STATE::INGAME) continue;
							if (false == in_range(npc, cl)) continue;

							cl->send_remove_player_packet(npc->id);
							cl->send_obtain_packet(cur_player->id, exp_size);
						}

						// 리스폰 이벤트 추가
						push_time_event(npc->id, 0, 30000, TIMER_TYPE::RESPAWN, PLAYER_ACTION::DEFAULT);
					}
				}
			}
			break;
		}
		case CS_CHAT: {
			CS_CHAT_PACKET* p = reinterpret_cast<CS_CHAT_PACKET*>(packet);

			auto client = cast_session(c_id);
			if (client == nullptr) break;

			client->vl.lock();
			unordered_set<int> old_vlist = client->view_list;
			client->vl.unlock();

			for (const int obj_id : old_vlist) {
				if (nullptr == objects[obj_id]) continue;
				if (true == is_npc(obj_id)) continue;
				auto other = cast_session(obj_id);
				if (other == nullptr) continue;
				if (other->state != STATE::INGAME) continue;

				other->send_chat_packet(c_id, p->mess);
			}

			break;
		}
		case CS_LOGOUT: {
			disconnect(c_id);
			break;
		}
	}
}

bool Server::do_npc_attack(shared_ptr<NPC>& npc, int target)
{
	auto player = cast_session(target);
	if (player == nullptr) return false;
	if (player->state != STATE::INGAME) return false;
	if (player->is_dead == true) return false;

	player->set_hp(player->hp - npc->power);

	bool old_state = true;
	if (true == atomic_compare_exchange_strong(&player->can_heal, &old_state, false)) {
		TIMER_EVENT c_ev{ player->id, chrono::steady_clock::now()+5s, TIMER_TYPE::PLAYER_ACTION, 0 };
		c_ev.skill_type = PLAYER_ACTION::HEAL;
		g_timer.push_event(c_ev);
	}

	if (player->hp <= 0) {
		npc->has_target = false;
		player->set_dead();

		player->vl.lock();
		auto& cur_view_list = player->view_list;
		player->view_list.clear();
		player->vl.unlock();

		for (const int obj_id : cur_view_list) {
			if (true == is_npc(obj_id)) continue;
			if (nullptr == objects[obj_id]) continue;
			auto cl = cast_session(obj_id);
			if (nullptr == cl) continue;
			cl->send_remove_player_packet(player->id);
			player->send_remove_player_packet(obj_id);
		}

		TIMER_EVENT ev{ player->id, chrono::steady_clock::now() + 5s, TIMER_TYPE::RESPAWN, 0 };
		g_timer.push_event(ev);
	}
	else {
		for (auto& obj : objects | views::take(MAX_USER)) {
			if (nullptr == obj) continue;
			if (false == in_range(obj, npc)) continue;
			auto cl = cast_session(obj->id);
			if (nullptr == cl) continue;
			if (cl->state != STATE::INGAME) continue;

			cl->send_hit_packet(npc->id, player->id, npc->power);
		}
	}

	player->send_change_stat_packet();

	return true;
}

void Server::worker_thread()
{
	while (true) {
		DWORD num_bytes;
		ULONG_PTR key;
		WSAOVERLAPPED* over = nullptr;
		BOOL ret = GetQueuedCompletionStatus(h_iocp, &num_bytes, &key, &over, INFINITE);
		OVER_EXP* ex_over = reinterpret_cast<OVER_EXP*>(over);
		if (FALSE == ret) {
			if (ex_over->_comp_type == OP_TYPE::ACCEPT) cout << "Accept Error";
			else {
				cout << "GQCS Error on client[" << key << "]\n";
				disconnect(static_cast<int>(key));
				if (ex_over->_comp_type == OP_TYPE::SEND) delete ex_over;
				continue;
			}
		}

		switch (ex_over->_comp_type) {
			case OP_TYPE::ACCEPT: {
				int client_id = get_new_client_id();
				if (-1 != client_id) {
					auto client = make_shared<Session>(client_id, c_socket);
					objects[client_id] = client;
					client->state = STATE::ALLOC;
					CreateIoCompletionPort(reinterpret_cast<HANDLE>(c_socket), h_iocp, client_id, 0);
					client->do_recv();
					c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
				}

				ZeroMemory(&s_over._over, sizeof(s_over._over));
				int addr_size = sizeof(SOCKADDR_IN);
				AcceptEx(s_socket, c_socket, s_over._send_buf, 0, addr_size + 16, addr_size + 16, 0, &s_over._over);
				break;
			}
			case OP_TYPE::RECV: {
				if (num_bytes == 0)
					disconnect(static_cast<int>(key));

				auto client = cast_session(static_cast<int>(key));
				if (client == nullptr) break;
				
				for (long long i = 0; i < num_bytes; ++i)
					client->my_buf.push_back(ex_over->_send_buf[i]);
				vector<char>& p = client->my_buf;
				while (num_bytes > 1) {
					unsigned short size = client->my_buf[0] + client->my_buf[1] * 256;
					if (size <= client->my_buf.size()) {
						process_packet(static_cast<int>(key), p.data());
						client->my_buf.erase(p.begin(), p.begin() + size);
						num_bytes -= size;
					}
					else {
						break;
					}
				}
				client->do_recv();
				break;
			}
			case OP_TYPE::SEND: {
				if (num_bytes == 0)
					disconnect(static_cast<int>(key));
				delete ex_over;
				break;
			}

			case OP_TYPE::NPC_ACTION: {
				int npc_id = static_cast<int>(key);
				auto npc = cast_npc(npc_id);
				
				bool is_active = false;
				for (auto& obj : objects | views::take(MAX_USER)) {
					if (obj == nullptr) continue;
					auto pl = cast_session(obj->id);
					if (pl->state != STATE::INGAME) continue;
					if (in_range(npc, obj)) {
						is_active = true;
						break;
					}
				}
				
				if (true == is_active) {
					switch (npc->type) {
						case NPC_TYPE::TORTOISE: {
							npc->random_move();
							break;
						}
						default: break;
					}
				}
				else {
					bool old_state = true;
					if (true == atomic_compare_exchange_strong(&npc->is_active, &old_state, false)) {
						npc->set_default();
					}
				}

				delete ex_over;
				break;
			}
			case OP_TYPE::COOL_DOWN: {
				int c_id = static_cast<int>(key);
				auto obj = cast_session(c_id);
				if (obj->state != STATE::INGAME) {
					delete ex_over;
					break;
				}

				PLAYER_ACTION type = reinterpret_cast<PLAYER_ACTION*>(ex_over->_send_buf)[0];
				switch (type) {
					case PLAYER_ACTION::ATTACK: {
						obj->can_attack = true;
						break;
					}
					case PLAYER_ACTION::MOVE: {
						obj->can_move = true;
						break;
					}
					case PLAYER_ACTION::HEAL: {
						obj->can_heal = true;
						obj->do_heal();
						break;
					}
					default: break;
				}
				delete ex_over;
				break;
			}
			case OP_TYPE::COMPL_DB: {
				int c_id = static_cast<int>(key);
				success_login(c_id, ex_over->_send_buf);
				delete ex_over;
				break;
			}
			case OP_TYPE::RESPAWN: {
				int obj_id = static_cast<int>(key);
				if (is_pc(obj_id)) {
					auto player = cast_session(obj_id);
					if (player != nullptr) {
						player->state = STATE::INGAME;

						player->send_move_packet(player);
						player->can_move = true;
						player->can_attack = true;
						player->is_dead = false;

						for (auto& obj : objects) { 
							if (obj == nullptr) continue;
							if (STATE::INGAME != obj->state) continue;
							if (obj->id == player->id) continue;
							if (false == in_range(player, obj)) continue;
							if (obj->is_dead == true) continue;

							if (is_pc(obj->id)) {
								auto other = reinterpret_pointer_cast<Session>(obj);
								other->send_add_player_packet(player);
							}
							else {
								wake_up_npc(obj->id, player->id);
							}

							player->send_add_player_packet(obj);
						}
					}
				}
				else {
					auto npc = cast_npc(obj_id);
					npc->set_default();
					for (auto& obj : objects | views::take(MAX_USER)) {
						if (nullptr == obj) continue;
						if (obj->state != STATE::INGAME) continue;
						if (false == in_range(obj, npc)) continue;

						auto cl = reinterpret_pointer_cast<Session>(obj);
						cl->send_add_player_packet(npc);
					}
					wake_up_npc(npc->id, -1);
				}

				delete ex_over;
				break;
			}
			default: break;
		}
	}
}

void Server::post(OVER_EXP* ex_over, int key)
{
	PostQueuedCompletionStatus(h_iocp, 1, key, &ex_over->_over);
}

shared_ptr<NPC> Server::cast_npc(int id)
{
	return std::move(reinterpret_pointer_cast<NPC>(objects[id]));
}

shared_ptr<Session> Server::cast_session(int id)
{
	return std::move(reinterpret_pointer_cast<Session>(objects[id]));
}
