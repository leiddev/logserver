/*
 * lujy 2021.6
 */
#include "bl/message.h"
#include "common.h"

using std::string;

namespace bl {
MessageBus& MessageBus::GetInstance() {
  static MessageBus bus;
  return bus;
}
void MessageBus::RegistMessage(string name,
                               std::function<int(sharedMessage)> func) {
  _mutex.Lock();
  _msg_map.push_back(std::make_tuple(name, func));
  _mutex.Unlock();
}

int MessageBus::PostMessage(sharedMessage msg) {
  int err = 0;
  _mutex.Lock();
  for (const auto& info : _msg_map) {
    if (std::get<0>(info) == msg->name) {
      err = std::get<1>(info)(msg);
      _mutex.Unlock();
      return err;
    }
  }
  _mutex.Unlock();
  return -1;
}

// int MessageBus::Process() {
//   int err = 0;
//   while (IsProcessRunning()) {
//     bl::sleep(1);
//   }
//   return err;
// }
}  // namespace bl