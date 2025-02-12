/*
 * Leid 2025.2
 */
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <linux/magic.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#endif

#ifdef _WIN32

bool isTempFS(std::string mount_path) {
  return true;
}

int mkpath(std::string &s) {
  size_t pre = 0, pos;
  std::string dir;
  int mdret = 0;

  if (s[s.size() - 1] != '\\') {
    // force trailing \ so we can handle everything in loop
    s += '\\';
  }

  while ((pos = s.find_first_of('\\', pre)) != std::string::npos) {
    dir = s.substr(0, pos++);
    pre = pos;
    if (dir.size() == 0)
      continue;  // if leading \ first time is 0 length

    int num = MultiByteToWideChar(CP_ACP, 0, dir.c_str(), -1, NULL, 0);
    wchar_t *wide = new wchar_t[num];
    MultiByteToWideChar(CP_ACP, 0, dir.c_str(), -1, wide, num);
    std::wstring wcpath(wide);
    delete[] wide;

    mdret = CreateDirectoryW(wcpath.c_str(), NULL);
    if ((mdret == 0) && (GetLastError() != ERROR_ALREADY_EXISTS))
      break;
  }

  if ((mdret == 0) && (GetLastError() != ERROR_ALREADY_EXISTS)) {
    ::perror("CreateDirectoryW");
    return -1;
  } else
    return 0;
}

#else

long getFileSystemType(std::string mount_path) {
  struct statfs fs_info;
  memset(&fs_info, 0, sizeof(fs_info));

  if (statfs(mount_path.c_str(), &fs_info)) {
    ::perror("statfs");
    return -1;
  }

  return fs_info.f_type;
}

bool isTempFS(std::string mount_path) {
  return (getFileSystemType(mount_path) == TMPFS_MAGIC);
}

int mkpath(std::string &s, mode_t mode) {
  size_t pre = 0, pos;
  std::string dir;
  int mdret;

  if (s[s.size() - 1] != '/') {
    // force trailing / so we can handle everything in loop
    s += '/';
  }

  while ((pos = s.find_first_of('/', pre)) != std::string::npos) {
    dir = s.substr(0, pos++);
    pre = pos;
    if (dir.size() == 0)
      continue;  // if leading / first time is 0 length
    if ((mdret = ::mkdir(dir.c_str(), mode)) && (errno != EEXIST))
      break;
  }

  if (mdret && (errno != EEXIST)) {
    ::perror("mkdir");
    return -1;
  } else
    return 0;
}

#endif
