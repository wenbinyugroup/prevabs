#include "PModelIO.hpp"

#include "PDCELVertex.hpp"
#include "PBaseLine.hpp"
#include "Material.hpp"
#include "PComponent.hpp"
#include "PModel.hpp"
#include "geo.hpp"
#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "overloadOperator.hpp"
#include "utilities.hpp"
#include "plog.hpp"

#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_print.hpp"

#include <string>
#include <vector>
#include <stack>
#include <sstream>
#include <iostream>


int readXMLElementComponentLaminate(
  PComponent *p_component, const xml_node<> *xn_component,
  std::vector<std::vector<std::string>> &dependents_all, std::vector<std::string> &depend_names,
  std::vector<Layup *> &p_layups, int &num_combined_layups,
  CrossSection *cs, PModel *pmodel, Message *pmessage
  ) {

  if (xn_component->first_node("location")) {
    p_component->setRefVertex(pmodel->getPointByName(
      xn_component->first_node("location")->value()));
  }

  // std::cout << "- reading segments" << std::endl;
  //
  //

  // Count the total number of segments
  int nseg = 0;
  for (auto nodeSegment = xn_component->first_node("segment"); nodeSegment;
        nodeSegment = nodeSegment->next_sibling("segment")) {
    nseg++;
  }









  // Read segments defined explicitly by a single base line and
  // a single layup in <segment>
  //
  for (auto nodeSegment = xn_component->first_node("segment"); nodeSegment;
        nodeSegment = nodeSegment->next_sibling("segment")) {
    Segment *p_segment;

    std::string segmentName;
    xml_attribute<> *p_xa_name{nodeSegment->first_attribute("name")};
    if (p_xa_name) {
      segmentName = nodeSegment->first_attribute("name")->value();
    }
    else {
      Segment::count_tmp++;
      segmentName = "sgm_" + std::to_string(Segment::count_tmp);
    }

    // std::cout << "[debug] reading segment: " << segmentName << std::endl;

    int i_freeend = -1;
    xml_attribute<> *p_xa_free{nodeSegment->first_attribute("free")};
    if (p_xa_free) {
      // std::cout << "[debug] p_xa_free->value() = " << p_xa_free->value()
      // << std::endl;
      std::string s_freeend = p_xa_free->value();
      if (s_freeend == "head" || s_freeend == "begin") {
        i_freeend = 0;
      } else if (s_freeend == "tail" || s_freeend == "end") {
        i_freeend = 1;
      }
    }
    // std::cout << "[debug] i_freeend = " << i_freeend << std::endl;

    std::string baselineName{nodeSegment->first_node("baseline")->value()};
    xml_node<> *nodeLayup{nodeSegment->first_node("layup")};
    std::string layupName{nodeLayup->value()};
    xml_attribute<> *attrDirection{nodeLayup->first_attribute("direction")};
    std::string layupSide{"left"};
    if (attrDirection) {
      layupSide = lowerString(attrDirection->value());
    }

    Baseline *p_baseline = pmodel->getBaselineByNameCopy(baselineName);
    if (p_baseline == nullptr) {
      std::cout << "[error] cannot find base line: " << baselineName
                << std::endl;
    }
    Layup *p_layup;
    // for (auto it = pmodel->baselines.begin(); it !=
    // pmodel->baselines.end();
    //     ++it) {
    //   if (it->getName() == baselineName)
    //     p_baseline = &(*it);
    // }
    LayerType *p_layertype_temp;
    Material *p_material_temp;
    // for (auto it = pmodel->layups.begin(); it != pmodel->layups.end();
    //      ++it) {
    //   if (it->getName() == layupName) {
    //     p_layup = &(*it);
    p_layup = pmodel->getLayupByName(layupName);

    if (p_layup != nullptr) {
      for (auto layer : p_layup->getLayers()) {
        // Layer type has been added to the used list if its id > 0
        // If layer type has been used,
        // then the corresponding material must also been used
        p_layertype_temp = layer.getLayerType();
        if (p_layertype_temp->id() == 0) {
          CrossSection::used_layertype_index++;
          p_layertype_temp->setId(CrossSection::used_layertype_index);
          cs->addUsedLayerType(p_layertype_temp);

          // Check the material
          p_material_temp = p_layertype_temp->material();
          if (p_material_temp->id() == 0) {
            CrossSection::used_material_index++;
            p_material_temp->setId(CrossSection::used_material_index);
            cs->addUsedMaterial(p_material_temp);
          }
        }
      }
    }
    else {
      // Raise exception
      std::cout << "[error] cannot find layup: " << layupName << std::endl;
    }
    p_layups.push_back(p_layup);




    // If there is only one segment and trim both ends, then split
    if (depend_names.size() > 0 && nseg == 1 && i_freeend == -1) {

      std::string split_by;
      PDCELVertex *v_split;

      // Default
      split_by = "id";
      std::size_t nvertex = p_baseline->vertices().size();
      if (nvertex % 2 == 0) {
        v_split = p_baseline->vertices()[nvertex / 2];
      }
      else {
        v_split = p_baseline->vertices()[(nvertex - 1) / 2];
      }

      // Specified
      xml_node<> *p_xn_split{nodeSegment->first_node("split")};
      if (p_xn_split) {
        // std::string split_by{"name"};
        xml_attribute<> *p_xa_by{p_xn_split->first_attribute("by")};
        if (p_xa_by) {
          split_by = p_xa_by->value();
        }

        // PDCELVertex *v_split;
        if (split_by == "name") {
          v_split = pmodel->getPointByName(p_xn_split->value());
        }
        else if (split_by == "id") {
          v_split = p_baseline->vertices()[atoi(p_xn_split->value()) - 1];
        }
      }

      // Split segment into two
      Segment *p_sgm_1, *p_sgm_2;
      std::string name_1, name_2;
      name_1 = segmentName + "_1";
      name_2 = segmentName + "_2";
      Baseline *p_bsl_1 = new Baseline(p_baseline->getName()+"_1", p_baseline->getType());
      Baseline *p_bsl_2 = new Baseline(p_baseline->getName()+"_2", p_baseline->getType());
      int bsl_i = 1;
      for (auto v : p_baseline->vertices()) {
        if (bsl_i == 1) {
          p_bsl_1->vertices().push_back(v);
        }
        if (v == v_split) {
          bsl_i = 2;
        }
        if (bsl_i == 2) {
          p_bsl_2->vertices().push_back(v);
        }
      }

      // p_bsl_1->print(pmessage, 9);
      // std::cout << std::endl;
      // p_bsl_2->print(pmessage, 9);

      pmodel->addBaseline(p_bsl_1);
      pmodel->addBaseline(p_bsl_2);

      p_sgm_1 = new Segment(pmodel, name_1, p_bsl_1, p_layup, layupSide, 0);
      if (i_freeend == 0 || i_freeend == -1) {
        p_sgm_1->setFreeEnd(i_freeend);
      }
      p_sgm_1->setClosed(false);
      p_sgm_1->setMatOrient1(p_component->getMatOrient1());
      p_sgm_1->setMatOrient2(p_component->getMatOrient2());
      p_component->addSegment(p_sgm_1);

      p_sgm_2 = new Segment(pmodel, name_2, p_bsl_2, p_layup, layupSide, 0);
      if (i_freeend == 1 || i_freeend == -1) {
        p_sgm_2->setFreeEnd(i_freeend);
      }
      p_sgm_2->setClosed(false);
      p_sgm_2->setMatOrient1(p_component->getMatOrient1());
      p_sgm_2->setMatOrient2(p_component->getMatOrient2());
      p_component->addSegment(p_sgm_2);

    }


    else {
      p_segment =
          new Segment(pmodel, segmentName, p_baseline, p_layup, layupSide, 0);
      p_segment->setFreeEnd(i_freeend);
      p_segment->setClosed(false);
      p_segment->setMatOrient1(p_component->getMatOrient1());
      p_segment->setMatOrient2(p_component->getMatOrient2());

      // Read trim config
      for (auto p_xn_trim = nodeSegment->first_node("trim"); p_xn_trim;
            p_xn_trim = p_xn_trim->next_sibling("trim")) {

        std::string loc;
        double x2, x3;

        xml_node<> *p_xn_location{p_xn_trim->first_node("location")};
        if (p_xn_location) {
          loc = p_xn_location->value();
        }

        xml_node<> *p_xn_direction{p_xn_trim->first_node("direction")};
        if (p_xn_direction) {
          std::stringstream ss{p_xn_direction->value()};
          ss >> x2 >> x3;
          if (loc == "head") {
            p_segment->setPrevBound(0, x2, x3);
          }
          else if (loc == "tail") {
            p_segment->setNextBound(0, x2, x3);
          }
        }
      }

      p_component->addSegment(p_segment);
    }

  }




  //
  //
  // Read segments defined by a base line and a list of layups.
  // Each layup has attributes of beginning and ending points on
  // the base line.
  //
  for (auto p_xn_segments = xn_component->first_node("segments"); p_xn_segments;
    p_xn_segments = p_xn_segments->next_sibling("segments")) {
    // std::cout << "\nreading segments" << std::endl;

    std::string s_bsl_name{p_xn_segments->first_node("baseline")->value()};
    Baseline *p_bsl = pmodel->getBaselineByName(s_bsl_name);
    if (p_bsl == nullptr) {
      std::cout << "[error] cannot find base line: " << s_bsl_name
                << std::endl;
    }

    std::string s_layup_side{"left"};
    xml_node<> *p_xn_side{p_xn_segments->first_node("layup_side")};
    if (p_xn_side) {
      s_layup_side = lowerString(p_xn_side->value());
    }

    // Read layups
    std::vector<double> v_u_sorted;

    std::vector<double> v_u_begin, v_u_end;
    std::vector<std::string> v_s_layup;
    std::vector<Layup *> v_p_layup;
    double tol = 1e-6;
    // bool split_sgm;
    double u_begin{0}, u_end{1};
    std::string s_layup_name;
    for (auto p_xn_layup = p_xn_segments->first_node("layup"); p_xn_layup;
      p_xn_layup = p_xn_layup->next_sibling("layup")) {
      // std::cout << "reading layup" << std::endl;

      // split_sgm = false;

      xml_attribute<> *p_xa_begin{p_xn_layup->first_attribute("begin")};
      if (p_xa_begin) {
        u_begin = atof(p_xa_begin->value());
      }
      
      // using x2 coordinate to partion segments
      // convert it into length-based partition
      xml_attribute<> *p_xa_begin_x2{p_xn_layup->first_attribute("begin_x2")};
      int pos{0};
      if (p_xa_begin_x2) {
        std::string begin_x2_value{p_xa_begin_x2->value()};
        pos = begin_x2_value.find(":");
        double begin_x2 = std::stod(begin_x2_value.substr(0,pos));
        int intersection_count = (pos != std::string::npos) ? std::stoi(begin_x2_value.substr(pos+1)) : 1;
        u_begin = findPointOnPolylineByCoordinate(p_bsl->vertices(), begin_x2, tol, intersection_count);
      }
      xml_attribute<> *p_xa_begin_x3{p_xn_layup->first_attribute("begin_x3")};
      if (p_xa_begin_x3) {
        std::string begin_x3_value{p_xa_begin_x3->value()};
        pos = begin_x3_value.find(":");
        double begin_x3 = std::stod(begin_x3_value.substr(0,pos));
        int intersection_count = (pos != std::string::npos) ? std::stoi(begin_x3_value.substr(pos+1)) : 1;
        u_begin = findPointOnPolylineByCoordinate(p_bsl->vertices(), begin_x3, tol, intersection_count);
      }

      xml_attribute<> *p_xa_end{p_xn_layup->first_attribute("end")};
      if (p_xa_end) {
        u_end = atof(p_xa_end->value());
      }
      xml_attribute<> *p_xa_end_x2{p_xn_layup->first_attribute("end_x2")};
      if (p_xa_end_x2) {
        std::string end_x2_value{p_xa_end_x2->value()};
        pos = end_x2_value.find(":");
        double end_x2 = std::stod(end_x2_value.substr(0,pos));
        int intersection_count = (pos != std::string::npos) ? std::stoi(end_x2_value.substr(pos+1)) : 1;
        u_end = findPointOnPolylineByCoordinate(p_bsl->vertices(), end_x2, tol, intersection_count);
      }
      xml_attribute<> *p_xa_end_x3{p_xn_layup->first_attribute("end_x3")};
      if (p_xa_end_x3) {
        std::string end_x3_value{p_xa_end_x3->value()};
        pos = end_x3_value.find(":");
        double end_x3 = std::stod(end_x3_value.substr(0,pos));
        int intersection_count = (pos != std::string::npos) ? std::stoi(end_x3_value.substr(pos+1)) : 1;
        u_end = findPointOnPolylineByCoordinate(p_bsl->vertices(), end_x3, tol, intersection_count);
      }

      while (u_begin <= -1 || u_end > 1) {
        if (u_begin <= -1) {
          u_begin += 1;
          u_end += 1;
        }
        else if (u_end > 1) {
          u_begin -= 1;
          u_end -= 1;
        }
      }
      // std::cout << "\nu_begin = " << u_begin << ", u_end = " << u_end << std::endl;

      if (u_begin < 0 && u_end < 0) {
        v_u_begin.push_back(u_begin + 1);
        v_u_end.push_back(u_end + 1);
        v_s_layup.push_back(p_xn_layup->value());
      }
      else if (u_begin < 0 && u_end >= 0) {
        // split_sgm = true;
        v_u_begin.push_back(u_begin + 1);
        v_u_end.push_back(1.0);
        v_s_layup.push_back(p_xn_layup->value());
        v_u_begin.push_back(0.0);
        v_u_end.push_back(u_end);
        v_s_layup.push_back(p_xn_layup->value());
      }
      else {
        v_u_begin.push_back(u_begin);
        v_u_end.push_back(u_end);
        v_s_layup.push_back(p_xn_layup->value());
      }

    }

    for (auto k = 0; k < v_s_layup.size(); k++) {
      u_begin = v_u_begin[k];
      u_end = v_u_end[k];
      s_layup_name = v_s_layup[k];

      // Create the sorted list of points
      std::vector<double>::iterator it;
      if (v_u_sorted.size() == 0) v_u_sorted.push_back(u_begin);
      else {
        for (it = v_u_sorted.begin(); it < v_u_sorted.end(); it++) {
          // std::cout << "  *it = " << *it << std::endl;
          if (*it == u_begin) break;
          else if (*it > u_begin) {
            v_u_sorted.insert(it, u_begin);
            break;
          }
        }
        if (it == v_u_sorted.end()) v_u_sorted.push_back(u_begin);
      }
      for (it = v_u_sorted.begin(); it < v_u_sorted.end(); it++) {
        // std::cout << "  *it = " << *it << std::endl;
        if (*it == u_end) break;
        else if (*it > u_end) {
          v_u_sorted.insert(it, u_end);
          break;
        }
      }
      if (it == v_u_sorted.end()) v_u_sorted.push_back(u_end);
      // if (config.debug) {
      //   std::cout << "\nsorted list of points" << std::endl;
      //   for (auto u : v_u_sorted) std::cout << " " << u;
      //   std::cout << std::endl;
      // }

      // std::string s_layup_name{v_s_layup[k]};
      Layup *p_layup_temp;
      LayerType *p_layertype_temp;
      Material *p_material_temp;
      p_layup_temp = pmodel->getLayupByName(s_layup_name);
      if (p_layup_temp != nullptr) {
        for (auto layer : p_layup_temp->getLayers()) {
          // Layer type has been added to the used list if its id > 0
          // If layer type has been used,
          // then the corresponding material must also been used
          p_layertype_temp = layer.getLayerType();
          if (p_layertype_temp->id() == 0) {
            CrossSection::used_layertype_index++;
            p_layertype_temp->setId(CrossSection::used_layertype_index);
            cs->addUsedLayerType(p_layertype_temp);

            // Check the material
            p_material_temp = p_layertype_temp->material();
            if (p_material_temp->id() == 0) {
              CrossSection::used_material_index++;
              p_material_temp->setId(CrossSection::used_material_index);
              cs->addUsedMaterial(p_material_temp);
            }
          }
        }
      }
      else {
        // Raise exception
        std::cout << "[error] cannot find layup: " << s_layup_name << std::endl;
      }
      v_p_layup.push_back(p_layup_temp);

    }

    if (config.debug) {
      std::cout << "\nsorted list of points" << std::endl;
      for (auto u : v_u_sorted) std::cout << " " << u;
      std::cout << std::endl;
    }

    // int n_sgms = v_p_layup.size();
    std::size_t n_sgms = v_u_sorted.size() - 1;



    // Combine layups
    std::vector<Layup *> v_p_layup_combined;

    std::vector< std::vector<double> > vv_u;
    std::vector< std::vector<Layup *> > vv_p_layup;
    Layup *tmp_layup_combined;
    for (int i = 0; i < n_sgms; i++) {
      // For every two adjacent u points,
      // find out all layups in this segment
      double sgm_pos = v_u_sorted[i];
      std::vector<double> tmp_v_u{v_u_sorted[i], v_u_sorted[i+1]};
      vv_u.push_back(tmp_v_u);

      // Sweep line for all u points from the begin to the end
      // except the last one.
      // The layup is counted if the sweep line is at the beginning
      // or in the segment.
      std::vector<Layup *> tmp_v_p_layup;
      for (int j = 0; j < v_p_layup.size(); j++) {
        if (sgm_pos >= v_u_begin[j] && sgm_pos < v_u_end[j]) {
          tmp_v_p_layup.push_back(v_p_layup[j]);
        }
      }
      vv_p_layup.push_back(tmp_v_p_layup);

      // Create a new combined layup if there are multiple layups
      // otherwise, use the existing one
      if (tmp_v_p_layup.size() > 1) {
        num_combined_layups++;
        std::string tmp_name = "layup_cmb_" + std::to_string(num_combined_layups);
        tmp_layup_combined = new Layup(tmp_name);
        combineLayups(tmp_layup_combined, tmp_v_p_layup);
        pmodel->addLayup(tmp_layup_combined);
        v_p_layup_combined.push_back(tmp_layup_combined);
      }
      else {
        v_p_layup_combined.push_back(tmp_v_p_layup[0]);
      }
    }

    // std::cout << "\nbefore combine segments" << std::endl;
    // for (int i = 0; i < n_sgms; i++) {
    //   std::cout << "\nsegment " << i + 1;
    //   std::cout << " [" << vv_u[i][0] << ", " << vv_u[i][1] << "]\n";
    //   for (int j = 0; j < vv_p_layup[i].size(); j++) {
    //     std::cout << "  " << vv_p_layup[i][j]->getName() << std::endl;
    //   }
    //   v_p_layup_combined[i]->print(pmessage, 9);
    // }

    // Combine successive layups and points if layups are the same
    std::stack<int> tmp_index_to_erase;
    for (auto i = 1; i < v_u_sorted.size()-1; i++) {
      // v_p_layup_combined[i]->print(pmessage, 9);
      if (*(v_p_layup_combined[i]) == *(v_p_layup_combined[i-1])) {
        // std::cout << "i = " << i << std::endl;
        tmp_index_to_erase.push(i);
      }
    }

    while (!tmp_index_to_erase.empty()) {
      // std::cout << "erase index " << tmp_index_to_erase.top() << std::endl;
      v_u_sorted.erase(
        v_u_sorted.begin() + tmp_index_to_erase.top()
      );
      v_p_layup_combined.erase(
        v_p_layup_combined.begin() + tmp_index_to_erase.top()
      );
      tmp_index_to_erase.pop();
    }

    n_sgms = v_u_sorted.size() - 1;

    // std::cout << "\nafter combine segments" << std::endl;
    // for (int i = 0; i < n_sgms; i++) {
    //   std::cout << "\nsegment " << i + 1;
    //   std::cout << " [" << vv_u[i][0] << ", " << vv_u[i][1] << "]\n";
    //   for (int j = 0; j < vv_p_layup[i].size(); j++) {
    //     std::cout << "  " << vv_p_layup[i][j]->getName() << std::endl;
    //   }
    //   v_p_layup_combined[i]->print(pmessage, 9);
    // }



    // Insert new vertices to the base line
    // std::cout << "\nbase line before\n";
    // p_bsl->print(pmessage, 9);

    // double tol = 1e-6;
    std::stack<int> pos_insert;
    std::stack<PDCELVertex *> newv_insert;
    std::vector<int> index_split, index_shift;
    int i_shift = 0;
    for (auto i = 0; i < v_u_sorted.size(); i++) {
      if (v_u_sorted[i] == 0.0) {
        index_split.push_back(0);
        index_shift.push_back(0);
      }
      else if (v_u_sorted[i] == 1.0) {
        index_split.push_back(p_bsl->vertices().size() - 1);
        index_shift.push_back(i_shift);
      }
      else if (v_u_sorted[i] > 0.0 && v_u_sorted[i] < 1.0) {
        std::vector<PDCELVertex *> tmp_v_p_vertex = p_bsl->vertices();
        PDCELVertex *p_v_param;
        bool is_new;
        int i_seg;
        p_v_param = findParamPointOnPolyline(
          tmp_v_p_vertex, v_u_sorted[i], is_new, i_seg, tol
        );
        // std::cout << "u = " << v_u_sorted[i];
        // std::cout << ", i_seg = " << i_seg;
        // std::cout << ", is_new = " << is_new;
        // std::cout << ", p_v_param = " << p_v_param->point() << std::endl;
        index_split.push_back(i_seg);
        index_shift.push_back(i_shift);
        if (is_new) {
          pos_insert.push(i_seg);
          newv_insert.push(p_v_param);
          i_shift++;
        }
      }
    }
    while (!newv_insert.empty()) {
      p_bsl->insertPVertex(pos_insert.top(), newv_insert.top());
      pos_insert.pop();
      newv_insert.pop();
    }

    // std::cout << "\nbase line after\n";
    // p_bsl->print(pmessage, 9);

    // std::cout << "\nsplit index before\n";
    // for (auto i : index_split) std::cout << " " << i;
    for (int i = 0; i < index_split.size(); i++) {
      if (index_split[i] != 0) {
        index_split[i] += index_shift[i];
      }
    }
    // std::cout << "\nsplit index after\n";
    // for (auto i : index_split) std::cout << " " << i;



    // Split the base line
    std::vector<Baseline *> v_p_bsl;
    std::vector<PDCELVertex *> tmp_v_full = p_bsl->vertices();
    Baseline *tmp_p_bsl;
    for (int i = 0; i < n_sgms; i++) {
      if (index_split[i] == 0 && index_split[i+1] == tmp_v_full.size() - 1) {
        tmp_p_bsl = new Baseline(p_bsl);
      }
      else {
        tmp_p_bsl = new Baseline(
          p_bsl->getName() + "_sub_" + std::to_string(v_p_bsl.size()+1), "straight"
        );
        for (int j = index_split[i]; j <= index_split[i+1]; j++) {
          tmp_p_bsl->addPVertex(tmp_v_full[j]);
        }
      }
      v_p_bsl.push_back(tmp_p_bsl);
    }

    // if (config.debug) {
    // std::cout << "\nbase line divided\n";
    // for (auto bsl : v_p_bsl) {
    //   bsl->print(pmessage, 9);
    //   std::cout << std::endl;
    // }
    // }



    // Create segments
    // std::cout << "\nsegments divided\n";
    for (auto i = 0; i < n_sgms; ++i) {
      Segment::count_tmp++;
      std::string s_sgm_name = "sgm_" + std::to_string(Segment::count_tmp);
      int i_freeend = -1;
      Segment *p_sgm = new Segment(
        pmodel, s_sgm_name, v_p_bsl[i], v_p_layup_combined[i], s_layup_side, 0
      );
      // p_sgm->setUBegin(v_u_begin[i]);
      // p_sgm->setUEnd(v_u_end[i]);
      p_sgm->setFreeEnd(i_freeend);
      p_sgm->setClosed(false);

      p_sgm->setMatOrient1(p_component->getMatOrient1());
      p_sgm->setMatOrient2(p_component->getMatOrient2());

      p_component->addSegment(p_sgm);
      // std::cout << p_sgm << std::endl;
    }

  }


  // Read trim config
  for (auto p_xn_trim = xn_component->first_node("trim"); p_xn_trim;
        p_xn_trim = p_xn_trim->next_sibling("trim")) {


    std::string loc;
    double x2, x3;

    xml_node<> *p_xn_location{p_xn_trim->first_node("location")};
    if (p_xn_location) {
      loc = p_xn_location->value();
      if (loc == "head") {
        p_component->setTrimHead(true);
      }
      else if (loc == "tail") {
        p_component->setTrimTail(true);
      }
    }

    xml_node<> *p_xn_direction{p_xn_trim->first_node("direction")};
    if (p_xn_direction) {
      std::stringstream ss{p_xn_direction->value()};
      ss >> x2 >> x3;
      if (loc == "head") {
        p_component->setTrimHeadVector(x2, x3);
      }
      else if (loc == "tail") {
        p_component->setTrimTailVector(x2, x3);
      }
    }

  }




  // if (config.debug) {
  //   pmessage->printBlank();
  //   pmessage->print(9, "summary of layups");
  //   for (auto lyp : pmodel->layups()) {
  //     lyp->print(pmessage, 9);
  //     pmessage->printBlank();
  //   }
  // }


  //
  // Read joint style
  //
  for (auto p_xn_joint = xn_component->first_node("joint"); p_xn_joint;
        p_xn_joint = p_xn_joint->next_sibling("joint")) {
    int js = atoi(p_xn_joint->first_attribute("style")->value());
    std::vector<std::string> sns;
    sns = splitString(p_xn_joint->value(), ',');
    p_component->addJointSegments(sns);
    p_component->addJointStyle(js);
  }

  return 0;

}
