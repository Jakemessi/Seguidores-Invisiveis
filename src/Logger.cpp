#include "PCH.h"
#include "Logger.h"
#include "Version.h"

// Permite criar arquivo de log simples com spdlog.
#include <spdlog/sinks/basic_file_sink.h>

// Biblioteca principal do spdlog para escrever logs.
#include <spdlog/spdlog.h>

// WIN32_LEAN_AND_MEAN é uma macro usada antes de incluir Windows.h.
// Ela faz o Windows.h carregar menos coisas antigas ou pouco usadas,
// deixando o include mais leve e reduzindo possíveis conflitos.
// O #ifndef evita redefinir a macro caso ela já tenha sido definida pelo CMake.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// NOMINMAX impede que o Windows.h crie macros chamadas min e max.
// Sem isso, essas macros podem entrar em conflito com funções modernas do C++,
// como std::min e std::max.
// O #ifndef garante que só definimos NOMINMAX se ela ainda não existir.
#ifndef NOMINMAX
#define NOMINMAX
#endif

// Inclui a API do Windows.
// Neste plugin, é usada para detectar o idioma do sistema.
#include <Windows.h>

//Para poder mudar log para português ou inglês e se o Logger Iniciou
bool VaiBrasa = false;
bool g_loggerIniciado = false;

void Print(const std::string& msg)
{
    if (auto* console = RE::ConsoleLog::GetSingleton()) {
        console->Print(msg.c_str());
    }
}

void LogInfo(const std::string& pt, const std::string& en)
{
    const std::string& msg = VaiBrasa ? pt : en;

    if (g_loggerIniciado) {
        logger::info("{}", msg);
    } else {
        Print(msg);
    }
}

void LogErro(const std::string& pt, const std::string& en)
{
    const std::string& msg = VaiBrasa ? pt : en;

    if (g_loggerIniciado) {
        logger::error("{}", msg);
    } else {
        Print(msg);
    }
}

std::string DetectarIdioma(bool& success)
{
    success = false;

    wchar_t localeName[LOCALE_NAME_MAX_LENGTH]{};

    if (!GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH)) {
        return "unknown";
    }

    int sizeNeeded = WideCharToMultiByte(
        CP_UTF8,
        0,
        localeName,
        -1,
        nullptr,
        0,
        nullptr,
        nullptr);

    if (sizeNeeded <= 0) {
        return "unknown";
    }

    std::string result(sizeNeeded, '\0');

    int converted = WideCharToMultiByte(
        CP_UTF8,
        0,
        localeName,
        -1,
        result.data(),
        sizeNeeded,
        nullptr,
        nullptr);

    if (converted <= 0) {
        return "unknown";
    }

    if (!result.empty() && result.back() == '\0') {
        result.pop_back();
    }

    success = true;
    return result;
}

void IniciarLog()
{
    if (g_loggerIniciado) {
        return;
    }

    bool localeOk = false;
    std::string localeName = DetectarIdioma(localeOk);

    VaiBrasa = localeName.starts_with("pt");

    auto path = logger::log_directory();

    if (!path) {
        Print("logger::log_directory() returned nullptr");
        return;
    }

    *path /= "InvisibilityAffectsFollowersToo.log";

    try {
        std::filesystem::create_directories(path->parent_path());

        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            path->string(),
            true);

        auto log = std::make_shared<spdlog::logger>(
            "global log",
            std::move(sink));

        log->set_level(spdlog::level::debug);
        log->flush_on(spdlog::level::info);

        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("[%H:%M:%S] [%l] %v");

        g_loggerIniciado = true;
    }
    catch (const std::exception& e) {
        Print(std::string("Failed to initialize logger: ") + e.what());
        return;
    }

    logger::info("{} v{} - Logger Made In Arstotzka", PLUGIN_NAME, PLUGIN_VERSION_STRING);

    if (VaiBrasa) {
        logger::info("Logger inicializado");
        logger::info("Idioma detectado: {}", localeName);
    } else {
        logger::info("Logger initialized");
        logger::info("Detected language: {}", localeName);
    }

    logger::info("Vai Corinthians!");

    if (!localeOk) {
        logger::error("GetUserDefaultLocaleName failed");
    }
}