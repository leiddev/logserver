/*
 * lujy 2021.6
 */
#ifndef _ELEMENT_H_
#define _ELEMENT_H_

#include "bl/json.h"
#include "bl/mutex.h"
#include "bl/package.h"
#include "bl/pad.h"
#include "bl/thread.h"
#include "bl/time.h"
#include "exp.h"
#include <vector>

namespace bl {
class BL_API PipeLine;
class BL_API Element : public bl::Object {
 public:
  Element();
  virtual ~Element();

 public:
  bool IsProcessRunning();
  bool IsKeyPoint();
  virtual int Init();
  virtual int DeInit();
  virtual int Start();
  virtual int Stop();
  virtual int Process();
  virtual std::string GetStatus();
  virtual int LoadParameters(const JSON &param);
  virtual int SetSrcPad(unsigned int index, Pad *pad);
  virtual Pad *GetSrcPad(unsigned int index);
  virtual int SetSinkPad(unsigned int index, Pad *pad);
  virtual Pad *GetSinkPad(unsigned int index);
  virtual int SetPipeLine(PipeLine *);
  virtual void PushSrcPad(unsigned int index, sptrPackage);
  virtual sptrPackage PopSinkPad(unsigned int index, int to = -1);

 protected:
  class ProcessThread : public Thread {
   public:
    ProcessThread(Element *e) {
      _element = e;
    };
    int Run() {
      SetName(_element->Name());
      return _element->Process();
    };

   private:
    Element *_element;
  };

 protected:
  JSON _parameter;
  ProcessThread _process_thread;
  std::vector<Pad *> _src_pads;
  std::vector<Pad *> _sink_pads;
  PipeLine *_pipeline;
  struct timeval tv0 = {0};
};
}  // namespace bl
#endif
