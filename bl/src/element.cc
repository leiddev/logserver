/*
 * lujy 2021.6
 */
#include "bl/element.h"
#include "bl/message.h"
#include "common.h"
#include <cstring>
#include <string>

using std::vector;

namespace bl {
Element::Element() : _process_thread(this) {
}

Element::~Element() {
}

int Element::Init() {
  vector<JSON> sink_pads;
  vector<JSON> src_pads;

  _parameter.GetArray("/src_pad_config", src_pads);

  unsigned src_nums = src_pads.size();
  size_t queue_size = _parameter.GetInt("/queue_size", 1);
  for (unsigned i = 0; i < src_nums; i++) {
    Pad *pad = new Pad(pad_type::PAD_TYPE_SRC, queue_size);
    _src_pads.push_back(pad);
  }

  _parameter.GetArray("/sink_pad_config", sink_pads);

  unsigned sink_nums = sink_pads.size();

  for (unsigned i = 0; i < sink_nums; i++) {
    Pad *pad = new Pad(pad_type::PAD_TYPE_SINK, queue_size);
    _sink_pads.push_back(pad);
  }
  LOG_INFO("{} src_pads:{}, sinkc_pads:{}", Name().c_str(), src_nums,
           sink_nums);
  return 0;
}

int Element::DeInit() {
  for (unsigned i = 0; i < _src_pads.size(); i++) {
    delete _src_pads[i];
  }
  _src_pads.clear();
  for (unsigned i = 0; i < _sink_pads.size(); i++) {
    delete _sink_pads[i];
  }
  _sink_pads.clear();
  return 0;
}

int Element::LoadParameters(const JSON &param) {
  int err = 0;
  _parameter = param;
  //LOG_DEBUG("{} parameter:", ToString());
  _parameter.Print();
  return err;
}

int Element::Start() {
  int err = 0;

  err = _process_thread.Start();
  CHECK_ERR(err);

  return err;
}

int Element::Stop() {
  int err = 0;

  err |= _process_thread.Stop();
  CHECK_ERR(err);

  return err;
}

int Element::SetSrcPad(unsigned int index, Pad *pad) {
  if (index >= _src_pads.size())
    return -1;
  _src_pads[index] = pad;
  return 0;
}

Pad *Element::GetSrcPad(unsigned int index) {
  if (index >= _src_pads.size())
    return nullptr;
  return _src_pads[index];
}

int Element::SetSinkPad(unsigned int index, Pad *pad) {
  if (index >= _sink_pads.size())
    return -1;
  _sink_pads[index] = pad;
  return 0;
}

Pad *Element::GetSinkPad(unsigned int index) {
  if (index >= _sink_pads.size())
    return nullptr;
  return _sink_pads[index];
}

int Element::Process() {
  int err = 0;
  using bl::sleep;
  for (;;) {
    LOG_INFO("{}:Base class, overwrite it", __FUNCTION__);
    sleep(1000);
  }
  return err;
}

std::string Element::GetStatus() {
  timeval tv = {0};
  std::string ss;
  double *fps = new double[_src_pads.size()];
  bl::gettimeofday(&tv, NULL);
  double dt = (tv.tv_sec * 1000 + tv.tv_usec / 1000) -
              (tv0.tv_sec * 1000 + tv0.tv_usec / 1000);
  ss = ss + "fps: ";
  for (size_t i = 0; i < _src_pads.size(); i++) {
    fps[i] = _src_pads[i]->GetSrcFrameNum() / dt * 1000;
    ss = ss + std::to_string(fps[i]) + ", ";
    _src_pads[i]->ResetSrcFrameNum();
  }
  tv0 = tv;
  delete[] fps;
  return ss;
}

bool Element::IsProcessRunning() {
  return _process_thread.GetThreadStatus();
}


int Element::SetPipeLine(PipeLine *p) {
  _pipeline = p;
  return !_pipeline;
}

bool Element::IsKeyPoint() {
  return _parameter.GetInt("/is_key", 0);
}
}  // namespace bl