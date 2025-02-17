#include "bl/pipeline.h"
#include "common.h"
#include <sstream>
#include <fstream>

using std::pair;
using std::string;
using std::vector;

namespace bl {

PipeLine::PipeLine() {
}

PipeLine::~PipeLine() {
}

int PipeLine::CreateElement(const JSON &param, const string &name) {
  int err = 0;
  if ("Element" == name) {
    CREATE_ELEMENT(Element, _element_map);
  } else {
    err = -1;
  }
  LOG_INFO("you should overrite CreateElement with your implement");
  return err;
}

Element *PipeLine::GetElement(const string &name) {
  Element *link = nullptr;
  ElementMap::iterator it = _element_map.begin();
  for (; it != _element_map.end(); it++) {
    if (it->first == name) {
      link = it->second;
      break;
    }
  }
  return link;
}
int PipeLine::ConnectLink(const JSON &param) {
  LOG_INFO("you should overrite CreateElement with your implement");
  return 0;
}

int PipeLine::Start(const JSON &param) {
  int err = 0;
  ElementMap::iterator it;
  vector<string> childs;
  param.GetChildName(childs);

  // create elements;
  for (vector<string>::iterator it = childs.begin(); it != childs.end(); it++) {
    err = CreateElement(param, it->c_str());
    CHECK_ERR(err);
  }

  // connect elements
  it = _element_map.begin();
  for (; it != _element_map.end(); it++) {
    vector<JSON> sink_pads;
    vector<JSON> src_pads;
    Element *e = it->second;

    JSON element_param(param, it->first);
    element_param.GetArray("/sink_pad_config", sink_pads);
    element_param.GetArray("/src_pad_config", src_pads);

    for (size_t i = 0; i < sink_pads.size(); i++) {
      string s = sink_pads[i].GetString("/name", "");
      size_t index = sink_pads[i].GetInt("/index", 0);
      Element *pe = GetElement(s);
      if (!pe) {
        LOG_ERR("no {} element in sink pads", s.c_str());
        return -1;
      }
      LOG_INFO("{}-sink:{} -> {}-src:{}", it->first.c_str(), i, s.c_str(),
               index);
      e->GetSinkPad(i)->SetPrevPad(pe->GetSrcPad(index));
    }

    for (size_t i = 0; i < src_pads.size(); i++) {
      string s = src_pads[i].GetString("/name", "");
      size_t index = src_pads[i].GetInt("/index", 0);
      Element *ne = GetElement(s);
      if (!ne) {
        LOG_ERR("no {} element in src pads", s.c_str());
        return -1;
      }
      LOG_INFO("{}-src:{} -> {}-sink:{}", it->first.c_str(), i, s.c_str(),
               index);
      e->GetSrcPad(i)->SetNextPad(ne->GetSinkPad(index));
    }
  }

  // start elements
  it = _element_map.begin();
  for (; it != _element_map.end(); it++) {
    err = it->second->Start();
    if (it->second->IsKeyPoint() && err)
      return err;
  }

  LOG_INFO("pipeline start success");
  return err;
}

int PipeLine::Stop() {
  int err = 0;
  ElementMap::iterator it = _element_map.begin();
  for (; it != _element_map.end(); it++) {
    it->second->Stop();
  }
  it = _element_map.begin();
  for (; it != _element_map.end(); it++) {
    it->second->DeInit();
  }
  it = _element_map.begin();
  for (; it != _element_map.end(); it++) {
    delete it->second;
  }
  _element_map.clear();

  LOG_INFO("pipeline stop success");

  return err;
}

int PipeLine::UpdataParameter(const bl::JSON &param) {
  ElementMap::iterator it = _element_map.begin();
  for (; it != _element_map.end(); it++) {
    it->second->LoadParameters(JSON(param, it->first));
  }
  return 0;
}

void PipeLine::ShowStatus(const string *log_file) {
  std::stringstream ss;
  char time[100] = {0};
  
  bl::timenow(time, bl::TIME_FORMAT_DEFAULT);
  ss << time << "\n";
  
  ElementMap::iterator it = _element_map.begin();
  ss << "----------------pipeline status:----------------\n";
  for (; it != _element_map.end(); it++) {
    ss << it->first << ": " << it->second->GetStatus() << "\n";
  }
  ss << "------------------------------------------------";
  LOG_INFO("{}", ss.str().c_str());

  if (log_file != nullptr) {
    std::fstream fs;
    fs.open(*log_file, std::ios::out | std::ios::app);
    if (fs.is_open()) {
      fs << ss.str();
      fs.close();
    }
  }
}
}  // namespace bl