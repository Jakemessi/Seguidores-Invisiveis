#pragma once

extern bool VaiBrasa;
extern bool g_loggerIniciado;

void Print(const std::string& msg);

void LogInfo(const std::string& pt, const std::string& en);

void LogErro(const std::string& pt, const std::string& en);

void IniciarLog();