// SPDX-License-Identifier: MIT
/*
 * Leid 2026.11
 *
 * Unit tests for applog: message formatting, raw writes, and file archiving.
 * The file tests need a writable tmpfs on Linux; each case uses its own
 * subdirectory so the process wide logger starts from a clean state.
 */
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

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
#include <linux/magic.h>
#include <sys/stat.h>
#include <sys/statfs.h>
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

// initFile defaults to the .txt suffix, so these tests use it too.
const char kDefaultSuffix[] = ".txt";

std::string currentFile() {
  return std::string("logserver") + kDefaultSuffix;
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

// <base>_YYYYMMDD_HHMMSS_mmm<suffix>, with an optional _NNN index when two
// rotations land in the same millisecond.
bool isArchiveName(const std::string &name, const std::string &suffix) {
  const std::string base = "logserver";
  if (!hasPrefix(name, base + "_") || !hasSuffix(name, suffix)) {
    return false;
  }
  std::string digits = name.substr(base.size() + 1, name.size() - suffix.size() - (base.size() + 1));

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
  CHECK(readFile(dir + currentFile(), &content));
  CHECK_EQ(std::string(payload, sizeof(payload) - 1), content);
}

TEST_CASE("internal messages keep the [APP] prefix and go to the file") {
  const std::string dir = prepareDir("internal_msg");
  applog::Logger &logger = applog::Logger::instance();
  logger.setLevel(applog::kTrace);
  CHECK(logger.initFile(dir, "logserver", 1024 * 1024, 5));

  logger.writef(applog::kInfo, "[APP] hello {}\n", "world");

  std::string content;
  CHECK(readFile(dir + currentFile(), &content));
  CHECK(content.find("[APP] hello world\n") != std::string::npos);
}

TEST_CASE("level filters out messages below the threshold") {
  const std::string dir = prepareDir("level_filter");
  applog::Logger &logger = applog::Logger::instance();
  logger.setLevel(applog::kError);
  CHECK(logger.initFile(dir, "logserver", 1024 * 1024, 5));

  const long before = fileSize(dir + currentFile());
  logger.writef(applog::kInfo, "[APP] dropped\n");
  CHECK_EQ(before, fileSize(dir + currentFile()));

  logger.writef(applog::kError, "[APP] kept\n");
  CHECK(fileSize(dir + currentFile()) > before);
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
    if (files[i] == currentFile()) {
      ++current;
      continue;
    }
    ++archives;
    if (!isArchiveName(files[i], kDefaultSuffix)) {
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

TEST_CASE("a .log suffix renames the current file and its archives") {
  const std::string dir = prepareDir("suffix_log");
  const std::string suffix = ".log";
  const std::size_t max_size = 16 * 1024;
  const std::size_t max_files = 3;

  applog::Logger &logger = applog::Logger::instance();
  logger.setLevel(applog::kTrace);
  CHECK(logger.initFile(dir, "logserver", max_size, max_files, suffix));
  CHECK(logger.filePath() == dir + "logserver.log");

  std::string chunk(999, 'S');
  chunk += '\n';
  for (int i = 0; i < 100; ++i) {
    logger.writeRaw(chunk.data(), chunk.size());
  }

  const std::vector<std::string> files = listFiles(dir);
  CHECK_EQ(max_files, files.size());

  bool naming_ok = true;
  std::size_t current = 0;
  for (std::size_t i = 0; i < files.size(); ++i) {
    if (files[i] == "logserver.log") {
      ++current;
      continue;
    }
    if (!isArchiveName(files[i], suffix)) {
      naming_ok = false;
      std::fprintf(stderr, "unexpected archive name: %s\n", files[i].c_str());
    }
  }
  CHECK_EQ(std::size_t(1), current);
  CHECK(naming_ok);
}

TEST_CASE("cleanup ignores files with the other suffix") {
  const std::string dir = prepareDir("suffix_mix");
  const std::size_t max_size = 8 * 1024;

  // Leftovers from a run that used the .txt suffix.
  {
    std::FILE *txt = std::fopen((dir + "logserver_20260101_000000_000.txt").c_str(), "wb");
    CHECK(txt != nullptr);
    if (txt) {
      std::fwrite("x", 1, 1, txt);
      std::fclose(txt);
    }
  }

  applog::Logger &logger = applog::Logger::instance();
  CHECK(logger.initFile(dir, "logserver", max_size, 2, ".log"));

  std::string chunk(999, 'T');
  chunk += '\n';
  for (int i = 0; i < 40; ++i) {
    logger.writeRaw(chunk.data(), chunk.size());
  }

  const std::vector<std::string> files = listFiles(dir);
  std::size_t log_files = 0;
  bool txt_kept = false;
  for (std::size_t i = 0; i < files.size(); ++i) {
    if (hasSuffix(files[i], ".txt")) {
      txt_kept = true;
    } else {
      ++log_files;
    }
  }
  CHECK_EQ(std::size_t(2), log_files);
  CHECK(txt_kept);
}

TEST_CASE("initFile again appends to the existing current file") {
  const std::string dir = prepareDir("restart");
  applog::Logger &logger = applog::Logger::instance();

  CHECK(logger.initFile(dir, "logserver", 1024 * 1024, 5));
  logger.writeRaw("first\n", 6);
  const long after_first = fileSize(dir + currentFile());
  CHECK_EQ(long(6), after_first);

  CHECK(logger.initFile(dir, "logserver", 1024 * 1024, 5));
  logger.writeRaw("second\n", 7);
  CHECK_EQ(after_first + 7, fileSize(dir + currentFile()));

  std::string content;
  CHECK(readFile(dir + currentFile(), &content));
  CHECK_EQ(std::string("first\nsecond\n"), content);
}

#ifndef _WIN32
// On Windows an open file cannot be deleted, so these cases are POSIX only.
// The logger re-checks the file at most every 32 writes or every second, so
// the assertions look at what lands after the check rather than exact counts.

// True when every line in the file is one chunk of `size` bytes.
bool onlyWholeRecords(const std::string &content, std::size_t size) {
  if (content.empty() || content.size() % size != 0) {
    return false;
  }
  for (std::size_t pos = 0; pos < content.size(); pos += size) {
    if (content[pos + size - 1] != '\n') {
      return false;
    }
  }
  return true;
}

TEST_CASE("recreates the log file after it is removed") {
  const std::string dir = prepareDir("recreate_removed");
  applog::Logger &logger = applog::Logger::instance();
  logger.setLevel(applog::kTrace);
  CHECK(logger.initFile(dir, "logserver", 1024 * 1024, 5));

  const std::string chunk(99, 'A');
  std::string record = chunk + '\n';
  for (int i = 0; i < 40; ++i) {
    logger.writeRaw(record.data(), record.size());
  }
  CHECK(fileSize(dir + currentFile()) > 0);

  std::remove((dir + currentFile()).c_str());
  for (int i = 0; i < 100; ++i) {
    logger.writeRaw(record.data(), record.size());
  }

  std::string content;
  CHECK(readFile(dir + currentFile(), &content));
  CHECK(!content.empty());
  CHECK(onlyWholeRecords(content, record.size()));
}

TEST_CASE("recreates the log file and its directory after both are removed") {
  const std::string dir = prepareDir("recreate_dir");
  applog::Logger &logger = applog::Logger::instance();
  logger.setLevel(applog::kTrace);
  CHECK(logger.initFile(dir, "logserver", 1024 * 1024, 5));

  const std::string chunk(99, 'B');
  std::string record = chunk + '\n';
  for (int i = 0; i < 40; ++i) {
    logger.writeRaw(record.data(), record.size());
  }

  removeDir(dir);
  CHECK(fileSize(dir + currentFile()) < 0);

  for (int i = 0; i < 100; ++i) {
    logger.writeRaw(record.data(), record.size());
  }

  std::string content;
  CHECK(readFile(dir + currentFile(), &content));
  CHECK(!content.empty());
  CHECK(onlyWholeRecords(content, record.size()));
}

TEST_CASE("follows a file that was renamed away") {
  const std::string dir = prepareDir("recreate_renamed");
  applog::Logger &logger = applog::Logger::instance();
  logger.setLevel(applog::kTrace);
  CHECK(logger.initFile(dir, "logserver", 1024 * 1024, 5));

  const std::string chunk(99, 'C');
  std::string record = chunk + '\n';
  for (int i = 0; i < 40; ++i) {
    logger.writeRaw(record.data(), record.size());
  }

  CHECK_EQ(0, std::rename((dir + currentFile()).c_str(), (dir + "moved.txt").c_str()));
  for (int i = 0; i < 100; ++i) {
    logger.writeRaw(record.data(), record.size());
  }

  std::string content;
  CHECK(readFile(dir + currentFile(), &content));
  CHECK(!content.empty());
  CHECK(onlyWholeRecords(content, record.size()));

  // The renamed file keeps what was written before the move, plus whatever
  // was written before the next check noticed the move.
  std::string moved;
  CHECK(readFile(dir + "moved.txt", &moved));
  CHECK(moved.size() >= std::size_t(40 * 100));
  CHECK(onlyWholeRecords(moved, record.size()));
}

TEST_CASE("keeps writing after the file is truncated in place") {
  const std::string dir = prepareDir("copytruncate");
  const std::size_t max_size = 16 * 1024;
  applog::Logger &logger = applog::Logger::instance();
  logger.setLevel(applog::kTrace);
  CHECK(logger.initFile(dir, "logserver", max_size, 5));

  const std::string chunk(99, 'D');
  std::string record = chunk + '\n';
  for (int i = 0; i < 40; ++i) {
    logger.writeRaw(record.data(), record.size());
  }
  CHECK_EQ(std::size_t(4000), static_cast<std::size_t>(fileSize(dir + currentFile())));

  // logrotate's copytruncate: empty the file without replacing the inode.
  std::FILE *file = std::fopen((dir + currentFile()).c_str(), "wb");
  CHECK(file != nullptr);
  if (file) {
    std::fclose(file);
  }
  CHECK_EQ(std::size_t(0), static_cast<std::size_t>(fileSize(dir + currentFile())));

  // 132 records bring the real size to 13200 bytes, still under max_size. If
  // the stale size were kept, the accounting would pass max_size and rotate.
  for (int i = 0; i < 132; ++i) {
    logger.writeRaw(record.data(), record.size());
  }

  std::string content;
  CHECK(readFile(dir + currentFile(), &content));
  CHECK_EQ(std::size_t(13200), content.size());
  CHECK(onlyWholeRecords(content, record.size()));
  CHECK_EQ(std::size_t(1), listFiles(dir).size());
}
#endif

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

TEST_CASE("a failed rename keeps the file instead of removing it") {
#ifndef __linux__
  // Fails the rename has to be forced with a permission bit, which is left
  // untested on a Windows runner: there the check would only pass.
  return;
#else
  if (::geteuid() == 0) {
    return;  // a root process ignores the permission bits
  }
  const std::string dir = prepareDir("rename_fails");
  applog::Logger &logger = applog::Logger::instance();
  CHECK(logger.initFile(dir, "logserver", 1024, 5));

  std::string chunk(99, 'R');
  chunk += '\n';
  for (int i = 0; i < 20; ++i) {
    logger.writeRaw(chunk.data(), chunk.size());
  }

  // Without write permission on the directory the archive cannot be created.
  CHECK_EQ(0, ::chmod(dir.c_str(), 0500));
  const long length_before = fileSize(dir + currentFile());
  for (int i = 0; i < 100; ++i) {
    logger.writeRaw(chunk.data(), chunk.size());
  }
  CHECK_EQ(0, ::chmod(dir.c_str(), 0700));

  // Nothing was lost: every line after the rotation that could not happen is
  // in the file, so it has grown well past max_file_size. (Under a root uid
  // the chmod above does not stop the rename at all, hence the skip.)
  std::string content;
  CHECK(readFile(dir + currentFile(), &content));
  CHECK(fileSize(dir + currentFile()) > length_before);
  CHECK(onlyWholeRecords(content, chunk.size()));

  // Once the directory is writable again the next rotation archives the file.
  for (int i = 0; i < 40; ++i) {
    logger.writeRaw(chunk.data(), chunk.size());
  }
  bool archived = false;
  const std::vector<std::string> files = listFiles(dir);
  for (std::size_t i = 0; i < files.size(); ++i) {
    if (isArchiveName(files[i], kDefaultSuffix)) {
      archived = true;
    }
  }
  CHECK(archived);
#endif
}

TEST_CASE("initFile refuses a directory that is not a temporary file system") {
#ifndef __linux__
  // The gate only exists where statfs() reports the file system type.
  return;
#else
  struct statfs root_info;
  CHECK_EQ(0, ::statfs("/", &root_info));
  if (root_info.f_type == TMPFS_MAGIC || root_info.f_type == RAMFS_MAGIC) {
    return;  // a container with / on tmpfs; nothing to refuse here
  }

  // The path has to exist to be refused: a missing one under / is turned down
  // by makePath for a reason of its own. It is created here only where that
  // is possible (a writable /, i.e. root) and removed again afterwards; when
  // it cannot be created, only the refusal is checked.
  const std::string dir = "/logserver_unsupported_fs_test/";
  const std::string marker = "logserver_marker.txt";
  removeDir(dir);
  makeDir(dir);
  const bool created = ::access(dir.c_str(), F_OK) == 0;
  if (created) {
    std::FILE *file = std::fopen((dir + marker).c_str(), "wb");
    CHECK(file != nullptr);
    if (file) {
      std::fclose(file);
    }
  }

  applog::Logger &logger = applog::Logger::instance();
  CHECK(!logger.initFile(dir, "logserver", 1024, 5));

  // Nothing may be written into the directory that was refused.
  CHECK(fileSize(dir + currentFile()) < 0);

  if (created) {
    CHECK(::access((dir + marker).c_str(), F_OK) == 0);
  }
  removeDir(dir);
#endif
}

TEST_CASE("initFile fails on a directory it cannot create") {
#ifndef __linux__
  return;  // a root process ignores the permission bits
#else
  if (::geteuid() == 0) {
    return;
  }
  const std::string parent = prepareDir("readonly_parent");
  CHECK_EQ(0, ::chmod(parent.c_str(), 0500));

  applog::Logger &logger = applog::Logger::instance();
  const bool created = logger.initFile(parent + "child" + kPathSeparator, "logserver", 1024, 5);
  CHECK_EQ(0, ::chmod(parent.c_str(), 0700));
  CHECK(!created);
  CHECK(::access((parent + "child").c_str(), F_OK) != 0);
#endif
}

int main() {
  const int result = testfw::run_all();
  removeDir(testRoot());
  return result;
}
