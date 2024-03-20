#pragma once
#include "stdafx.h"

struct TIMER_EVENT;

class Timer {
	concurrency::concurrent_priority_queue<TIMER_EVENT> timer_queue{};
public:
	Timer() { };
	~Timer() { };

	void timer_thread();
	void push_event(const TIMER_EVENT& ev);
	void process_event(const TIMER_EVENT& ev);
};