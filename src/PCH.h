#pragma once

// This file is required.
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <SKSE/Logger.h>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

//Para poder mudar log para português ou inglês
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
//------------------------------------------------------------

using namespace std::literals;
namespace logger = SKSE::log;