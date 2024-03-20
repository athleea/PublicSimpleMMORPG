#include "timer.h"
#include "global.h"

using namespace std;

void Timer::timer_thread()
{
	priority_queue<TIMER_EVENT> temp_queue;
	while (true) {
		TIMER_EVENT ev;
		auto current_time = chrono::steady_clock::now();
		
		while ( not temp_queue.empty() && temp_queue.top().wakeup_time < current_time) {
			ev = temp_queue.top();
			process_event(ev);
			temp_queue.pop();
		}

		if (true == timer_queue.try_pop(ev)) {
			if (ev.wakeup_time > current_time) {
				temp_queue.push(ev);
				this_thread::sleep_for(1ms);
				continue;
			}
			process_event(ev);
			continue;
		}
		this_thread::sleep_for(1ms);
	}
}

void Timer::push_event(const TIMER_EVENT& evt)
{
	timer_queue.push(evt);
}


void Timer::process_event(const TIMER_EVENT& evt)
{
	switch (evt.event_id) {
		case TIMER_TYPE::PLAYER_ACTION: {
			OVER_EXP* ov = new OVER_EXP{};
			ov->_comp_type = OP_TYPE::COOL_DOWN;
			memcpy(ov->_send_buf, (char*)&evt.skill_type, sizeof(PLAYER_ACTION));
			g_server.post(ov, evt.obj_id);
			break;
		}
		case TIMER_TYPE::RESPAWN: {
			OVER_EXP* ov = new OVER_EXP{};
			ov->_comp_type = OP_TYPE::RESPAWN;
			g_server.post(ov, evt.obj_id);
			break;
		}
		case TIMER_TYPE::NPC_ACTION: {
			OVER_EXP* ov = new OVER_EXP{};
			ov->_comp_type = OP_TYPE::NPC_ACTION;
			g_server.post(ov, evt.obj_id);
			break;
		}
		default: break;
	}
}
