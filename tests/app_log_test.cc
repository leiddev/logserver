// SPDX-License-Identifier: MIT
/*
 * Leid 2026.11
 */
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

// Unit tests for applog: message formatting, raw writes, and file archiving.
// The file tests need a writable tmpfs on Linux; each case uses its own
// subdirectory so the process wide logger starts from a clean state.

#include "app_log.hh"
#include "test_framework.hh"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

#ifdef _WIN32
const char kPathSeparator = '\\';

std::string testRoot() {
  char buffer[MAX_PATH] = {0};
  if (::GetTempPathA(MAX_PATH, buffer) == 0) {
    return ".\\logserver_test\\";
  }
  std::string root(buffer);
  if (!root.empty() && root[root.size() - 1] != kPathSeparator) {
    root += kPathSeparator;
  }
  return root + "logserver_test\\";
}

void makeDir(const std::string &path) {
  ::CreateDirectoryA(path.c_str(), nullptr);
}

void removeDir(const std::string &path) {
  std::string pattern = path + "*";
  WIN32_FIND_DATAA data;
  HANDLE handle = ::FindFirstFileA(pattern.c_str(), &data);
  if (handle != INVALID_HANDLE_VALUE) {
    do {
      if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        ::DeleteFileA((path + data.cFileName).c_str());
      }
    } while (::FindNextFileA(handle, &data) != 0);
    ::FindClose(handle);
  }
  ::RemoveDirectoryA(path.c_str());
}

#else
const char kPathSeparator = '/';

std::string testRoot() {
  // /dev/shm is a tmpfs on Linux, which is what initFile requires.
  return "/dev/shm/logserver_test_logs/";
}

void makeDir(const std::string &path) {
  ::mkdir(path.c_str(), 0700);
}

void removeDir(const std::string &path) {
  DIR *dir = ::opendir(path.c_str());
  if (dir) {
    struct dirent *entry;
    while ((entry = ::readdir(dir)) != nullptr) {
      std::string name = entry->d_name;
      if (name != "." && name != "..") {
        std::remove((path + name).c_str());
      }
    }
    ::closedir(dir);
  }
  ::rmdir(path.c_str());
}

#endif

// Each case gets its own directory: the logger is a singleton, but initFile
// points it at a fresh path and reopens the file there.
std::string prepareDir(const std::string &name) {
  const std::string root = testRoot();
  const std::string dir = root + name + kPathSeparator;
  removeDir(dir);
  makeDir(root);
  makeDir(dir);
  return dir;
}

std::vector<std::string> listFiles(const std::string &dir) {
  std::vector<std::string> names;

#ifdef _WIN32
  WIN32_FIND_DATAA data;
  HANDLE handle = ::FindFirstFileA((dir + "*").c_str(), &data);
  if (handle == INVALID_HANDLE_VALUE) {
    return names;
  }
  do {
    if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
      names.push_back(std::string(data.cFileName));
    }
  } while (::FindNextFileA(handle, &data) != 0);
  ::FindClose(handle);
#else
  DIR *handle = ::opendir(dir.c_str());
  if (!handle) {
    return names;
  }
  struct dirent *entry;
  while ((entry = ::readdir(handle)) != nullptr) {
    std::string name = entry->d_name;
    if (name != "." && name != "..") {
      names.push_back(name);
    }
  }
  ::closedir(handle);
#endif

  for (std::size_t i = 0; i + 1 < names.size(); ++i) {
    for (std::size_t k = 0; k + 1 < names.size() - i; ++k) {
      if (names[k] > names[k + 1]) {
        std::string tmp = names[k];
        names[k] = names[k + 1];
        names[k + 1] = tmp;
      }
    }
  }
  return names;
}

bool readFile(const std::string &path, std::string *out) {
  std::FILE *file = std::fopen(path.c_str(), "rb");
  if (!file) {
    return false;
  }
  out->clear();
  char buffer[4096];
  std::size_t got;
  while ((got = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
    out->append(buffer, got);
  }
  std::fclose(file);
  return true;
}

long fileSize(const std::string &path) {
  std::FILE *file = std::fopen(path.c_str(), "rb");
  if (!file) {
    return -1;
  }
  std::fseek(file, 0, SEEK_END);
  long size = std::ftell(file);
  std::fclose(file);
  return size;
}

bool hasPrefix(const std::string &value, const std::string &prefix) {
  return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

bool hasSuffix(const std::string &value, const std::string &suffix) {
  return value.size() >= suffix.size() &&
         value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// <base>_YYYYMMDD_HHMMSS_mmm.log, with an optional _NNN index when two
// rotations land in the same millisecond.
bool isArchiveName(const std::string &name) {
  const std::string base = "logserver";
  if (!hasPrefix(name, base + "_") || !hasSuffix(name, ".log")) {
    return false;
  }
  std::string digits = name.substr(base.size() + 1, name.size() - 4 - (base.size() + 1));

  // Collision suffix: _NNN with exactly three digits.
  if (digits.size() == 8 + 1 + 6 + 1 + 3 + 1 + 3 && digits[digits.size() - 4] == '_') {
    for (std::size_t i = digits.size() - 3; i < digits.size(); ++i) {
      if (digits[i] < '0' || digits[i] > '9') {
        return false;
      }
    }
    digits = digits.substr(0, digits.size() - 4);
  }

  if (digits.size() != 8 + 1 + 6 + 1 + 3) {
    return false;
  }
  for (std::size_t i = 0; i < digits.size(); ++i) {
    const char c = digits[i];
    const bool separator = (c == '_');
    const bool digit = (c >= '0' && c <= '9');
    if (!separator && !digit) {
      return false;
    }
  }
  return true;
}

}  // namespace

TEST_CASE("format replaces {} in order") {
  CHECK_EQ(std::string("a1b2"), applog::format("a{}b{}", 1, 2));
  CHECK_EQ(std::string("x"), applog::format("{}", "x"));
  CHECK_EQ(std::string("no placeholder"), applog::format("no placeholder", 42));
  CHECK_EQ(std::string("dangling 1"), applog::format("dangling {}", 1, 2, 3));
}

TEST_CASE("format renders bool as true/false") {
  CHECK_EQ(std::string("true"), applog::format("{}", true));
  CHECK_EQ(std::string("false"), applog::format("{}", false));
}

TEST_CASE("format accepts numbers and std::string") {
  CHECK_EQ(std::string("42"), applog::format("{}", 42));
  CHECK_EQ(std::string("3.5"), applog::format("{}", 3.5));
  CHECK_EQ(std::string("hello"), applog::format("{}", std::string("hello")));
  CHECK_EQ(std::string("hello"), applog::format("{}", "hello"));
}

TEST_CASE("initFile rejects max_files == 0 without creating the directory") {
  const std::string dir = testRoot() + "no_dir_for_zero" + kPathSeparator;
  removeDir(dir);

  applog::Logger &logger = applog::Logger::instance();
  CHECK(!logger.initFile(dir, "logserver", 1024, 0));
  CHECK(listFiles(dir).empty());

  logger.writeRaw("ignored\n", 8);
  CHECK(listFiles(dir).empty());
}

TEST_CASE("initFile rejects max_file_size == 0") {
  const std::string dir = prepareDir("zero_size");
  applog::Logger &logger = applog::Logger::instance();
  CHECK(!logger.initFile(dir, "logserver", 0, 5));
  CHECK(listFiles(dir).empty());
}

TEST_CASE("initFile rejects a path that exists as a regular file") {
  const std::string root = testRoot();
  const std::string path = root + "not_a_dir";
  makeDir(root);
  std::remove(path.c_str());

  std::FILE *file = std::fopen(path.c_str(), "wb");
  CHECK(file != nullptr);
  if (file) {
    std::fwrite("x", 1, 1, file);
    std::fclose(file);
  }

  applog::Logger &logger = applog::Logger::instance();
  CHECK(!logger.initFile(path + kPathSeparator, "logserver", 1024, 5));

  // The existing file must be left alone.
  std::string content;
  CHECK(readFile(path, &content));
  CHECK_EQ(std::string("x"), content);

  std::remove(path.c_str());
}

TEST_CASE("raw writes are neither formatted nor terminated") {
  const std::string dir = prepareDir("raw_write");
  applog::Logger &logger = applog::Logger::instance();
  CHECK(logger.initFile(dir, "logserver", 1024 * 1024, 5));

  const char payload[] = "raw {braces} 100%\0after-nul";
  logger.writeRaw(payload, sizeof(payload) - 1);

  std::string content;
  CHECK(readFile(dir + "logserver.log", &content));
  CHECK_EQ(std::string(payload, sizeof(payload) - 1), content);
}

TEST_CASE("internal messages keep the [APP] prefix and go to the file") {
  const std::string dir = prepareDir("internal_msg");
  applog::Logger &logger = applog::Logger::instance();
  logger.setLevel(applog::kTrace);
  CHECK(logger.initFile(dir, "logserver", 1024 * 1024, 5));

  logger.writef(applog::kInfo, "[APP] hello {}\n", "world");

  std::string content;
  CHECK(readFile(dir + "logserver.log", &content));
  CHECK(content.find("[APP] hello world\n") != std::string::npos);
}

TEST_CASE("level filters out messages below the threshold") {
  const std::string dir = prepareDir("level_filter");
  applog::Logger &logger = applog::Logger::instance();
  logger.setLevel(applog::kError);
  CHECK(logger.initFile(dir, "logserver", 1024 * 1024, 5));

  const long before = fileSize(dir + "logserver.log");
  logger.writef(applog::kInfo, "[APP] dropped\n");
  CHECK_EQ(before, fileSize(dir + "logserver.log"));

  logger.writef(applog::kError, "[APP] kept\n");
  CHECK(fileSize(dir + "logserver.log") > before);
  logger.setLevel(applog::kTrace);
}

TEST_CASE("rotation archives with a timestamp and honours max_files") {
  const std::string dir = prepareDir("rotation");
  const std::size_t max_size = 16 * 1024;
  const std::size_t max_files = 4;

  applog::Logger &logger = applog::Logger::instance();
  logger.setLevel(applog::kTrace);
  CHECK(logger.initFile(dir, "logserver", max_size, max_files));

  std::string chunk(999, 'R');
  chunk += '\n';
  for (int i = 0; i < 100; ++i) {
    logger.writeRaw(chunk.data(), chunk.size());
  }

  const std::vector<std::string> files = listFiles(dir);
  CHECK_EQ(max_files, files.size());

  std::size_t current = 0;
  std::size_t archives = 0;
  bool size_ok = true;
  bool naming_ok = true;
  for (std::size_t i = 0; i < files.size(); ++i) {
    if (files[i] == "logserver.log") {
      ++current;
      continue;
    }
    ++archives;
    if (!isArchiveName(files[i])) {
      naming_ok = false;
      std::fprintf(stderr, "unexpected archive name: %s\n", files[i].c_str());
    }
    if (fileSize(dir + files[i]) > static_cast<long>(max_size)) {
      size_ok = false;
    }
  }
  CHECK_EQ(std::size_t(1), current);
  CHECK_EQ(max_files - 1, archives);
  CHECK(naming_ok);
  CHECK(size_ok);
}

TEST_CASE("initFile again appends to the existing current file") {
  const std::string dir = prepareDir("restart");
  applog::Logger &logger = applog::Logger::instance();

  CHECK(logger.initFile(dir, "logserver", 1024 * 1024, 5));
  logger.writeRaw("first\n", 6);
  const long after_first = fileSize(dir + "logserver.log");
  CHECK_EQ(long(6), after_first);

  CHECK(logger.initFile(dir, "logserver", 1024 * 1024, 5));
  logger.writeRaw("second\n", 7);
  CHECK_EQ(after_first + 7, fileSize(dir + "logserver.log"));

  std::string content;
  CHECK(readFile(dir + "logserver.log", &content));
  CHECK_EQ(std::string("first\nsecond\n"), content);
}

TEST_CASE("cleanup removes only this base name") {
  const std::string dir = prepareDir("cleanup");
  applog::Logger &logger = applog::Logger::instance();

  // A file that does not follow the naming rule must be left alone.
  {
    std::FILE *other = std::fopen((dir + "keep_me.txt").c_str(), "wb");
    CHECK(other != nullptr);
    if (other) {
      std::fwrite("x", 1, 1, other);
      std::fclose(other);
    }
  }

  CHECK(logger.initFile(dir, "logserver", 8 * 1024, 2));
  std::string chunk(999, 'C');
  chunk += '\n';
  for (int i = 0; i < 40; ++i) {
    logger.writeRaw(chunk.data(), chunk.size());
  }

  const std::vector<std::string> files = listFiles(dir);
  std::size_t log_files = 0;
  bool keep_me_present = false;
  for (std::size_t i = 0; i < files.size(); ++i) {
    if (files[i] == "keep_me.txt") {
      keep_me_present = true;
    } else {
      ++log_files;
    }
  }
  CHECK_EQ(std::size_t(2), log_files);
  CHECK(keep_me_present);
}

int main() {
  const int result = testfw::run_all();
  removeDir(testRoot());
  return result;
}
