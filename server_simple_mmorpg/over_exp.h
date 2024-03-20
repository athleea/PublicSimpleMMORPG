#pragma once
#include "stdafx.h"

enum class OP_TYPE { 
	ACCEPT, 
	RECV, 
	SEND, 
	COMPL_DB ,
	COOL_DOWN,
	RESPAWN,
	NPC_ACTION,
};

class OVER_EXP {
public:
	WSAOVERLAPPED _over;
	WSABUF _wsabuf;
	char _send_buf[BUF_SIZE]{};
	OP_TYPE _comp_type;
	int _ai_target_obj{};
	OVER_EXP()
	{
		_wsabuf.len = BUF_SIZE;
		_wsabuf.buf = _send_buf;
		_comp_type = OP_TYPE::RECV;
		ZeroMemory(&_over, sizeof(_over));
	}
	OVER_EXP(char* packet)
	{
		unsigned short packet_size = *reinterpret_cast<unsigned short*>(packet);
		_wsabuf.len = packet_size;
		_wsabuf.buf = _send_buf;
		ZeroMemory(&_over, sizeof(_over));
		_comp_type = OP_TYPE::SEND;
		memcpy(_send_buf, packet, packet_size);
	}
};