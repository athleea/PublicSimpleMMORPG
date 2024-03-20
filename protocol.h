constexpr int PORT_NUM = 4000;
constexpr int NAME_SIZE = 20;
constexpr int CHAT_SIZE = 200;

constexpr int MAX_USER			= 50000;
constexpr int NPC_TORTOISE	= 25000;
constexpr int NPC_DRAGON	= 25000;
constexpr int NPC_BIRD			= 25000;
constexpr int NPC_TIGER		= 25000;
constexpr int MAX_NPC = NPC_BIRD + NPC_TIGER + NPC_DRAGON + NPC_TORTOISE;

constexpr int W_WIDTH = 2000;
constexpr int W_HEIGHT = 2000;

// Packet ID
constexpr char CS_LOGIN = 0;
constexpr char CS_MOVE = 1;
constexpr char CS_CHAT = 2;
constexpr char CS_ATTACK = 3;
constexpr char CS_TELEPORT = 4;
constexpr char CS_LOGOUT = 5;

constexpr char SC_LOGIN_INFO = 2;
constexpr char SC_ADD_OBJECT = 3;
constexpr char SC_REMOVE_OBJECT = 4;
constexpr char SC_MOVE_OBJECT = 5;
constexpr char SC_CHAT = 6;
constexpr char SC_LOGIN_OK = 7;
constexpr char SC_LOGIN_FAIL = 8;
constexpr char SC_STAT_CHANGE = 9;
constexpr char SC_HIT = 10;
constexpr char SC_OBTAIN = 11;

enum class NPC_TYPE {
	DRAGON,
	TIGER,
	BIRD,
	TORTOISE
};

#pragma pack (push, 1)
struct CS_LOGIN_PACKET {
	unsigned short size;
	char	type;
	char	name[NAME_SIZE];
};

struct CS_MOVE_PACKET {
	unsigned short size;
	char	type;
	char	direction;  // 0 : UP, 1 : DOWN, 2 : LEFT, 3 : RIGHT
	unsigned	 move_time;
};

struct CS_CHAT_PACKET {
	unsigned short size;
	char	type;
	char	mess[CHAT_SIZE];
};

struct CS_TELEPORT_PACKET {
	unsigned short size;
	char	type;
};

struct CS_LOGOUT_PACKET {
	unsigned short size;
	char	type;
};

struct SC_LOGIN_INFO_PACKET {
	unsigned short size;
	char	type;
	int		id;
	int		hp;
	int		max_hp;
	int		exp;
	int		level;
	short	x, y;
};

struct SC_ADD_OBJECT_PACKET {
	unsigned short size;
	char	type;
	int		id;
	short	x, y;
	char	name[NAME_SIZE];
};

struct SC_REMOVE_OBJECT_PACKET {
	unsigned short size;
	char	type;
	int		id;
};

struct SC_MOVE_OBJECT_PACKET {
	unsigned short size;
	char	type;
	int		id;
	short	x, y;
	unsigned int move_time;
};

struct SC_CHAT_PACKET {
	unsigned short size;
	char	type;
	int		id;
	char	mess[CHAT_SIZE];
};

struct SC_LOGIN_OK_PACKET {
	unsigned short size;
	char	type;
};

struct SC_LOGIN_FAIL_PACKET {
	unsigned short size;
	char	type;

};

struct SC_STAT_CHANGE_PACKET {
	unsigned short size;
	char	type;
	int		hp;
	int		max_hp;
	int		exp;
	int		level;

};

// add custom packet

struct CS_ATTACK_PACKET {
	unsigned short size;
	char	type;
};

struct SC_HIT_PACKET {
	unsigned short size;
	char	type;
	int attacker;
	int victim;
	int damage;
};
struct SC_OBTAIN_PAKET {
	unsigned short size;
	char	type;
	int obtainer;
	int exp;
};
#pragma pack (pop)