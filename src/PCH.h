#pragma once

// Cabecalho compartilhado para acelerar a compilacao dos arquivos C++.
// Reune as bibliotecas usadas em varias partes do plugin.
// This file is required.

// Disponibiliza os tipos do Skyrim, as interfaces SKSE e o logger.
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <SKSE/Logger.h>

// Bibliotecas padrao para arquivos, funcoes, memoria, texto e listas.
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Facilita o uso de literais C++ e do logger do SKSE.
using namespace std::literals;
namespace logger = SKSE::log;