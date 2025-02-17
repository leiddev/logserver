/*
 * lujy 2021.6
 */
#ifndef _MESSAGE_H_
#define _MESSAGE_H_

#include "bl/element.h"
#include "bl/json.h"
#include "bl/package.h"
#include "bl/semaphore.h"
#include "exp.h"
#include <functional>
#include <string>
#include <vector>

namespace bl {
struct BL_API Message : public bl::Package {
  Message(std::string name, std::string set, std::string get) {
    this->name = name;
    this->set = set;
    this->get = get;
  };
  std::string name;
  std::string set;
  std::string get;
};
using sharedMessage = std::shared_ptr<bl::Message>;

class BL_API MessageBus {
 public:
  static MessageBus &GetInstance();

 public:
  void RegistMessage(std::string, std::function<int(sharedMessage)>);
  int PostMessage(sharedMessage msg);

 private:
  bl::Mutex _mutex;
  std::vector<std::tuple<std::string, std::function<int(sharedMessage)>>>
      _msg_map;
};

}  // namespace bl
#endif
