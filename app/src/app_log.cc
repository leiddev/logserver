/*
 * Leid 2026.11
 */
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "app_log.hh"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <ctime>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <linux/magic.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#endif

namespace {

const char *const kFileSuffix = ".log";
const char *const kArchiveSeparator = "_";

#ifdef _WIN32
const char *const kPathSeparator = "\\";
#else
const char *const kPathSeparator = "/";
#endif

std::string joinPath(const std::string &dir, const std::string &name) {
  if (dir.empty()) {
    return name;
  }
  char last = dir[dir.size() - 1];
  if (last == '\\' || last == '/') {
    return dir + name;
  }
  return dir + kPathSeparator + name;
}

bool pathExists(const std::string &path) {
  std::FILE *file = std::fopen(path.c_str(), "rb");
  if (!file) {
    return false;
  }
  std::fclose(file);
  return true;
}

#ifdef _WIN32

// Create the directory and all of its parents, like "mkdir -p".
bool makePath(const std::string &path) {
  if (path.empty()) {
    return true;
  }

  std::string normalized = path;
  if (normalized[normalized.size() - 1] != '\\') {
    normalized += '\\';
  }

  std::size_t begin = 0;
  std::size_t pos;
  while ((pos = normalized.find_first_of("\\/", begin)) != std::string::npos) {
    std::string dir = normalized.substr(0, pos);
    begin = pos + 1;
    if (dir.empty() || (dir.size() == 2 && dir[1] == ':')) {
      continue;
    }

    int length = ::MultiByteToWideChar(CP_ACP, 0, dir.c_str(), -1, nullptr, 0);
    if (length <= 0) {
      return false;
    }
    std::vector<wchar_t> wide(static_cast<std::size_t>(length));
    ::MultiByteToWideChar(CP_ACP, 0, dir.c_str(), -1, wide.data(), length);

    DWORD attributes = ::GetFileAttributesW(wide.data());
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
      continue;
    }
    if (::CreateDirectoryW(wide.data(), nullptr) == 0) {
      return false;
    }
  }
  return true;
}

bool isTemporaryFs(const std::string & /*path*/) {
  return true;
}

std::vector<std::string> listDir(const std::string &dir) {
  std::vector<std::string> names;

  WIN32_FIND_DATAA data;
  HANDLE handle = ::FindFirstFileA(joinPath(dir, "*").c_str(), &data);
  if (handle == INVALID_HANDLE_VALUE) {
    return names;
  }
  do {
    if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
      names.push_back(data.cFileName);
    }
  } while (::FindNextFileA(handle, &data) != 0);
  ::FindClose(handle);
  return names;
}

#else

bool makePath(const std::string &path) {
  if (path.empty()) {
    return true;
  }

  std::string normalized = path;
  if (normalized[normalized.size() - 1] != '/') {
    normalized += '/';
  }

  std::size_t begin = 0;
  std::size_t pos;
  while ((pos = normalized.find_first_of('/', begin)) != std::string::npos) {
    std::string dir = normalized.substr(0, pos);
    begin = pos + 1;
    if (dir.empty()) {
      continue;
    }
    if (::mkdir(dir.c_str(), 0777) != 0 && errno != EEXIST) {
      return false;
    }
  }
  return true;
}

bool isTemporaryFs(const std::string &path) {
  struct statfs fs_info;
  std::memset(&fs_info, 0, sizeof(fs_info));
  if (::statfs(path.c_str(), &fs_info) != 0) {
    return false;
  }
  return fs_info.f_type == TMPFS_MAGIC;
}

std::vector<std::string> listDir(const std::string &dir) {
  std::vector<std::string> names;

  DIR *dir_ptr = ::opendir(dir.c_str());
  if (!dir_ptr) {
    return names;
  }
  struct dirent *entry;
  while ((entry = ::readdir(dir_ptr)) != nullptr) {
    std::string name = entry->d_name;
    if (name != "." && name != "..") {
      names.push_back(name);
    }
  }
  ::closedir(dir_ptr);
  return names;
}

#endif

bool isArchiveName(const std::string &name, const std::string &prefix) {
  std::size_t suffix_size = std::strlen(kFileSuffix);
  return name.size() > prefix.size() + suffix_size &&
         name.compare(0, prefix.size(), prefix) == 0 &&
         name.compare(name.size() - suffix_size, suffix_size, kFileSuffix) == 0;
}

// Local time with millisecond resolution, e.g. "20261109_143005_123".
// std::localtime keeps its result in a static buffer, so the caller must
// serialize the call; all callers here hold Logger::mutex_.
std::string timestampSuffix() {
  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
  std::time_t seconds = std::chrono::system_clock::to_time_t(now);
  long long milliseconds =
      static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()) %
      1000;

  std::tm local_time;
#ifdef _WIN32
  localtime_s(&local_time, &seconds);
#else
  std::tm *local_time_ptr = std::localtime(&seconds);
  if (local_time_ptr) {
    local_time = *local_time_ptr;
  } else {
    std::memset(&local_time, 0, sizeof(local_time));
  }
#endif

  char date_time[32] = {0};
  std::strftime(date_time, sizeof(date_time), "%Y%m%d_%H%M%S", &local_time);

  char suffix[48] = {0};
  std::snprintf(suffix, sizeof(suffix), "%s_%03lld", date_time, milliseconds);
  return suffix;
}

}  // namespace

namespace applog {

std::string vformat(const char *format, std::size_t arg_count, const std::string *args) {
  std::string result;
  if (!format) {
    return result;
  }

  std::size_t index = 0;
  for (const char *p = format; *p; ++p) {
    if (p[0] == '{' && p[1] == '}') {
      if (index < arg_count) {
        result += args[index++];
      }
      ++p;
    } else {
      result += *p;
    }
  }
  return result;
}

Logger &Logger::instance() {
  static Logger logger;
  return logger;
}

Logger::~Logger() {
  closeFileLocked();
}

void Logger::setLevel(Level level) {
  std::lock_guard<std::mutex> lock(mutex_);
  level_ = level;
}

bool Logger::initFile(const std::string &dir, const std::string &base_name, std::size_t max_file_size,
                      std::size_t max_files) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (max_file_size == 0) {
    return false;
  }
  if (!makePath(dir)) {
    std::fprintf(stderr, "[APP] [%s:%d] failed to create log directory: %s\n", __FILE__, __LINE__, dir.c_str());
    return false;
  }
  if (!isTemporaryFs(dir)) {
    return false;
  }
  if (max_files == 0) {
    return false;
  }

  closeFileLocked();
  dir_ = dir;
  base_name_ = base_name;
  max_file_size_ = max_file_size;
  max_files_ = max_files;
  base_path_ = joinPath(dir_, base_name_ + kFileSuffix);

  if (!openFileLocked()) {
    return false;
  }
  cleanupLocked();
  return true;
}

std::string Logger::filePath() {
  std::lock_guard<std::mutex> lock(mutex_);
  return base_path_;
}

void Logger::write(Level level, const std::string &message) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (level < level_ || message.empty()) {
    return;
  }
  std::fwrite(message.data(), 1, message.size(), stdout);
  std::fflush(stdout);

  writeFileLocked(message.data(), message.size());
}

void Logger::writeRaw(const char *data, std::size_t length) {
  if (!data || length == 0) {
    return;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  writeFileLocked(data, length);
}

bool Logger::openFileLocked() {
  file_ = std::fopen(base_path_.c_str(), "ab");
  if (!file_) {
    if (!open_error_reported_) {
      std::fprintf(stderr, "[APP] [%s:%d] failed to open log file %s: %s\n", __FILE__, __LINE__, base_path_.c_str(),
                   std::strerror(errno));
      open_error_reported_ = true;
    }
    return false;
  }
  open_error_reported_ = false;

  current_size_ = 0;
  if (std::fseek(file_, 0, SEEK_END) == 0) {
    long size = std::ftell(file_);
    if (size > 0) {
      current_size_ = static_cast<std::size_t>(size);
    }
  }
  return true;
}

void Logger::closeFileLocked() {
  if (file_) {
    std::fclose(file_);
    file_ = nullptr;
  }
  current_size_ = 0;
}

void Logger::writeFileLocked(const char *data, std::size_t length) {
  if (max_files_ == 0) {
    return;
  }
  if (!file_ && !openFileLocked()) {
    return;
  }
  if (current_size_ > 0 && current_size_ + length > max_file_size_) {
    rotateLocked();
    if (!file_) {
      return;
    }
  }

  if (std::fwrite(data, 1, length, file_) != length || std::fflush(file_) != 0) {
    std::fprintf(stderr, "[APP] [%s:%d] failed to write log file %s: %s\n", __FILE__, __LINE__, base_path_.c_str(),
                 std::strerror(errno));
    closeFileLocked();
    return;
  }
  current_size_ += length;
}

std::string Logger::archivePathLocked() {
  std::string base = joinPath(dir_, base_name_ + kArchiveSeparator + timestampSuffix());
  std::string path = base + kFileSuffix;

  int index = 1;
  while (pathExists(path)) {
    char suffix[16] = {0};
    std::snprintf(suffix, sizeof(suffix), "%s%03d", kArchiveSeparator, index++);
    path = base + suffix + kFileSuffix;
  }
  return path;
}

void Logger::rotateLocked() {
  if (!file_) {
    return;
  }
  closeFileLocked();

  if (pathExists(base_path_)) {
    std::string archive = archivePathLocked();
    if (std::rename(base_path_.c_str(), archive.c_str()) != 0) {
      std::fprintf(stderr, "[APP] [%s:%d] failed to rename %s to %s: %s\n", __FILE__, __LINE__, base_path_.c_str(),
                   archive.c_str(), std::strerror(errno));
      std::remove(base_path_.c_str());
    }
  }

  openFileLocked();
  cleanupLocked();
}

void Logger::cleanupLocked() {
  const std::string prefix = base_name_ + kArchiveSeparator;

  std::vector<std::string> archives;
  std::vector<std::string> names = listDir(dir_);
  for (std::size_t i = 0; i < names.size(); ++i) {
    if (isArchiveName(names[i], prefix)) {
      archives.push_back(joinPath(dir_, names[i]));
    }
  }
  std::sort(archives.begin(), archives.end());

  std::size_t total = archives.size() + (pathExists(base_path_) ? 1 : 0);
  for (std::size_t i = 0; total > max_files_ && i < archives.size(); ++i) {
    if (std::remove(archives[i].c_str()) != 0) {
      std::fprintf(stderr, "[APP] [%s:%d] failed to remove %s: %s\n", __FILE__, __LINE__, archives[i].c_str(),
                   std::strerror(errno));
      return;
    }
    --total;
  }
}

}  // namespace applog
