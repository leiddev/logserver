/*
 * Leid 2025.2
 */
#pragma once
#ifndef _UTILITY_HH_
#define _UTILITY_HH_

#ifdef _WIN32
bool isTempFS(std::string mount_path);
int mkpath(std::string &s);

#else
bool isTempFS(std::string mount_path);
long getFileSystemType(std::string mount_path);
int mkpath(std::string &s, mode_t mode);

#endif

#endif  // !_UTILITY_HH_
