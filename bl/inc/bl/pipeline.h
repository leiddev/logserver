/*
 * lujy 2021.6
 */
#ifndef _PIPELINE_H_
#define _PIPELINE_H_

#include "bl/element.h"
#include "bl/json.h"
#include "bl/message.h"
#include <vector>

#define CREATE_ELEMENT(name, map)                     \
  do {                                                \
    Element *e = new name();                          \
    e->SetPipeLine(this);                             \
    e->LoadParameters(JSON(param, "/" #name));        \
    e->Init();                                        \
    map.push_back(pair<string, Element *>(#name, e)); \
    LOG_INFO(#name " create success");              \
  } while (0)

namespace bl {

using ElementMap = std::vector<std::pair<std::string, Element *>>;

class PipeLine : public bl::Object {
 public:
  PipeLine();
  virtual ~PipeLine();

 public:
  virtual int Start(const bl::JSON &param);
  virtual int Stop();
  virtual int UpdataParameter(const bl::JSON &param);
  virtual void ShowStatus(const std::string *log_file);
  Element *GetElement(const std::string &name);

 protected:
  virtual int CreateElement(const JSON &param, const std::string &name);
  virtual int ConnectLink(const JSON &param);

 protected:
  class MessageBus : public Element {
    MessageBus(PipeLine *p) {
      _pipeline = p;
    }

   private:
    PipeLine *_pipeline;
  };

 protected:
  ElementMap _element_map;
};
}  // namespace bl
#endif  // !_PIPELINE_H_