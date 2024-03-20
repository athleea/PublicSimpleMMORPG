#pragma once
#include "stdafx.h"

constexpr int MAX_SQL_SIZE = 256;

enum class SQL_TYPE { LOGIN, LOGOUT };

#pragma pack (push, 1)
struct DB_EVENT {
	char buf[MAX_SQL_SIZE];
};


struct SD_LOGIN_INFO {
	SQL_TYPE type;
	int c_id;
	char name[NAME_SIZE];
};

struct SD_LOGOUT {
	SQL_TYPE type;
	char name[NAME_SIZE];
	int x;
	int y;
	int hp;
	int exp;
	int level;
};

struct DS_LOGIN_INFO {
	char name[NAME_SIZE];
	int x;
	int y;
	int hp;
	int exp;
	int level;
};
#pragma pack (pop)

class DBManager {
private:
	SQLHENV henv{};
	SQLHDBC hdbc{};

	concurrency::concurrent_queue<DB_EVENT> db_queue{};
public:
	DBManager();
	~DBManager();

	void database_thread();

	bool connect();
	void push_event(const DB_EVENT& ev);
	void push_event(void* packet, size_t packet_size);

	void exec_login(int id, std::wstring& name);
	void exec_logout(char* name, short x, short y, int hp, int level, int exp);
	void exec_register(int id, std::wstring& name);
};