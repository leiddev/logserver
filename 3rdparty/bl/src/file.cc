/*
 * lujy 2021.6
 */
#include "bl/file.h"
#include <fstream>
//#include <io.h>
#include <string.h>
//#include <iostream>
//#include <direct.h>
#include "common.h"

using std::string;
using std::vector;

namespace bl {

File::File(const string &name, const string &path, const string &suffix) {
  this->path = path;
  this->name = name;
  this->suffix = suffix;
  this->full_name = ((this->path == "") ? this->path : this->path + "/") + this->name + this->suffix;
}

File::~File() {
}

int File::Read(const void *buffer, long long &len) const {
  std::ifstream _in_stream;

  _in_stream.open(full_name.c_str(), std::ifstream::binary);
  if (!_in_stream) {
    LOG_ERR("open {} failed", full_name.c_str());
    return -1;
  }

  _in_stream.seekg(0, std::ios::end);
  len = _in_stream.tellg();
  _in_stream.seekg(0, std::ios::beg);

  _in_stream.read((char *)buffer, len);
  LOG_DEBUG("read {} {} bytes", full_name.c_str(), len);

  _in_stream.close();
  return (len == 0) ? -1 : 0;
}

int File::Write(const void *buffer, long long len) const {
  int err = 0;
  std::ofstream _out_steam;

  _out_steam.open(full_name.c_str(), std::ifstream::binary | std::ios::trunc);
  if (!_out_steam) {
    LOG_ERR("open {} failed", full_name.c_str());
    return -1;
  }

  _out_steam.write((const char *)buffer, len);

  _out_steam.close();
  return err;
}

bool File::EndsWith(const string &name, const vector<string> &ends) {
  for (vector<string>::const_iterator it = ends.begin(); it != ends.end(); it++) {
    if (name.find(*it) != -1)
      return true;
  }
  return false;
}

void File::GetFiles(const string &src_path, const vector<string> &ends, vector<File> &files, bool is_recursive) {
#if 0
  long hFile = 0;
  struct _finddata_t fileinfo;
  string p;
  string abs_path, rel_path, img_name, inf_name;
  LOG_INFO("finding...{}", src_path.c_str());

  if ((hFile = _findfirst(p.assign(src_path).append("/*").c_str(),
                          &fileinfo)) != -1) {
    do {
      if ((fileinfo.attrib & _A_SUBDIR)) {
        if (strcmp(fileinfo.name, ".") != 0 && strcmp(fileinfo.name, "..") != 0)
          if (is_recursive)
            GetFiles(p.assign(src_path).append("/").append(fileinfo.name), ends,
                     files, is_recursive);
      } else {
        if (EndsWith(fileinfo.name, ends)) {
          string tmp = p.assign(fileinfo.name);

          string name = tmp.substr(0, tmp.find("."));

          string suffix =
              tmp.substr(tmp.find("."), tmp.length() - tmp.find("."));
          File f(name, src_path, suffix);
          files.push_back(f);
        }
      }
    } while (_findnext(hFile, &fileinfo) == 0);
    _findclose(hFile);
  }
#endif
}
}  // namespace bl