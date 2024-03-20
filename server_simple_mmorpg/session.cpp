#include <cmath>
#include "Session.h"
#include "global.h"

using namespace std;


void Session::add_exp(int exp_size)
{
	exp += exp_size;
	int level_up_size = (1 << (level - 1)) * 100;
	if (exp >= level_up_size) {
		exp -= level_up_size;
		++level;
	}
}

void Session::set_dead()
{
	bool old_state = false;
	if (false == atomic_compare_exchange_strong(&is_dead, &old_state, true))
		return;

	state = STATE::ALLOC;
	x = W_WIDTH / 2;
	y = W_HEIGHT / 2;
	can_move = false;
	can_attack = false;
	can_heal  = false;

	exp = exp >> 1;
	if (exp <= 0) exp = 0;
	hp = MAX_PLAYER_HP;
}

void Session::do_heal()
{
	bool old_state = true;
	if (true == atomic_compare_exchange_strong(&can_heal, &old_state, false)) {
		if (hp <= MAX_PLAYER_HP) {
			set_hp(hp + static_cast<int>(MAX_PLAYER_HP * 0.1));
			send_change_stat_packet();

			TIMER_EVENT c_ev{ id, chrono::steady_clock::now() + 5s, TIMER_TYPE::PLAYER_ACTION, 0 };
			c_ev.skill_type = PLAYER_ACTION::HEAL;
			g_timer.push_event(c_ev);
		}
	}
}

void Session::do_move(const char direction)
{
	TIMER_EVENT evt{};
	evt.wakeup_time = chrono::steady_clock::now() + chrono::milliseconds(move_speed);
	evt.obj_id = id;
	evt.event_id = TIMER_TYPE::PLAYER_ACTION;
	evt.skill_type = PLAYER_ACTION::MOVE;
	evt.target_id = -1;
	g_timer.push_event(evt);

	switch (direction) {
		case 0: if (y > 0) --y; break;
		case 1: if (y < W_HEIGHT - 1) ++y; break;
		case 2: if (x > 0) --x; break;
		case 3: if (x < W_WIDTH - 1) ++x; break;
	}
}

void Session::do_recv()
{
	DWORD recv_flag = 0;
	memset(&_recv_over._over, 0, sizeof(_recv_over._over));
	_recv_over._wsabuf.len = BUF_SIZE - my_buf.size();
	_recv_over._wsabuf.buf = _recv_over._send_buf + my_buf.size();
	WSARecv(socket, &_recv_over._wsabuf, 1, 0, &recv_flag, &_recv_over._over, 0);
}

void Session::do_send(void* packet)
{
	OVER_EXP* sdata = new OVER_EXP{ reinterpret_cast<char*>(packet) };
	WSASend(socket, &sdata->_wsabuf, 1, 0, 0, &sdata->_over, 0);
}

void Session::send_login_info_packet()
{
	SC_LOGIN_INFO_PACKET p{};
	p.id = id;
	p.size = sizeof(SC_LOGIN_INFO_PACKET);
	p.type = SC_LOGIN_INFO;
	p.x = x;
	p.y = y;
	p.max_hp = MAX_PLAYER_HP;
	p.hp = hp;
	p.level = level;
	p.exp = exp;
	do_send(&p);
}
void Session::send_move_packet(const std::shared_ptr<Object>& other)
{
	SC_MOVE_OBJECT_PACKET p{};
	p.id = other->id;
	p.size = sizeof(SC_MOVE_OBJECT_PACKET);
	p.type = SC_MOVE_OBJECT;
	p.x = other->x;
	p.y = other->y;
	p.move_time = last_move_time;
	do_send(&p);
}

void Session::send_add_player_packet(const std::shared_ptr<Object>& other)
{
	SC_ADD_OBJECT_PACKET p{};
	p.id = other->id;
	strcpy_s(p.name, other->name);
	p.size = sizeof(p);
	p.type = SC_ADD_OBJECT;
	p.x = other->x;
	p.y = other->y;
	vl.lock();
	view_list.insert(other->id);
	vl.unlock();
	do_send(&p);
}

void Session::send_chat_packet(int p_id, const char* msg)
{
	SC_CHAT_PACKET p{};
	p.id = p_id;
	p.type = SC_CHAT;
	strcpy_s(p.mess, msg);
	p.size = sizeof(p.id) + sizeof(p.size) + sizeof(p.type) + strlen(p.mess) + 1;
	do_send(&p);
}

void Session::send_remove_player_packet(int c_id)
{
	vl.lock();
	if (view_list.count(c_id))
		view_list.erase(c_id);
	else {
		vl.unlock();
		return;
	}
	vl.unlock();

	SC_REMOVE_OBJECT_PACKET p{};
	p.id = c_id;
	p.size = sizeof(p);
	p.type = SC_REMOVE_OBJECT;
	do_send(&p);
}

void Session::send_login_fail_packet()
{
	SC_LOGIN_FAIL_PACKET p{};
	p.size = sizeof p;
	p.type = SC_LOGIN_FAIL;

	do_send(&p);
}

void Session::send_change_stat_packet()
{
	SC_STAT_CHANGE_PACKET p{};
	p.size = sizeof(SC_STAT_CHANGE_PACKET);
	p.type = SC_STAT_CHANGE;
	p.max_hp = MAX_PLAYER_HP;
	p.hp = hp;
	p.level = level;
	p.exp = exp;

	do_send(&p);
}

void Session::send_hit_packet(int attacker, int victim, int damage)
{
	SC_HIT_PACKET p{};
	p.size = sizeof(SC_HIT_PACKET);
	p.type = SC_HIT;
	p.attacker = attacker;
	p.victim = victim;
	p.damage = damage;

	do_send(&p);
}
void Session::send_obtain_packet(int obtainer, int exp)
{
	SC_OBTAIN_PAKET p{};
	p.size = sizeof(SC_OBTAIN_PAKET);
	p.type = SC_OBTAIN;
	p.obtainer = obtainer;
	p.exp = exp;

	do_send(&p);
}
;