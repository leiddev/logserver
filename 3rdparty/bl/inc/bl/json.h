/*
 * lujy 2020.6
 */
#ifndef _BL_JSON_H_
#define _BL_JSON_H_

#include "exp.h"
#include <string>
#include <vector>
#include <bl/file.h>

//#ifdef __cplusplus
// extern "C" {
//#endif

constexpr auto BL_JSON_MAX_LEM = (30 * 1024 * 1024);

namespace bl {

class BL_API JSON {
 public:
  JSON();
  JSON(void *root);
  JSON(const JSON &parent,
       const std::string &pos = "/");  // copy constructor
  JSON(JSON &&parent);                 // move constructor
  virtual ~JSON();

  int Load(const bl::File &file);
  int Load(const std::string &json);
  int Load(const char *json);

  int ReLoad(const bl::File &file);
  int ReLoad(const std::string &json);

  int Save(const bl::File &file);
  void Print();
  void ToString(std::string &s) const;
  std::string ToString(bool formatted = true) const;
  int CreateInt(const std::string &path, const std::string &name, int value);
  int SetInt(const std::string &pos, int value);
  int GetInt(const std::string &pos, int dft) const;

  int CreateDouble(const std::string &path, const std::string &name, double value);
  double SetDouble(const std::string &pos, double value);
  double GetDouble(const std::string &pos, double dft) const;

  int CreateString(const std::string &path, const std::string &name, const std::string &value);

  int SetString(const std::string &pos, const std::string &value);

  int GetString(const std::string &pos, std::string &value, const std::string &dft) const;
  std::string GetString(const std::string &pos, const std::string &dft) const;
  // int GetRootName(std::string &name) const;

  int GetChildName(std::vector<std::string> &childs, int depth = 1) const;

  JSON &operator=(const JSON &);

  int GetArray(const std::string &pos, std::vector<JSON> &array) const;
  int GetArrayString(const std::string &pos, std::vector<std::string> &array) const;
  int CreateArray(const std::string &path, const std::string &name, std::vector<JSON> &array);
  int CreateArrayRoot(std::vector<JSON> &array);
  int CreateIntArray(const std::string &path, const std::string &name, const int *numbers, int count);
  int CreateFloatArray(const std::string &path, const std::string &name, const float *numbers, int count);
  int CreateDoubleArray(const std::string &path, const std::string &name, const double *numbers, int count);
  int CreateStringArray(const std::string &path, const std::string &name, const char **strings, int count);

  static void Split(const std::string &s, std::vector<std::string> &tokens, const std::string &delimiters = " ");

  int CreateBool(const std::string &path, const std::string &name, bool value);
  int SetBool(const std::string &pos, bool value);
  bool GetBool(const std::string &pos, bool dft) const;

 private:
  void *_Locate(const std::string &location) const;
  void *_Create(const std::string &location);

 private:
  void *_root;
};

}  // namespace bl

//#ifdef __cplusplus
//}
//#endif

#endif