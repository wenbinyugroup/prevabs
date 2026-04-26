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
#include <cstddef>
#include <cmath>

namespace {

std::string makeAutoSingleSegmentName(std::size_t index) {
  return "sgm_" + std::to_string(index);
}

std::string makeAutoSegmentsBaseName(
  const xml_node<> *node_segments,
  std::size_t &unnamed_block_counter
) {
  xml_attribute<> *attr_name = node_segments->first_attribute("name");
  if (attr_name != nullptr) {
    const std::string name = trim(attr_name->value());
    if (!name.empty()) {
      return name;
    }
  }

  ++unnamed_block_counter;
  return "sgm_blk_" + std::to_string(unnamed_block_counter);
}

std::string makeGeneratedSegmentName(
  const std::string &base_name,
  std::size_t segment_index,
  std::size_t segment_count
) {
  if (segment_count <= 1) {
    return base_name;
  }

  return base_name + "_" + std::to_string(segment_index + 1);
}

// Register layer types and materials from a layup into the cross-section's
// used-index lists. Only registers items not already assigned an id.
void registerLayupUsage(Layup *p_layup, CrossSection *cs) {
  for (auto layer : p_layup->getLayers()) {
    LayerType *p_lt = layer.getLayerType();
    if (p_lt->id() == 0) {
      CrossSection::used_layertype_index++;
      p_lt->setId(CrossSection::used_layertype_index);
      cs->addUsedLayerType(p_lt);

      Material *p_mat = p_lt->material();
      if (p_mat->id() == 0) {
        CrossSection::used_material_index++;
        p_mat->setId(CrossSection::used_material_index);
        cs->addUsedMaterial(p_mat);
      }
    }
  }
}

// Parse a coordinate attribute of the form "coord" or "coord:nth_intersection"
// and resolve it to a polyline parameter u. No-ops when the attribute is absent.
void resolveCoordParam(
  const xml_attribute<> *attr,
  const std::vector<PDCELVertex *> &vertices,
  PolylineAxis axis, double tol, double &u
) {
  if (!attr) return;
  std::string value{attr->value()};
  int pos = static_cast<int>(value.find(":"));
  const std::string ctx = std::string("<layup ") + attr->name() + "=...>";
  double coord = parseRequiredDouble(value.substr(0, pos), ctx);
  int count = (pos >= 0) ? parseRequiredInt(value.substr(pos + 1), ctx) : 1;
  u = findPolylineParamByCoordinate(vertices, coord, tol, count, axis);
}

}  // namespace

int readXMLElementComponentLaminate(
  PComponent *p_component, const xml_node<> *xn_component,
  std::vector<std::vector<std::string>> & /*dependents_all*/, std::vector<std::string> &depend_names,
  std::vector<Layup *> &p_layups, int &num_combined_layups,
  CrossSection *cs, PModel *pmodel
  ) {
  std::size_t unnamed_segment_counter = 0;
  std::size_t unnamed_segments_block_counter = 0;

  if (xn_component->first_node("location")) {
    p_component->setRefVertex(pmodel->getPointByName(
      xn_component->first_node("location")->value()));
  }

  // Count the total number of segments
  int nseg = 0;
  for (auto nodeSegment = xn_component->first_node("segment"); nodeSegment;
        nodeSegment = nodeSegment->next_sibling("segment")) {
    nseg++;
  }
  // Read segments defined explicitly by a single base line and
  // a single layup in <segment>
  for (auto nodeSegment = xn_component->first_node("segment"); nodeSegment;
        nodeSegment = nodeSegment->next_sibling("segment")) {
    Segment *p_segment;

    std::string segmentName;
    xml_attribute<> *p_xa_name{nodeSegment->first_attribute("name")};
    if (p_xa_name) {
      segmentName = nodeSegment->first_attribute("name")->value();
    }
    else {
      segmentName = makeAutoSingleSegmentName(++unnamed_segment_counter);
    }

    int i_freeend = -1;
    xml_attribute<> *p_xa_free{nodeSegment->first_attribute("free")};
    if (p_xa_free) {
      std::string s_freeend = p_xa_free->value();
      if (s_freeend == "head" || s_freeend == "begin") {
        i_freeend = 0;
      } else if (s_freeend == "tail" || s_freeend == "end") {
        i_freeend = 1;
      }
    }
    std::string baselineName{requireNode(nodeSegment, "baseline", "<segment>")->value()};
    xml_node<> *nodeLayup{requireNode(nodeSegment, "layup", "<segment>")};
    std::string layupName{nodeLayup->value()};
    xml_attribute<> *attrDirection{nodeLayup->first_attribute("direction")};
    std::string layupSide{"left"};
    if (attrDirection) {
      layupSide = lowerString(attrDirection->value());
    }

    Baseline *p_baseline = pmodel->getBaselineByNameCopy(baselineName);
    if (p_baseline == nullptr) {
      throw std::runtime_error(
        "cannot find baseline '" + baselineName + "' in segment '" + segmentName + "'"
      );
    }
    Layup *p_layup = pmodel->getLayupByName(layupName);

    if (p_layup != nullptr) {
      registerLayupUsage(p_layup, cs);
    } else {
      throw std::runtime_error(
        "cannot find layup '" + layupName + "' in segment '" + segmentName + "'"
      );
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
        xml_attribute<> *p_xa_by{p_xn_split->first_attribute("by")};
        if (p_xa_by) {
          split_by = p_xa_by->value();
        }

        if (split_by == "name") {
          v_split = pmodel->getPointByName(p_xn_split->value());
          if (!v_split) {
            throw std::runtime_error(
              "cannot find split point '" + std::string(p_xn_split->value())
              + "' in segment '" + segmentName + "'"
            );
          }
        }
        else if (split_by == "id") {
          v_split = p_baseline->vertices()[
            parseRequiredInt(p_xn_split->value(),
              "<split by='id'> in segment '" + segmentName + "'") - 1];
        }
        else {
          throw std::runtime_error(
            "<split by='" + split_by + "'> in segment '" + segmentName
            + "': unrecognized 'by' value; use 'name' or 'id'"
          );
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

      pmodel->addBaseline(p_bsl_1);
      pmodel->addBaseline(p_bsl_2);

      p_sgm_1 = new Segment(name_1, p_bsl_1, p_layup, layupSide, 0);
      if (i_freeend == 0 || i_freeend == -1) {
        p_sgm_1->setFreeEnd(i_freeend);
      }
      p_sgm_1->setMatOrient1(p_component->getMatOrient1());
      p_sgm_1->setMatOrient2(p_component->getMatOrient2());
      p_component->addSegment(p_sgm_1);

      p_sgm_2 = new Segment(name_2, p_bsl_2, p_layup, layupSide, 0);
      if (i_freeend == 1 || i_freeend == -1) {
        p_sgm_2->setFreeEnd(i_freeend);
      }
      p_sgm_2->setMatOrient1(p_component->getMatOrient1());
      p_sgm_2->setMatOrient2(p_component->getMatOrient2());
      p_component->addSegment(p_sgm_2);

    }


    else {
      p_segment =
          new Segment(segmentName, p_baseline, p_layup, layupSide, 0);
      p_segment->setFreeEnd(i_freeend);
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


  // Read segments defined by a base line and a list of layups.
  // Each layup has attributes of beginning and ending points on
  // the base line.
  for (auto p_xn_segments = xn_component->first_node("segments"); p_xn_segments;
    p_xn_segments = p_xn_segments->next_sibling("segments")) {
    const std::string segment_name_base = makeAutoSegmentsBaseName(
      p_xn_segments, unnamed_segments_block_counter
    );
    std::string s_bsl_name{requireNode(p_xn_segments, "baseline", "<segments>")->value()};
    Baseline *p_bsl = pmodel->getBaselineByName(s_bsl_name);
    if (p_bsl == nullptr) {
      throw std::runtime_error(
        "cannot find baseline '" + s_bsl_name + "' in <segments>"
      );
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
      xml_attribute<> *p_xa_begin{p_xn_layup->first_attribute("begin")};
      if (p_xa_begin)
        u_begin = parseRequiredDouble(p_xa_begin->value(), "<layup begin=...>");

      // x2/x3 coordinate attributes override the normalized begin/end value
      resolveCoordParam(p_xn_layup->first_attribute("begin_x2"),
                        p_bsl->vertices(), PolylineAxis::X2, tol, u_begin);
      resolveCoordParam(p_xn_layup->first_attribute("begin_x3"),
                        p_bsl->vertices(), PolylineAxis::X3, tol, u_begin);

      xml_attribute<> *p_xa_end{p_xn_layup->first_attribute("end")};
      if (p_xa_end)
        u_end = parseRequiredDouble(p_xa_end->value(), "<layup end=...>");

      resolveCoordParam(p_xn_layup->first_attribute("end_x2"),
                        p_bsl->vertices(), PolylineAxis::X2, tol, u_end);
      resolveCoordParam(p_xn_layup->first_attribute("end_x3"),
                        p_bsl->vertices(), PolylineAxis::X3, tol, u_end);

      if (!std::isfinite(u_begin) || !std::isfinite(u_end)) {
        PLOG(error) << "failed to resolve layup begin/end coordinates on"
                    << " baseline '" << s_bsl_name << "'";
        return -1;
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

    for (std::size_t k = 0; k < v_s_layup.size(); ++k) {
      u_begin = v_u_begin[k];
      u_end = v_u_end[k];
      s_layup_name = v_s_layup[k];

      // Create the sorted list of points
      std::vector<double>::iterator it;
      if (v_u_sorted.size() == 0) v_u_sorted.push_back(u_begin);
      else {
        for (it = v_u_sorted.begin(); it < v_u_sorted.end(); it++) {
          if (*it == u_begin) break;
          else if (*it > u_begin) {
            v_u_sorted.insert(it, u_begin);
            break;
          }
        }
        if (it == v_u_sorted.end()) v_u_sorted.push_back(u_begin);
      }
      for (it = v_u_sorted.begin(); it < v_u_sorted.end(); it++) {
        if (*it == u_end) break;
        else if (*it > u_end) {
          v_u_sorted.insert(it, u_end);
          break;
        }
      }
      if (it == v_u_sorted.end()) v_u_sorted.push_back(u_end);
      Layup *p_layup_temp = pmodel->getLayupByName(s_layup_name);
      if (p_layup_temp != nullptr) {
        registerLayupUsage(p_layup_temp, cs);
      } else {
        throw std::runtime_error(
          "cannot find layup '" + s_layup_name + "' in <segments>"
        );
      }
      v_p_layup.push_back(p_layup_temp);

    }

    if (config.debug) {
      std::ostringstream oss;
      oss << "sorted list of points:";
      for (std::size_t i = 0; i < v_u_sorted.size(); ++i) {
        oss << ' ' << v_u_sorted[i];
      }
      PLOG(debug) << oss.str();
    }

    if (v_u_sorted.size() < 2) {
      PLOG(warning) << "<segments> on baseline '" << s_bsl_name
                    << "' does not define any non-empty segment interval;"
                    << " skipping segment creation";
      continue;
    }

    std::size_t n_sgms = v_u_sorted.size() - 1;

    // Combine layups
    std::vector<Layup *> v_p_layup_combined;
    Layup *tmp_layup_combined;
    for (std::size_t i = 0; i < n_sgms; ++i) {
      // For every two adjacent u points,
      // find out all layups in this segment
      double sgm_pos = v_u_sorted[i];

      // Sweep line for all u points from the begin to the end
      // except the last one.
      // The layup is counted if the sweep line is at the beginning
      // or in the segment.
      std::vector<Layup *> tmp_v_p_layup;
      for (std::size_t j = 0; j < v_p_layup.size(); ++j) {
        if (sgm_pos >= v_u_begin[j] && sgm_pos < v_u_end[j]) {
          tmp_v_p_layup.push_back(v_p_layup[j]);
        }
      }

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

    // Combine successive layups and points if layups are the same
    std::stack<std::size_t> tmp_index_to_erase;
    for (std::size_t i = 1; i + 1 < v_u_sorted.size(); ++i) {
      if (*(v_p_layup_combined[i]) == *(v_p_layup_combined[i-1])) {
        tmp_index_to_erase.push(i);
      }
    }

    while (!tmp_index_to_erase.empty()) {
      const std::vector<double>::difference_type erase_index =
        static_cast<std::vector<double>::difference_type>(
          tmp_index_to_erase.top()
        );
      v_u_sorted.erase(
        v_u_sorted.begin() + erase_index
      );
      v_p_layup_combined.erase(
        v_p_layup_combined.begin() + erase_index
      );
      tmp_index_to_erase.pop();
    }

    n_sgms = v_u_sorted.size() - 1;
    // Insert new vertices to the base line
    std::stack<int> pos_insert;
    std::stack<PDCELVertex *> newv_insert;
    std::vector<int> index_split, index_shift;
    int i_shift = 0;
    for (std::size_t i = 0; i < v_u_sorted.size(); ++i) {
      if (v_u_sorted[i] == 0.0) {
        index_split.push_back(0);
        index_shift.push_back(0);
      }
      else if (v_u_sorted[i] == 1.0) {
        index_split.push_back(static_cast<int>(p_bsl->vertices().size()) - 1);
        index_shift.push_back(i_shift);
      }
      else if (v_u_sorted[i] > 0.0 && v_u_sorted[i] < 1.0) {
        std::vector<PDCELVertex *> tmp_v_p_vertex = p_bsl->vertices();
        PDCELVertex *p_v_param;
        bool is_new;
        int i_seg;
        p_v_param = findPolylinePointAtParam(
          tmp_v_p_vertex, v_u_sorted[i], is_new, i_seg, tol
        );
        if (p_v_param == nullptr) {
          PLOG(error) << "findPolylinePointAtParam failed for baseline '"
                      << s_bsl_name << "' at u = " << v_u_sorted[i];
          return -1;
        }
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

    for (std::size_t i = 0; i < index_split.size(); ++i) {
      if (index_split[i] != 0) {
        index_split[i] += index_shift[i];
      }
    }

    // Split the base line
    std::vector<Baseline *> v_p_bsl;
    std::vector<PDCELVertex *> tmp_v_full = p_bsl->vertices();
    Baseline *tmp_p_bsl;
    for (std::size_t i = 0; i < n_sgms; ++i) {
      if (index_split[i] == 0 && index_split[i+1] == tmp_v_full.size() - 1) {
        tmp_p_bsl = new Baseline(p_bsl);
      }
      else {
        tmp_p_bsl = new Baseline(
          p_bsl->getName() + "_sub_" + std::to_string(v_p_bsl.size()+1), "straight"
        );
        for (int j = index_split[i]; j <= index_split[i+1]; ++j) {
          tmp_p_bsl->addPVertex(tmp_v_full[j]);
        }
      }
      v_p_bsl.push_back(tmp_p_bsl);
    }

    // Create segments
    for (std::size_t i = 0; i < n_sgms; ++i) {
      const std::string s_sgm_name = makeGeneratedSegmentName(
        segment_name_base, i, n_sgms
      );
      int i_freeend = -1;
      Segment *p_sgm = new Segment(
        s_sgm_name, v_p_bsl[i], v_p_layup_combined[i], s_layup_side, 0
      );
      p_sgm->setFreeEnd(i_freeend);
      p_sgm->setMatOrient1(p_component->getMatOrient1());
      p_sgm->setMatOrient2(p_component->getMatOrient2());

      p_component->addSegment(p_sgm);
    }

  }

  // Read joint style
  for (auto p_xn_joint = xn_component->first_node("joint"); p_xn_joint;
        p_xn_joint = p_xn_joint->next_sibling("joint")) {
    const int js_value = parseRequiredInt(
      requireAttr(p_xn_joint, "style", "<joint>")->value(), "<joint style=...>");
    const JointStyle js =
        (js_value == static_cast<int>(JointStyle::smooth))
            ? JointStyle::smooth
            : JointStyle::step;
    std::vector<std::string> sns;
    sns = splitString(p_xn_joint->value(), ',');
    p_component->addJointSegments(sns);
    p_component->addJointStyle(js);
  }

  return 0;

}
