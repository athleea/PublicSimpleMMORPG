#define SFML_STATIC 1
#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <iostream>
#include <unordered_map>
#include <Windows.h>
#include <chrono>
#include <string>
#include <queue>
#include <format>
#include <sstream>
#include <vector>


#pragma comment (lib, "opengl32.lib")
#pragma comment (lib, "winmm.lib")
#pragma comment (lib, "ws2_32.lib")

#include "../protocol.h"

using namespace std;

sf::TcpSocket s_socket;

constexpr auto SCREEN_WIDTH = 21;
constexpr auto SCREEN_HEIGHT = 21;
constexpr int BUF_SIZE = 1024;

constexpr auto TILE_WIDTH = 50;
constexpr auto TILE_HEIGHT = 50;
constexpr auto WINDOW_WIDTH = 1024;
constexpr auto WINDOW_HEIGHT = 800;

int g_left_x;
int g_top_y;
int g_myid;
bool g_chat_flag;

sf::RenderWindow* g_window;
sf::Font g_font;
sf::String g_player_input;
sf::Text g_chat_text;
sf::Text g_player_text;

class OBJECT {
private:
	bool m_showing;
	sf::Sprite m_sprite;

	sf::Text m_name;
	sf::Text m_chat;
	chrono::system_clock::time_point m_msg_end_time;
	chrono::system_clock::time_point m_dead_time;
public:
	int id;
	int m_x, m_y;
	char name[NAME_SIZE];
	int max_hp;
	int hp;
	int level;
	int exp;


	OBJECT(sf::Texture& t, int x, int y, int x2, int y2) {
		m_showing = false;
		m_sprite.setTexture(t);
		m_sprite.setTextureRect(sf::IntRect(x, y, x2, y2));
		set_name("NONAME");
		m_msg_end_time = chrono::system_clock::now();
		m_dead_time = chrono::system_clock::now();
	}
	OBJECT() {
		m_showing = false;
	}

	void set_stat(int mh, int h, int lv, int xp)
	{
		max_hp = mh;
		hp = h;
		level = lv;
		exp = xp;
	}

	void show()
	{
		m_showing = true;
	}
	void hide()
	{
		m_showing = false;
	}

	void a_move(int x, int y) {
		m_sprite.setPosition((float)x, (float)y);
	}

	void a_draw() {
		g_window->draw(m_sprite);
	}

	void move(int x, int y) {
		m_x = x;
		m_y = y;
	}
	void draw() {
		if (false == m_showing) return;
		float rx = (m_x - g_left_x) * TILE_WIDTH;
		float ry = (m_y - g_top_y) * TILE_HEIGHT;
		m_sprite.setPosition(rx, ry);
		g_window->draw(m_sprite);
		auto size = m_name.getGlobalBounds();

		if (m_msg_end_time < chrono::system_clock::now()) {
			m_name.setPosition(rx + 32 - size.width / 2, ry - 10);
			g_window->draw(m_name);
		}
		else {
			m_chat.setPosition(rx + 32 - size.width / 2, ry - 10);
			g_window->draw(m_chat);
		}

	}
	void set_name(const char str[]) {
		m_name.setFont(g_font);
		m_name.setString(str);
		if (id < MAX_USER) m_name.setFillColor(sf::Color(0, 0, 255));
		else m_name.setFillColor(sf::Color(255, 0, 0));
		m_name.setStyle(sf::Text::Bold);
		strcpy_s(name, str);
	}

	void set_chat(const char str[]) {
		m_chat.setCharacterSize(30);
		m_chat.setFont(g_font);
		m_chat.setString(str);
		m_chat.setFillColor(sf::Color(255, 255, 255));
		m_chat.setStyle(sf::Text::Bold);
		m_msg_end_time = chrono::system_clock::now() + chrono::seconds(3);
	}

	void set_dead() {
		m_chat.setCharacterSize(100);
		m_chat.setString(string("Wait 5 seconds for respawn"));
		m_chat.setFillColor(sf::Color(255, 0, 0));
		m_dead_time = chrono::system_clock::now() + chrono::seconds(5);
	}

};

OBJECT avatar;
unordered_map <int, OBJECT> players;

OBJECT white_tile;
OBJECT black_tile;

sf::Texture* board;
sf::Texture* pieces;
sf::Texture* monster;
sf::RectangleShape g_chat_rect(sf::Vector2f(400, 300));
std::string g_cur_string;
sf::Text g_cur_text;

vector<char> buf;
list<string> chat_queue;

void set_input_chat()
{
	if (chat_queue.size() >= 15) {
		chat_queue.pop_front();
	}

	g_player_input.clear();
	for (const string& chat : chat_queue) {
		g_player_input += chat + "\n";
	}

	g_chat_text.setString(g_player_input);
}

void send_packet(void* packet)
{
	unsigned short size = reinterpret_cast<unsigned short*>(packet)[0];
	unsigned char* p = reinterpret_cast<unsigned char*>(packet);
	size_t sent = size;
	s_socket.send(packet, p[0], sent);
}

void client_initialize()
{
	board = new sf::Texture;
	pieces = new sf::Texture;
	monster = new sf::Texture;

	board->loadFromFile("chessmap.bmp");
	pieces->loadFromFile("chess2.png");
	monster->loadFromFile("monster.png");
	if (false == g_font.loadFromFile("NanumGothic.ttf")) {
		cout << "Font Loading Error!\n";
		exit(-1);
	}
	white_tile = OBJECT{ *board, 5, 5, TILE_WIDTH, TILE_HEIGHT };
	black_tile = OBJECT{ *board, 69, 5, TILE_WIDTH, TILE_HEIGHT };
	avatar = OBJECT{ *pieces, 320, 0, TILE_WIDTH, TILE_HEIGHT };
	avatar.move(4, 4);

	g_player_text.setPosition(10, 10);
	g_player_text.setFont(g_font);

	g_chat_text.setPosition(10, WINDOW_HEIGHT - 300);
	g_chat_text.setFillColor(sf::Color::White);
	g_chat_text.setFont(g_font);
	g_chat_text.setCharacterSize(15);
	g_chat_flag = false;

	g_chat_rect.setFillColor(sf::Color(0, 0, 0, 80));
	g_chat_rect.setPosition(10, WINDOW_HEIGHT - 300);

	g_cur_text.setFont(g_font);
	g_cur_text.setPosition(10, WINDOW_HEIGHT - 50);
	g_cur_text.setFillColor(sf::Color::Red);
	g_cur_text.setFont(g_font);
	g_cur_text.setCharacterSize(15);

}

void client_finish()
{
	CS_LOGOUT_PACKET p{};
	p.size = sizeof p;
	p.type = CS_LOGOUT;

	send_packet(&p);

	players.clear();
	delete board;
	delete pieces;
}

void process_packet(char* ptr)
{
	static bool first_time = true;
	char type = reinterpret_cast<char*>(ptr)[2];
	switch (type)
	{
	case SC_LOGIN_INFO:
	{
		SC_LOGIN_INFO_PACKET* packet = reinterpret_cast<SC_LOGIN_INFO_PACKET*>(ptr);
		g_myid = packet->id;
		avatar.id = g_myid;
		avatar.move(packet->x, packet->y);
		g_left_x = packet->x - SCREEN_WIDTH / 2;
		g_top_y = packet->y - SCREEN_HEIGHT / 2;
		avatar.show();
		avatar.set_stat(packet->max_hp, packet->hp, packet->level, packet->exp);
		break;
	}

	case SC_ADD_OBJECT:
	{
		SC_ADD_OBJECT_PACKET* my_packet = reinterpret_cast<SC_ADD_OBJECT_PACKET*>(ptr);
		int id = my_packet->id;

		if (id == g_myid) {
			avatar.move(my_packet->x, my_packet->y);
			g_left_x = my_packet->x - SCREEN_WIDTH / 2;
			g_top_y = my_packet->y - SCREEN_HEIGHT / 2;
			avatar.show();
		}
		else if (id < MAX_USER) {
			players[id] = OBJECT{ *pieces, 256, 0, TILE_WIDTH, TILE_HEIGHT };
			players[id].id = id;
			players[id].move(my_packet->x, my_packet->y);
			players[id].set_name(my_packet->name);
			players[id].show();
		}

		else {
			int img_width = 0;
			if (id >= MAX_USER && id < MAX_USER + NPC_TORTOISE) {
				// TORTOISE
				img_width = 70;
			}
			else if (id >= MAX_USER + NPC_TORTOISE && id < MAX_USER + NPC_TORTOISE + NPC_DRAGON) {
				// DRAGON
				img_width = 140;
			}
			else if (id >= MAX_USER + NPC_TORTOISE + NPC_DRAGON &&
				id < MAX_USER + NPC_TORTOISE + NPC_DRAGON + NPC_BIRD) {
				// BIRD
				img_width = 0;
			}
			else {
				// TIGER
				img_width = 210;
			}

			players[id] = OBJECT{ *monster, img_width, 0,  TILE_WIDTH, TILE_HEIGHT };
			players[id].id = id;
			players[id].move(my_packet->x, my_packet->y);
			players[id].set_name(my_packet->name);
			players[id].show();
		}
		break;
	}
	case SC_MOVE_OBJECT:
	{
		SC_MOVE_OBJECT_PACKET* my_packet = reinterpret_cast<SC_MOVE_OBJECT_PACKET*>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid) {
			avatar.move(my_packet->x, my_packet->y);
			g_left_x = my_packet->x - SCREEN_WIDTH / 2;
			g_top_y = my_packet->y - SCREEN_HEIGHT / 2;
		}
		else {
			players[other_id].move(my_packet->x, my_packet->y);
		}
		break;
	}

	case SC_REMOVE_OBJECT:
	{
		SC_REMOVE_OBJECT_PACKET* my_packet = reinterpret_cast<SC_REMOVE_OBJECT_PACKET*>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid) {
			avatar.hide();
		}
		else {
			players.erase(other_id);
		}
		break;
	}
	case SC_CHAT:
	{
		SC_CHAT_PACKET* my_packet = reinterpret_cast<SC_CHAT_PACKET*>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid) {
			avatar.set_chat(my_packet->mess);
		}
		else {
			string chat{ players[my_packet->id].name };
			chat += " : " + string(my_packet->mess);
			chat_queue.push_back(chat);
			set_input_chat();
		}

		break;
	}
	case SC_STAT_CHANGE: {
		SC_STAT_CHANGE_PACKET* my_packet = reinterpret_cast<SC_STAT_CHANGE_PACKET*>(ptr);
		avatar.level = my_packet->level;
		avatar.hp = my_packet->hp;
		avatar.max_hp = my_packet->max_hp;
		avatar.level = my_packet->level;
		avatar.exp = my_packet->exp;

		if (avatar.hp <= 0) {
			string chat{ avatar.name };
			chat += " died";
			chat_queue.push_back(chat);
			set_input_chat();
		}
		break;
	}
	case SC_HIT: {
		SC_HIT_PACKET* my_packet = reinterpret_cast<SC_HIT_PACKET*>(ptr);
		string temp{};
		if (my_packet->attacker == g_myid) {
			temp = std::format("{} Damage!", my_packet->damage);
		}
		else if (my_packet->victim == g_myid) {
			temp = std::format("{} damage from {}'s attack.", my_packet->damage, players[my_packet->attacker].name);
		}
		else {
			temp = std::format("{} damaged {} from {}'s attack.", players[my_packet->victim].name, my_packet->damage, players[my_packet->attacker].name);
		}
		chat_queue.push_back(temp);
		set_input_chat();
		break;
	}
	case SC_OBTAIN: {
		SC_OBTAIN_PAKET* my_packet = reinterpret_cast<SC_OBTAIN_PAKET*>(ptr);

		string temp{};
		if (my_packet->obtainer == g_myid) {
			temp = std::format("Gained {} EXP.", my_packet->exp);
		}
		else {
			temp = std::format("{} Gained {} EXP", players[my_packet->obtainer].name, my_packet->exp);
		}

		chat_queue.push_back(temp);
		set_input_chat();
		break;
	}
	case SC_LOGIN_FAIL:
	{
		printf("Faild to login\n");
		exit(-1);
		break;
	}

	default:
		printf("Unknown PACKET type [%d]\n", ptr[2]);
	}
}

void process_data(char* net_buf, size_t io_byte)
{
	for (size_t i = 0; i < io_byte; ++i) {
		buf.push_back(net_buf[i]);
	}
	static size_t size = 0;
	while (io_byte > 1) {
		size = buf[0] + buf[1] * 256;
		if (size <= buf.size()) {
			process_packet(buf.data());
			buf.erase(buf.begin(), buf.begin() + size);
			io_byte -= size;
		}
		else {
			break;
		}
	}
}

void client_main()
{
	char net_buf[BUF_SIZE];
	size_t	received;

	auto recv_result = s_socket.receive(net_buf, BUF_SIZE, received);
	if (recv_result == sf::Socket::Error)
	{
		wcout << L"Recv Error!";
		exit(-1);
	}
	if (recv_result == sf::Socket::Disconnected) {
		wcout << L"Disconnected\n";
		exit(-1);
	}
	if (recv_result != sf::Socket::NotReady)
		if (received > 1) process_data(net_buf, received);

	for (int i = 0; i < SCREEN_WIDTH; ++i)
		for (int j = 0; j < SCREEN_HEIGHT; ++j) {
			int tile_x = i + g_left_x;
			int tile_y = j + g_top_y;
			if ((tile_x < 0) || (tile_y < 0)) continue;
			if (0 == (tile_x / 3 + tile_y / 3) % 2) {
				white_tile.a_move(TILE_WIDTH * i, TILE_HEIGHT * j);
				white_tile.a_draw();
			}
			else
			{
				black_tile.a_move(TILE_WIDTH * i, TILE_HEIGHT * j);
				black_tile.a_draw();
			}
		}
	avatar.draw();
	for (auto& pl : players)
		pl.second.draw();
	char buf[100];
	sprintf_s(buf, "(%d, %d)\n HP : %d/%d\n LEVEL : %d\n EXP : %d", avatar.m_x, avatar.m_y, avatar.hp, avatar.max_hp, avatar.level, avatar.exp);
	g_player_text.setString(buf);
	g_window->draw(g_player_text);
	g_window->draw(g_chat_rect);
	g_window->draw(g_cur_text);
	g_window->draw(g_chat_text);

}

void send_move_packet(int dir)
{
	CS_MOVE_PACKET p{};
	p.size = sizeof(p);
	p.type = CS_MOVE;
	p.direction = dir;
	send_packet(&p);
}

void send_attack_packet()
{
	CS_ATTACK_PACKET p{};
	p.size = sizeof(p);
	p.type = CS_ATTACK;
	send_packet(&p);
}

void send_chat_packet()
{
	CS_CHAT_PACKET p{};
	strcpy_s(p.mess, g_cur_string.c_str());
	p.size = strlen(p.mess) + 1 + sizeof(p.size) + sizeof(p.type);
	p.type = CS_CHAT;
	send_packet(&p);

	string temp = string(avatar.name) + " : ";
	chat_queue.push_back(temp + g_cur_string);
	set_input_chat();

	g_cur_string.clear();
	g_cur_text.setString(L"");
}

int main()
{
	wcout.imbue(locale("korean"));
	sf::Socket::Status status = s_socket.connect("127.0.0.1", PORT_NUM);
	s_socket.setBlocking(false);

	if (status != sf::Socket::Done) {
		s_socket.Error;
		exit(-1);
	}

	client_initialize();

	CS_LOGIN_PACKET p{};
	p.size = sizeof(p);
	p.type = CS_LOGIN;

	string player_name;
	cout << "ID : ";
	cin >> player_name;
	strcpy_s(p.name, player_name.c_str());
	send_packet(&p);
	avatar.set_name(p.name);
	sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "2D CLIENT");
	g_window = &window;

	queue<string> message_queue;


	while (window.isOpen())
	{
		sf::Event event;
		while (window.pollEvent(event))
		{
			if (event.type == sf::Event::Closed)
				window.close();

			if (event.type == sf::Event::KeyPressed) {
				int direction = -1;
				switch (event.key.code) {
				case sf::Keyboard::Left:
					send_move_packet(2);
					break;
				case sf::Keyboard::Right:
					send_move_packet(3);
					break;
				case sf::Keyboard::Up:
					send_move_packet(0);
					break;
				case sf::Keyboard::Down:
					send_move_packet(1);
					break;
				case sf::Keyboard::LControl:
					send_attack_packet();
					break;
				case sf::Keyboard::Enter:
					g_chat_flag = !g_chat_flag;
					if (!g_chat_flag) {
						send_chat_packet();
					}
					break;
				case sf::Keyboard::Escape:
					window.close();
					break;
				}
			}

			else if (event.type == sf::Event::TextEntered && g_chat_flag) {
				if (event.text.unicode == 8 && g_cur_string.size() > 0)
					g_cur_string.pop_back();
				else if (g_cur_string.size() < CHAT_SIZE) {
					if (event.text.unicode != 13) {
						std::u32string u32String(1, event.text.unicode);
						std::string utf8String(u32String.begin(), u32String.end());
						g_cur_string += utf8String;
					}
				}
				g_cur_text.setString(g_cur_string);
			}
		}

		window.clear();
		client_main();
		window.display();
	}
	client_finish();

	return 0;
}