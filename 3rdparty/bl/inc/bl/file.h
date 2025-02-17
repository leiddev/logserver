/*
 *lujy 2020.1
 */
#ifndef _BL_FILE_H_
#define _BL_FILE_H_

#include <string>
#include <vector>
#include "exp.h"

//#ifdef __cplusplus
// extern "C" {
//#endif

namespace bl {

class BL_API File {
 public:
  File(const std::string &name, const std::string &path = ".",
       const std::string &suffix = "");
  ~File();

 public:
  int Read(const void *buffer, long long &len) const;
  int Write(const void *buffer, long long len) const;

  static void GetFiles(const std::string &path,
                       const std::vector<std::string> &ends,
                       std::vector<File> &files, bool is_recursive = false);
  static bool EndsWith(const std::string &name,
                       const std::vector<std::string> &ends);

 private:
  // File(const File &f);// prevent copy constructor

 public:
  std::string full_name;
  std::string path;
  std::string name;
  std::string suffix;
};

}  // namespace bl

//#ifdef __cplusplus
//}
//#endif

#endif