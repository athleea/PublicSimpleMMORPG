#pragma once
#include <array>
#include <atomic>
#include <chrono>
#include <format>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <ranges>
#include <queue>

#include <concurrent_queue.h>
#include <concurrent_priority_queue.h>
#include <concurrent_unordered_map.h>

#include <WS2tcpip.h>
#include <MSWSock.h>
#include <sqlext.h>
#include <locale.h>

#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MSWSock.lib")
#pragma comment(lib, "lua54.lib")

#include "include/lua.hpp"
#include "../protocol.h"

constexpr int BUF_SIZE = 4096;


