#pragma once

#include "declarations.hpp"

#include <string>
#include <vector>

class PElementNodeDatum {
private:
  int _main_id;
  std::vector<int> _sub_ids;
  std::vector<double> _datum;
  std::vector<std::string> _str_datum;
  // std::vector<std::string> _tags;
public:
  PElementNodeDatum() = default;
  virtual ~PElementNodeDatum() {}

  PElementNodeDatum(int main_id) : _main_id(main_id) {};

  int getMainId() const { return _main_id; }
  std::vector<int> getSubIds() const { return _sub_ids; }
  std::vector<double> getData() const { return _datum; }
  std::vector<std::string> getStringData() const { return _str_datum; }
  // std::vector<std::string> getTags() const { return _tags; }

  void addSubId(int &id) { _sub_ids.push_back(id); }
  void addData(double &data) { _datum.push_back(data); }
  void addStringData(std::string &data) { _str_datum.push_back(data); }
  // void addTag(std::string &tag) { _tags.push_back(tag); }
};


class PElementNodeData {
private:
  int _type;  // 0: node, 1: element, 2: element nodal
  int _order;  // 0: scalar, 1: vector, 2: tensor
  std::string _label;
  std::vector<std::string> _component_labels;
  std::vector<PElementNodeDatum *> _data;
public:
  PElementNodeData() = default;
  PElementNodeData(int type, int order, std::string label) : _type(type), _order(order), _label(label) {}

  std::string getLabel() const { return _label; }
  std::vector<std::string> getComponentLabels() const { return _component_labels; }
  std::vector<PElementNodeDatum *> getData() { return _data; }

  void setComponentLabels(std::vector<std::string> labels) { _component_labels = labels; }

  void addComponentLabel(std::string &label) { _component_labels.push_back(label); }
  void addData(PElementNodeDatum *datum) { _data.push_back(datum); }

  bool isEmpty() { return (_data.size() == 0); }

  void writeGmshMsh(FILE *, FILE *, int &, Message *);
  // void writeGmshMsh(std::string &, std::string &, Message *);
};

