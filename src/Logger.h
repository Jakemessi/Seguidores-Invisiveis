#pragma once

#include <string>

// Mostra texto no console interno do Skyrim.
void Print(const std::string& msg);

// Registra uma informacao em portugues ou ingles.
void LogInfo(
    const std::string& pt,
    const std::string& en
);

// Registra um erro em portugues ou ingles.
void LogErro(
    const std::string& pt,
    const std::string& en
);

// Prepara o arquivo de log e escolhe o idioma.
void IniciarLog();

// Retorna se o idioma atual e portugues.
bool VaiBrasa();