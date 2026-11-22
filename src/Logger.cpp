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
// Guarda o idioma escolhido e se o arquivo de log ja foi iniciado.
namespace
{
    bool g_vaiBrasa = false;
    bool g_loggerIniciado = false;
}

// Mostra uma mensagem no console interno do Skyrim, se estiver disponivel.
void Print(const std::string& msg)
{
    // Evita acessar o console caso ele ainda nao exista.
    if (auto* console = RE::ConsoleLog::GetSingleton()) {
        console->Print(msg.c_str());
    }
}

// Escreve uma mensagem informativa no idioma configurado.
void LogInfo(const std::string& pt, const std::string& en)
{
    // Escolhe o texto em portugues ou ingles.
    const std::string& msg = g_vaiBrasa ? pt : en;

    // Usa o arquivo de log; sem ele, mostra a mensagem no console.
    if (g_loggerIniciado) {
        logger::info("{}", msg);
    } else {
        Print(msg);
    }
}

// Escreve erros no idioma configurado, com retorno ao console se preciso.
void LogErro(const std::string& pt, const std::string& en)
{
    const std::string& msg = g_vaiBrasa ? pt : en;

    // Usa o logger quando disponivel; caso contrario, escreve no console.
    if (g_loggerIniciado) {
        logger::error("{}", msg);
    } else {
        Print(msg);
    }
}

// Informa se o idioma selecionado para as mensagens e portugues.
bool VaiBrasa()
{
    return g_vaiBrasa;
}

// Detecta o idioma do Windows e informa se a consulta deu certo.
std::string DetectarIdioma(bool& success)
{
    success = false;

    wchar_t localeName[LOCALE_NAME_MAX_LENGTH]{};

    // Se o Windows nao informar o idioma, retorna um valor desconhecido.
    if (!GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH)) {
        return "unknown";
    }

    // Descobre quantos bytes sao necessarios para converter o idioma em UTF-8.
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

    // Reserva o espaco exato para receber a conversao.
    std::string result(sizeNeeded, '\0');

    // Converte o nome do idioma do Windows para uma string UTF-8.
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

    // Remove o terminador nulo que nao precisa aparecer na string.
    if (!result.empty() && result.back() == '\0') {
        result.pop_back();
    }

    success = true;
    return result;
}

// Configura o arquivo de log e define o idioma das mensagens.
void IniciarLog()
{
    // Evita inicializar o mesmo logger mais de uma vez.
    if (g_loggerIniciado) {
        return;
    }

    bool localeOk = false;
    std::string localeName = DetectarIdioma(localeOk);

    // Usa portugues quando o idioma do Windows comeca com "pt".
    g_vaiBrasa = localeName.starts_with("pt");

    // Encontra o diretorio de logs criado pelo SKSE.
    auto path = logger::log_directory();

    if (!path) {
        Print("logger::log_directory() returned nullptr");
        return;
    }

    // Define o nome do arquivo onde as mensagens serao gravadas.
    *path /= "InvisibilityAffectsFollowersToo.log";

    // Prepara a pasta e cria o logger que grava em arquivo.
    try {
        std::filesystem::create_directories(path->parent_path());

        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            path->string(),
            true);

        auto log = std::make_shared<spdlog::logger>(
            "global log",
            std::move(sink));

        // Define o nivel das mensagens e quando enviar dados ao arquivo.
        log->set_level(spdlog::level::debug);
        log->flush_on(spdlog::level::info);

        // Ativa o logger e padroniza data, nivel e texto das mensagens.
        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("[%H:%M:%S] [%l] %v");

        g_loggerIniciado = true;
    }
    // Se a criacao falhar, informa o erro no console.
    catch (const std::exception& e) {
        Print(std::string("Failed to initialize logger: ") + e.what());
        return;
    }

    // Identifica o plugin e sua versao no inicio do log.
    logger::info("{} v{} - Logger Made In Arstotzka", PLUGIN_NAME, PLUGIN_VERSION_STRING);

    // Registra o resultado da inicializacao no idioma escolhido.
    if (g_vaiBrasa) {
        logger::info("Logger inicializado");
        logger::info("Idioma detectado: {}", localeName);
    } else {
        logger::info("Logger initialized");
        logger::info("Detected language: {}", localeName);
    }

    logger::info("Vai Corinthians!");

    // Registra uma falha de deteccao de idioma, se houver.
    if (!localeOk) {
        logger::error("GetUserDefaultLocaleName failed");
    }
}