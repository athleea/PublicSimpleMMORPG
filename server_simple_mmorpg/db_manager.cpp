#include "db_manager.h"
#include "global.h"

using namespace std;

void show_error(SQLHANDLE hHandle, SQLSMALLINT hType, RETCODE RetCode)
{
	SQLSMALLINT iRec = 0;
	SQLINTEGER iError;
	WCHAR wszmsgage[1000];
	WCHAR wszState[SQL_SQLSTATE_SIZE + 1];
	if (RetCode == SQL_INVALID_HANDLE) {
		fwprintf(stderr, L"Invalid handle!\n");
		return;
	}
	while (SQLGetDiagRec(hType, hHandle, ++iRec, wszState, &iError, wszmsgage,
		(SQLSMALLINT)(sizeof(wszmsgage) / sizeof(WCHAR)), (SQLSMALLINT*)NULL) == SQL_SUCCESS) {
		// Hide data truncated..
		if (wcsncmp(wszState, L"01004", 5)) {
			fwprintf(stderr, L"[%5.5s] %s (%d)\n", wszState, wszmsgage, iError);
		}
	}
}

DBManager::DBManager()
{
	if (connect() == false) {
		cout << "Failed Connecting DB\n";
		exit(-1);
	}
}

DBManager::~DBManager()
{
	SQLDisconnect(hdbc);
	SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
	SQLFreeHandle(SQL_HANDLE_ENV, henv);
}

void DBManager::database_thread()
{
	DB_EVENT dv{};
	while (true) {
		if (true == db_queue.try_pop(dv)) {
			switch (static_cast<SQL_TYPE>(dv.buf[0])) {
				case SQL_TYPE::LOGIN: {
						SD_LOGIN_INFO* p = reinterpret_cast<SD_LOGIN_INFO*>(dv.buf);
						std::wstring name(p->name, &p->name[strlen(p->name)]);
						exec_login(p->c_id, name);
						break;
					}
				case SQL_TYPE::LOGOUT: {
						SD_LOGOUT* p = reinterpret_cast<SD_LOGOUT*>(dv.buf);
						exec_logout(p->name, p->x, p->y, p->hp, p->level, p->exp);
						break;
					}
				}
			continue;
		}
		this_thread::sleep_for(1ms);
	}
}

bool DBManager::connect()
{
	SQLRETURN retcode;

	retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv); // Allocate environment handle
	retcode = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER*)SQL_OV_ODBC3, 0); // Set the ODBC version environment attribute  
	retcode = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc); // Allocate connection handle  
	SQLSetConnectAttr(hdbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)5, 0); // Set login timeout to 5 seconds
	retcode = SQLConnect(hdbc, (SQLWCHAR*)L"TermProject", SQL_NTS, (SQLWCHAR*)NULL, 0, NULL, 0); // Connect to data source  
	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
		std::cout << "Completed Connecting DB\n";
		return true;
	}
	else {
		show_error(hdbc, SQL_HANDLE_STMT, retcode);
		return false;
	}
}

void DBManager::push_event(const DB_EVENT& ev)
{
	DB_EVENT db_ev = ev;
	db_queue.push(db_ev);
}

void DBManager::push_event(void* packet, size_t packet_size)
{
	DB_EVENT ev{};
	memcpy(ev.buf, packet, packet_size);
	db_queue.push(ev);
}

void DBManager::exec_login(int c_id, wstring& name)
{
	SQLHSTMT hstmt;
	SQLRETURN retcode;
	SQLCHAR player_name[NAME_SIZE]{};
	SQLINTEGER player_x{}, player_y{}, player_hp{}, player_exp{}, player_level{};
	SQLLEN cb_name = 0, cb_x = 0, cb_y = 0, cb_exp = 0, cb_hp = 0, cb_level = 0;

	std::wstring query{ L"EXEC get_login_info " + name };

	auto player = g_server.cast_session(c_id);

	retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
	retcode = SQLExecDirect(hstmt, (SQLWCHAR*)query.c_str(), SQL_NTS);

	if (retcode == SQL_SUCCESS) {
		retcode = SQLBindCol(hstmt, 1, SQL_C_CHAR, player_name, NAME_SIZE, &cb_name);
		retcode = SQLBindCol(hstmt, 2, SQL_C_LONG, &player_x, 10, &cb_x);
		retcode = SQLBindCol(hstmt, 3, SQL_C_LONG, &player_y, 10, &cb_y);
		retcode = SQLBindCol(hstmt, 4, SQL_C_LONG, &player_hp, 10, &cb_hp);
		retcode = SQLBindCol(hstmt, 5, SQL_C_LONG, &player_level, 10, &cb_level);
		retcode = SQLBindCol(hstmt, 6, SQL_C_LONG, &player_exp, 10, &cb_exp);

		retcode = SQLFetch(hstmt);
		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
			DS_LOGIN_INFO packet{};
			std::string str_name ((const char*) player_name);
			strcpy_s(packet.name, str_name.c_str());
			packet.x = player_x;
			packet.y = player_y;
			packet.hp = player_hp;
			packet.level = player_level;
			packet.exp = player_exp;
			packet.exp = player_exp;

			OVER_EXP* exover = new OVER_EXP;
			exover->_comp_type = OP_TYPE::COMPL_DB;
			memcpy(exover->_send_buf, &packet, sizeof(DS_LOGIN_INFO));

			g_server.post(exover, player->id);
		}
		else {
			retcode = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
			exec_register(c_id, name);
			return;
		}
	}
	else {
		show_error(hstmt, SQL_HANDLE_STMT, retcode);
	}

	retcode = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
}

void DBManager::exec_logout(char* n, short x, short y, int hp, int level, int exp)
{
	//SET @X, @Y, @EXP, @LEVEL, @HP
	SQLHSTMT hstmt;
	SQLRETURN retcode;

	std::wstring name(n, &n[strlen(n)]);
	std::wstring query = std::format(L"EXEC save_player_pos {}, {}, {}, {}, {}, {}",
		name.c_str(), 
		std::to_wstring(x), 
		std::to_wstring(y),
		std::to_wstring(exp), 
		std::to_wstring(level), 
		std::to_wstring(hp)
	);

	retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
	retcode = SQLExecDirect(hstmt, (SQLWCHAR*)query.c_str(), SQL_NTS);
	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
	}
	else {
		show_error(hstmt, SQL_HANDLE_STMT, retcode);
	}

	retcode = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
}

void DBManager::exec_register(int c_id, wstring& name)
{
	SQLHSTMT hstmt;
	SQLRETURN retcode;

	short x = rand() % W_WIDTH;
	short y = rand() % W_HEIGHT;
	//std::wstring query {L"EXEC register_client " + name};
	std::wstring query = std::format(L"EXEC register_client {}, {}, {}",
		name.c_str(),
		std::to_wstring(x),
		std::to_wstring(y)
	);

	retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
	retcode = SQLExecDirect(hstmt, (SQLWCHAR*)query.c_str(), SQL_NTS);
	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
		retcode = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
		exec_login(c_id, name);
		return;
	}
	else {
		show_error(hstmt, SQL_HANDLE_STMT, retcode);
	}

	retcode = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
}

