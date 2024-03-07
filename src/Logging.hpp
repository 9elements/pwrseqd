#pragma once
extern int _loglevel;

void log_debug(const std::string& s);
void log_info(const std::string& s);
void log_err(const std::string& s);
void log_sel(const std::string& data, const std::string& path, const bool& assert);
