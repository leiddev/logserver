/*
 * lujy 2021.6
 */

#include "bl/json.h"
#include "cJSON.h"
#include "common.h"
#include <cstring>
#include <fstream>
#include <sstream>

using bl::File;
using std::string;
using std::vector;

namespace bl {

JSON::JSON() {
  _root = nullptr;
}

JSON::JSON(const JSON &parent, const string &pos) {
  cJSON *item = (cJSON *)parent._Locate(pos);
  if (item) {
    char *content = cJSON_Print(item);
    if (content) {
      _root = cJSON_Parse(content);
      free(content);
    }
  }
}

// move constructor
JSON::JSON(JSON &&parent) {
  _root = parent._root;
  parent._root = nullptr;
}

JSON &JSON::operator=(const JSON &json) {
  if (_root) {
    cJSON_Delete((cJSON *)_root);
    _root = nullptr;
  }

  cJSON *item = (cJSON *)json._Locate("/");
  if (item) {
    char *content = cJSON_Print(item);
    if (content) {
      _root = cJSON_Parse(content);
      free(content);
    }
  }
  return *this;
}

JSON::JSON(void *root) {
  _root = root;
}

JSON::~JSON() {
  if (_root)
    cJSON_Delete((cJSON *)_root);
}

int JSON::Load(const File &file) {
  int err = 0;
  std::fstream fs(file.full_name);
  std::stringstream ss;
  ss << fs.rdbuf();
  string content = ss.str();
  _root = cJSON_Parse(content.c_str());
  if (!_root)
    return -1;
  return err;
}

int JSON::Load(const string &json) {
  _root = cJSON_Parse(json.c_str());
  if (!_root)
    return -1;
  return 0;
}

int JSON::Load(const char *json) {
  _root = cJSON_Parse(json);
  if (!_root)
    return -1;
  return 0;
}

int JSON::ReLoad(const File &file) {
  if (_root)
    cJSON_Delete((cJSON *)_root);
  return Load(file);
}

int JSON::ReLoad(const string &json) {
  if (_root)
    cJSON_Delete((cJSON *)_root);
  return Load(json);
}

int JSON::Save(const File &file) {
  int err = 0;

  if (!_root)
    return -1;

  char *content = cJSON_Print((cJSON *)_root);

  if (!content)
    return -1;

  err = file.Write(content, std::strlen(content));

  free(content);

  return err;
}

void JSON::Print() {
  if (!_root)
    return;

  char *content = cJSON_Print((cJSON *)_root);

  if (!content)
    return;

  LOG_INFO("{}", content);

  free(content);
}

void JSON::ToString(string &s) const {
  if (!_root)
    return;
  char *json = cJSON_Print((cJSON *)_root);
  s = json;  // copy;
  free(json);
}

string JSON::ToString(bool formatted) const {
  std::stringstream ss;
  char *json = NULL;
  if (!_root)
    return "err";
  if (formatted)
    json = cJSON_Print((cJSON *)_root);
  else
    json = cJSON_PrintUnformatted((cJSON *)_root);
  ss << json;
  free(json);
  return ss.str();
}

int JSON::GetArray(const string &pos, vector<JSON> &array) const {
  cJSON *item = nullptr;
  cJSON *element = nullptr;

  item = (cJSON *)_Locate(pos);
  if (!item || item->type != cJSON_Array)
    return -1;

  int array_size = cJSON_GetArraySize(item);

  for (int i = 0; i < array_size; i++) {
    element = cJSON_GetArrayItem(item, i);
    array.push_back(JSON(cJSON_Duplicate(element, 1)));
  }

  return 0;
}

int JSON::GetArrayString(const string &pos, vector<string> &array) const {
  cJSON *item = nullptr;
  cJSON *element = nullptr;

  item = (cJSON *)_Locate(pos);
  if (!item || item->type != cJSON_Array)
    return -1;

  size_t array_size = cJSON_GetArraySize(item);

  for (size_t i = 0; i < array_size; i++) {
    element = cJSON_GetArrayItem(item, i);
    array.push_back(element->valuestring);
  }

  return 0;
}

int JSON::CreateArray(const string &path, const string &name, vector<JSON> &array) {
  cJSON *item = (cJSON *)_Create(path);
  if (!item)
    return -1;

  cJSON *arrayItem = cJSON_CreateArray();
  cJSON_AddItemToObject(item, name.c_str(), arrayItem);
  for (int i = 0; i < array.size(); i++)
    cJSON_AddItemToArray(arrayItem, cJSON_Duplicate((cJSON *)(array[i]._root), 1));

  return 0;
}

int JSON::CreateArrayRoot(vector<JSON> &array) {
  if (!_root) {
    _root = cJSON_CreateArray();
  }

  if (((cJSON *)_root)->type != cJSON_Array) {
    return -1;
  }

  for (int i = 0; i < array.size(); i++)
    cJSON_AddItemToArray((cJSON *)_root, cJSON_Duplicate((cJSON *)(array[i]._root), 1));

  return 0;
}

int JSON::CreateIntArray(const string &path, const string &name, const int *numbers, int count) {
  cJSON *item = (cJSON *)_Create(path);
  if (!item)
    return -1;

  cJSON *intArray = cJSON_CreateIntArray(numbers, count);
  cJSON_AddItemToObject(item, name.c_str(), intArray);

  return 0;
}

int JSON::CreateFloatArray(const string &path, const string &name, const float *numbers, int count) {
  cJSON *item = (cJSON *)_Create(path);
  if (!item)
    return -1;

  cJSON *intArray = cJSON_CreateFloatArray(numbers, count);
  cJSON_AddItemToObject(item, name.c_str(), intArray);

  return 0;
}

int JSON::CreateDoubleArray(const string &path, const string &name, const double *numbers, int count) {
  cJSON *item = (cJSON *)_Create(path);
  if (!item)
    return -1;

  cJSON *intArray = cJSON_CreateDoubleArray(numbers, count);
  cJSON_AddItemToObject(item, name.c_str(), intArray);

  return 0;
}

int JSON::CreateStringArray(const string &path, const string &name, const char **strings, int count) {
  cJSON *item = (cJSON *)_Create(path);
  if (!item)
    return -1;

  cJSON *intArray = cJSON_CreateStringArray(strings, count);
  cJSON_AddItemToObject(item, name.c_str(), intArray);

  return 0;
}

int JSON::GetInt(const string &pos, int dft) const {
  cJSON *item = nullptr;
  int val = 0;

  item = (cJSON *)_Locate(pos);
  if (item && item->type == cJSON_Number) {
    val = item->valueint;
  } else {
    val = dft;
  }

  return val;
}

int JSON::SetInt(const string &pos, int value) {
  cJSON *item = nullptr;
  int val = 0;

  item = (cJSON *)_Locate(pos);

  if (!item || item->type != cJSON_Number)
    return -1;

  // item->valueint = value;
  cJSON_SetNumberValue(item, value);

  //_content = cJSON_Print(_root);

  return 0;
}

int JSON::CreateInt(const string &path, const string &name, int value) {
  vector<string> nodes;
  cJSON *item = (cJSON *)_Create(path);
  if (!item)
    return -1;

  cJSON_AddNumberToObject(item, name.c_str(), value);
  return 0;
}

double JSON::GetDouble(const string &pos, double dft) const {
  cJSON *item = nullptr;
  double val = 0;

  item = (cJSON *)_Locate(pos);
  if (item && item->type == cJSON_Number) {
    val = item->valuedouble;
  } else {
    val = dft;
  }

  return val;
}

double JSON::SetDouble(const string &pos, double value) {
  cJSON *item = nullptr;
  double val = 0;

  item = (cJSON *)_Locate(pos);

  if (!item || item->type != cJSON_Number)
    return -1;

  // item->valueint = value;
  cJSON_SetNumberValue(item, value);

  //_content = cJSON_Print(_root);

  return 0;
}

int JSON::CreateDouble(const string &path, const string &name, double value) {
  vector<string> nodes;
  cJSON *item = (cJSON *)_Create(path);
  if (!item)
    return -1;

  cJSON_AddNumberToObject(item, name.c_str(), value);
  return 0;
}

int JSON::GetString(const string &pos, string &value, const string &dft) const {
  cJSON *item = nullptr;
  item = (cJSON *)_Locate(pos);
  if (item && item->type == cJSON_String) {
    value = item->valuestring;
  } else {
    value = dft;
  }
  return 0;
}

string JSON::GetString(const string &pos, const string &dft) const {
  cJSON *item = nullptr;
  string value;
  item = (cJSON *)_Locate(pos);
  if (item && item->type == cJSON_String) {
    value = item->valuestring;
  } else {
    value = dft;
  }
  return value;
}

int JSON::SetString(const string &pos, const string &value) {
  cJSON *item = nullptr;

  int val = 0;

  item = (cJSON *)_Locate(pos);

  if (!item || item->type != cJSON_String)
    return -1;

  free(item->valuestring);
  item->valuestring = 0;

  // item->valuestring = (char *)malloc(value.size());

  // if (!item->valuestring) return -1;

  // memset(item->valuestring, 0, value.size());
  // memcpy(item->valuestring, value.c_str(), value.size());

  item->valuestring = strdup(value.c_str());

  //_content = cJSON_Print(_root);

  return 0;
}

int JSON::CreateString(const string &path, const string &name, const string &value) {
  vector<string> nodes;
  cJSON *item = (cJSON *)_Create(path);
  if (!item)
    return -1;

  cJSON_AddStringToObject(item, name.c_str(), value.c_str());
  return 0;
}

// int JSON::GetRootName(std::string &name) const {
//  int err = 0;
//  cJSON *parent = (cJSON *)_root;
//  CHECK_PTR(parent);
//
//  name = parent->string;
//
//  return err;
//}

int JSON::GetChildName(vector<string> &childs, int depth) const {
  int err = 0;
  cJSON *parent = (cJSON *)_root;
  CHECK_PTR(parent);

  cJSON *child = parent->child;
  CHECK_PTR(child);

  while (parent && depth) {
    child = parent->child;
    while (child) {
      // LOG_INFO("{} {}", parent->string, child->string);
      childs.push_back(child->string);
      child = child->next;
    }
    parent = parent->child;
    depth--;
  }

  return 0;
}

void *JSON::_Locate(const string &location) const {
  vector<string> node;
  vector<string>::iterator it;
  if (!_root) {
    return nullptr;
  }

  if (location == "/") {
    return _root;
  }

  cJSON *item = (cJSON *)_root;

  Split(location, node, "/");

  it = node.begin();

  for (; it != node.end(); it++) {
    item = cJSON_GetObjectItem(item, (*it).c_str());
    // LOG_DEBUG("{}", item->string);
    if (!item)
      break;
  }
  return item;
}

void *JSON::_Create(const string &location) {
  vector<string> node;
  vector<string>::iterator it;
  cJSON *parent = 0;
  cJSON *item = 0;

  if (!_root) {
    _root = cJSON_CreateObject();
  }

  if (location == "/") {
    return _root;
  }

  parent = item = (cJSON *)_root;

  Split(location, node, "/");

  it = node.begin();

  for (; it != node.end(); it++) {
    parent = item;
    item = cJSON_GetObjectItem(item, (*it).c_str());
    // LOG_DEBUG("{}", item->string);
    if (!item) {
      item = cJSON_CreateObject();
      cJSON_AddItemToObject(parent, (*it).c_str(), item);
    };
  }
  return item;
}

void JSON::Split(const string &s, vector<string> &tokens, const string &delimiters) {
  string::size_type lastPos = s.find_first_not_of(delimiters, 0);
  string::size_type pos = s.find_first_of(delimiters, lastPos);
  while (string::npos != pos || string::npos != lastPos) {
    tokens.push_back(s.substr(lastPos, pos - lastPos));
    lastPos = s.find_first_not_of(delimiters, pos);
    pos = s.find_first_of(delimiters, lastPos);
  }
}

bool JSON::GetBool(const string &pos, bool dft) const {
  cJSON *item = nullptr;
  bool val = dft;

  item = (cJSON *)_Locate(pos);

  if (item) {
    if (item->type == cJSON_False) {
      val = false;
    } else if (item->type == cJSON_True) {
      val = true;
    }
  }

  return val;
}

int JSON::SetBool(const string &pos, bool value) {
  cJSON *item = nullptr;
  bool val = false;

  item = (cJSON *)_Locate(pos);

  if (!item || ((item->type != cJSON_False) && (item->type != cJSON_True)))
    return -1;

  if (value) {
    item->type = cJSON_True;
  } else {
    item->type = cJSON_False;
  }

  return 0;
}

int JSON::CreateBool(const string &path, const string &name, bool value) {
  vector<string> nodes;
  cJSON *item = (cJSON *)_Create(path);
  if (!item)
    return -1;

  cJSON_AddBoolToObject(item, name.c_str(), value);
  return 0;
}

}  // namespace bl