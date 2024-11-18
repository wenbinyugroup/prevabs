#include "PModel.hpp"
#include "PModelIO.hpp"

#include "PDataClasses.hpp"
#include "globalVariables.hpp"
#include "plog.hpp"
#include "utilities.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>



int readInputDehomo(const std::string &filenameCrossSection,
                    const std::string &filePath, PModel *pmodel,
                    Message *pmessage) {
  pmessage->increaseIndent();

  // -----------------------------------------------------------------
  // Open xml file
  xml_document<> xd_cs;
  std::ifstream ifs_cs{config.main_input};
  if (!ifs_cs.is_open()) {
    // std::cout << markError << " Unable to open file: " << config.main_input
    //           << std::endl;
    PLOG(error) << pmessage->message("unable to open file: " + config.main_input);
    return 1;
  } else {
    // printInfo(i_indent, "reading main input data: " + config.main_input);
    PLOG(info) << pmessage->message("reading main input data: " + config.main_input);
  }

  std::vector<char> buffer{(std::istreambuf_iterator<char>(ifs_cs)),
                           std::istreambuf_iterator<char>()};
  buffer.push_back('\0');

  try {
    xd_cs.parse<0>(&buffer[0]);
  } catch (parse_error &e) {
    // std::cout << markError << " Unable to parse the file: " << config.main_input
    //           << std::endl;
    PLOG(error) << pmessage->message("unable to parse file: " + config.main_input);
    std::cerr << e.what() << std::endl;
  }


  // ------------------------------
  xml_node<> *p_xn_sg{xd_cs.first_node("cross_section")};
  if (!p_xn_sg) {
    p_xn_sg = xd_cs.first_node("sg");
  }
  // std::string s_file_name_glb;
  // s_file_name_glb = config.file_name_vsc + ".glb";

  // ------------------------------
  xml_node<> *p_xn_analysis{p_xn_sg->first_node("analysis")};

  int model_dim = 1;
  xml_node<> *p_xn_model_dim{p_xn_analysis->first_node("model_dim")};
  if (p_xn_model_dim) {
    std::string ss{p_xn_model_dim->value()};
    if (ss[0] != '\0') {
      model_dim = atoi(ss.c_str());
    }
  }
  pmodel->setAnalysisModelDim(model_dim);

  int model = 0;
  if (p_xn_analysis) {
    xml_node<> *p_xn_model{p_xn_analysis->first_node("model")};
    if (p_xn_model) {
      model = atoi(p_xn_model->value());
    }
  }
  pmodel->setAnalysisModel(model);



  // ------------------------------
  // Read load cases
  printInfo(i_indent, "reading structural analysis results");

  xml_node<> *p_xn_global{p_xn_sg->first_node("global")};

  xml_attribute<> *p_xa_temp = p_xn_global->first_attribute("measure");
  int measure{0};
  if (p_xa_temp) {
    std::string s_measure{p_xa_temp->value()};
    if (s_measure == "stress" || s_measure == "0") measure = 0;
    else if (s_measure == "strain" || s_measure == "1") measure = 1;
  }


  // Will be deprecated
  xml_node<> *p_xn_loads{p_xn_global->first_node("loads")};
  if (p_xn_loads) {
    LoadCase lc = readXMLElementLoadCase(p_xn_global, measure, model, pmodel, pmessage);
    pmodel->addLoadCase(lc);
  }


  for (auto p_xn_case = p_xn_global->first_node(); p_xn_case;
       p_xn_case = p_xn_case->next_sibling()) {

    std::string _name{p_xn_case->name()};

    if (_name == "case") {
      LoadCase lc = readXMLElementLoadCase(p_xn_case, measure, model, pmodel, pmessage);
      pmodel->addLoadCase(lc);
    }
    else if (_name == "include") {
      readXMLElementLoadCaseInclude(p_xn_case, measure, model, pmodel, pmessage);
    }

  }

  if (pmodel->getLoadCases().size() == 0) {
    LoadCase lc;
    lc.measure = measure;
    pmodel->addLoadCase(lc);
  }

  pmessage->decreaseIndent();

  return 1;
}









int readOutputDehomo(const std::string &fn_sg, PModel *pmodel, Message *pmessage) {
  pmessage->increaseIndent();


  // TODO: for each load case

  int case_id = 1;
  LocalState *state = new LocalState(case_id);


  std::string fn;

  if (config.dehomo) {
    PLOG(info) << pmessage->message("reading dehomogenization outputs...");

    // Read node data (displacement)
    fn = fn_sg + ".U";
    readVABSU(fn, state, pmessage);

    // Read element data (stress and strain)
    if (config.analysis_tool == 1) {
      fn = fn_sg + ".ELE";
      readVABSEle(fn, state, pmessage);
    }
    else if (config.analysis_tool == 2) {
      fn = fn_sg + ".SN";
      readSCSn(fn, state, pmessage);
    }

    // Read element nodal data (stress and strain)
  }

  if (config.fail_strength || config.fail_index || config.fail_envelope) {
    PLOG(info) << pmessage->message("reading failure analysis outputs...");

    // Read element data (failure index and strength ratio)
    fn = fn_sg + ".fi";
    readMsgFi(fn, state, pmodel->getMesh()->getElements().size(), pmessage);
  }

  pmodel->addLocalState(state);


  pmessage->decreaseIndent();

  return 0;
}









LoadCase readXMLElementLoadCase(
  const xml_node<> *p_xn_loadcase, const int &measure, const int &model,
  PModel *pmodel, Message *pmessage
  ) {

  LoadCase loadcase;
  loadcase.measure = measure;

  std::vector<double> vd_rotation(9, 0.0);

  std::vector<double> vd_load;

  // if (p_xn_global) {
  xml_attribute<> *p_xa_temp = p_xn_loadcase->first_attribute("measure");
  if (p_xa_temp) {
    std::string s_measure{p_xa_temp->value()};
    if (s_measure == "stress" || s_measure == "0") loadcase.measure = 0;
    else if (s_measure == "strain" || s_measure == "1") loadcase.measure = 1;
  }

  xml_node<> *p_xn_temp;

  p_xn_temp = p_xn_loadcase->first_node("displacements");
  if (p_xn_temp && (splitString(p_xn_temp->value(), ' ')).size() != 0) {
    loadcase.displacement = parseNumbersFromString(p_xn_temp->value());
  }

  p_xn_temp = p_xn_loadcase->first_node("rotations");
  if (p_xn_temp && (splitString(p_xn_temp->value(), ' ')).size() != 0) {
    vd_rotation = parseNumbersFromString(p_xn_temp->value());
  }
  loadcase.rotation[0] =
      std::vector<double>(vd_rotation.begin(), vd_rotation.begin() + 3);
  loadcase.rotation[1] =
      std::vector<double>(vd_rotation.begin() + 3, vd_rotation.begin() + 6);
  loadcase.rotation[2] =
      std::vector<double>(vd_rotation.begin() + 6, vd_rotation.begin() + 9);

  p_xn_temp = p_xn_loadcase->first_node("loads");
  if (p_xn_temp) {
    vd_load = parseNumbersFromString(p_xn_temp->value());
    loadcase.load = vd_load;
  }

  if (pmodel->analysisModelDim() == 1) {  // Beam model
    if (model == 0) {
      loadcase.force[0] = vd_load[0];
      loadcase.moment[0] = vd_load[1];
      loadcase.moment[1] = vd_load[2];
      loadcase.moment[2] = vd_load[3];
    } else if (model == 1) {
      loadcase.force[0] = vd_load[0];
      loadcase.force[1] = vd_load[1];
      loadcase.force[2] = vd_load[2];
      loadcase.moment[0] = vd_load[3];
      loadcase.moment[1] = vd_load[4];
      loadcase.moment[2] = vd_load[5];
    }
  }

  xml_node<> *p_xn_distributed{p_xn_loadcase->first_node("distributed")};
  if (p_xn_distributed) {
    p_xn_temp = p_xn_distributed->first_node("forces");
    if (p_xn_temp && (splitString(p_xn_temp->value(), ' ')).size() != 0) {
      loadcase.distr_force = parseNumbersFromString(p_xn_temp->value());
    }

    p_xn_temp = p_xn_distributed->first_node("forces_d1");
    if (p_xn_temp && (splitString(p_xn_temp->value(), ' ')).size() != 0) {
      loadcase.distr_force_d1 = parseNumbersFromString(p_xn_temp->value());
    }

    p_xn_temp = p_xn_distributed->first_node("forces_d2");
    if (p_xn_temp && (splitString(p_xn_temp->value(), ' ')).size() != 0) {
      loadcase.distr_force_d2 = parseNumbersFromString(p_xn_temp->value());
    }

    p_xn_temp = p_xn_distributed->first_node("forces_d3");
    if (p_xn_temp && (splitString(p_xn_temp->value(), ' ')).size() != 0) {
      loadcase.distr_force_d3 = parseNumbersFromString(p_xn_temp->value());
    }

    p_xn_temp = p_xn_distributed->first_node("moments");
    if (p_xn_temp && (splitString(p_xn_temp->value(), ' ')).size() != 0) {
      loadcase.distr_moment = parseNumbersFromString(p_xn_temp->value());
    }

    p_xn_temp = p_xn_distributed->first_node("moments_d1");
    if (p_xn_temp && (splitString(p_xn_temp->value(), ' ')).size() != 0) {
      loadcase.distr_moment_d1 = parseNumbersFromString(p_xn_temp->value());
    }

    p_xn_temp = p_xn_distributed->first_node("moments_d2");
    if (p_xn_temp && (splitString(p_xn_temp->value(), ' ')).size() != 0) {
      loadcase.distr_moment_d2 = parseNumbersFromString(p_xn_temp->value());
    }

    p_xn_temp = p_xn_distributed->first_node("moments_d3");
    if (p_xn_temp && (splitString(p_xn_temp->value(), ' ')).size() != 0) {
      loadcase.distr_moment_d3 = parseNumbersFromString(p_xn_temp->value());
    }
  }

  if (config.fail_envelope) {
    loadcase.envelope_axis1 = p_xn_loadcase->first_node("axis1")->value();
    loadcase.envelope_axis2 = p_xn_loadcase->first_node("axis2")->value();
    loadcase.envelope_div = 10;
    p_xn_temp = p_xn_loadcase->first_node("divisions");
    if (p_xn_temp && (splitString(p_xn_temp->value(), ' ')).size() != 0) {
      loadcase.envelope_div = atoi(p_xn_loadcase->first_node("divisions")->value());
    }
  }
  // }

  return loadcase;
}









int readXMLElementLoadCaseInclude(
  const xml_node<> *p_xn_loadcase, const int &measure, const int &model,
  PModel *pmodel, Message *pmessage
  ) {

  std::string fmt{"csv"};
  xml_attribute<> *p_xa_temp = p_xn_loadcase->first_attribute("format");
  if (p_xa_temp) {
    fmt = p_xa_temp->value();
  }

  std::string fn{p_xn_loadcase->value()};
  fn = config.file_directory + fn;
  // printInfo(i_indent, "reading load cases from file <" + fn + ">...");

  if (fmt == "csv") {
    int headrow{1};
    xml_attribute<> *p_xa_temp = p_xn_loadcase->first_attribute("header");
    if (p_xa_temp) {
      std::string s_temp = p_xa_temp->value();
      headrow = atoi(s_temp.c_str());
    }
    readLoadCasesFromCSV(fn, headrow, measure, model, pmodel, pmessage);
  }

  return 0;
}









int readLoadCasesFromCSV(
  const std::string &fn, const int &head, const int &measure, const int &model,
  PModel *pmodel, Message *pmessage
  ) {

  PLOG(info) << pmessage->message("reading load cases from file <" + fn + ">...");

  std::vector<std::vector<std::string>> s_load_cases;

  parseCSVString(fn, s_load_cases);

  for (auto i = 0; i < s_load_cases.size(); i++) {
    if (i >= head) {

      // for (auto l : load_case) {
      //   std::cout << l << "  ";
      // }
      // std::cout << "\n";

      LoadCase loadcase;
      loadcase.measure = measure;
      std::vector<std::string> load_case = s_load_cases[i];

      if (model == 0) {
        loadcase.force[0]  = atof(load_case[0].c_str());
        loadcase.moment[0] = atof(load_case[1].c_str());
        loadcase.moment[1] = atof(load_case[2].c_str());
        loadcase.moment[2] = atof(load_case[3].c_str());
      } else if (model == 1) {
        loadcase.force[0]  = atof(load_case[0].c_str());
        loadcase.force[1]  = atof(load_case[1].c_str());
        loadcase.force[2]  = atof(load_case[2].c_str());
        loadcase.moment[0] = atof(load_case[3].c_str());
        loadcase.moment[1] = atof(load_case[4].c_str());
        loadcase.moment[2] = atof(load_case[5].c_str());
      }

      pmodel->addLoadCase(loadcase);

    }
  }

  return 0;
}









int PModel::writeGLB(std::string fn, Message *pmessage) {
  pmessage->increaseIndent();
  PLOG(info) << pmessage->message("writing glb file: " + fn);

  FILE *file;
  file = fopen(fn.c_str(), "w");

  // Write material strength properties for strength analysis
  if (config.fail_strength || config.fail_index || config.fail_envelope) {
    for (auto m : _cross_section->getUsedMaterials()) {
      // m->printMaterial();
      m->writeStrengthProperties(file, pmessage);
    }
  }
  fprintf(file, "\n");

  // Write load case(s)
  for (auto i = 0; i < _load_cases.size(); i++) {

    LoadCase loadcase = _load_cases[i];
    if (config.analysis_tool == 1) { // VABS
      if (i == 0) {
        writeVectorToFile(file, loadcase.displacement);
        fprintf(file, "\n");
        writeVectorToFile(file, loadcase.rotation[0]);
        writeVectorToFile(file, loadcase.rotation[1]);
        writeVectorToFile(file, loadcase.rotation[2]);
        fprintf(file, "\n");
      }

      fprintf(file, "%16e", loadcase.force[0]);
      writeVectorToFile(file, loadcase.moment);
      if (_analysis_model == 1) { // timoshenko model
        fprintf(file, "%16e%16e\n", loadcase.force[1], loadcase.force[2]);
        fprintf(file, "\n");
      }

      if (_analysis_model == 1 && i == 0) { // timoshenko model
        writeVectorToFile(file, loadcase.distr_force, "%16e", false);
        writeVectorToFile(file, loadcase.distr_moment);
        writeVectorToFile(file, loadcase.distr_force_d1, "%16e", false);
        writeVectorToFile(file, loadcase.distr_moment_d1);
        writeVectorToFile(file, loadcase.distr_force_d2, "%16e", false);
        writeVectorToFile(file, loadcase.distr_moment_d2);
        writeVectorToFile(file, loadcase.distr_force_d3, "%16e", false);
        writeVectorToFile(file, loadcase.distr_moment_d3);
        fprintf(file, "\n");
      }
    }


    else if (config.analysis_tool == 2) { // SwiftComp
      if (config.dehomo) {
        writeVectorToFile(file, loadcase.displacement);
        fprintf(file, "\n");
        writeVectorToFile(file, loadcase.rotation[0]);
        writeVectorToFile(file, loadcase.rotation[1]);
        writeVectorToFile(file, loadcase.rotation[2]);
        fprintf(file, "\n");
      }

      fprintf(file, "%8d\n", loadcase.measure);
      fprintf(file, "\n");

      if (config.fail_envelope) {
        std::vector<std::string> v_s_order;
        if (_analysis_model == 0) {
          if (loadcase.measure == 0) v_s_order = {"f1", "m1", "m2", "m3"};
          else if (loadcase.measure == 1) v_s_order = {"e11", "k11", "k12", "k13"};
        }
        else if (_analysis_model == 1) {
          if (loadcase.measure == 0) v_s_order = {"f1", "f2", "f3", "m1", "m2", "m3"};
          else if (loadcase.measure == 1) v_s_order = {"e11", "g12", "g13", "k11", "k12", "k13"};
        }

        for (int i = 0; i < v_s_order.size(); i++) {
          if (loadcase.envelope_axis1 == v_s_order[i]) {
            fprintf(file, "%8d", i+1);
            break;
          }
        }
        for (int i = 0; i < v_s_order.size(); i++) {
          if (loadcase.envelope_axis2 == v_s_order[i]) {
            fprintf(file, "%8d", i+1);
            break;
          }
        }
        fprintf(file, "\n");
      }

      else if (config.dehomo || config.fail_index) {
        writeVectorToFile(file, loadcase.load);
      }

    }
  }

  fprintf(file, "\n");

  fclose(file);

  pmessage->decreaseIndent();

  return 1;
}









int readVABSU(const std::string &filename, LocalState *state, Message *pmessage) {
  pmessage->increaseIndent();

  // TODO: multiple load cases

  std::vector<std::string> col_name{"U1", "U2", "U3"};

  PElementNodeData *p_nd = new PElementNodeData(0, 1, "Displacement");

  p_nd->setComponentLabels(col_name);

  std::ifstream ifen;
  ifen.open(filename);
  if (ifen.fail()) {
    PLOG(error) << pmessage->message("unable to find the file: " + filename);
  } else {
    PLOG(info) << pmessage->message("reading VABS recovered data: " + filename);

    while (ifen) {

      std::string line;
      int nid;
      double x2, x3, value;

      getline(ifen, line);

      if (!line.empty() && line[line.size() - 1] == '\r')
        line.erase(line.size() - 1);
      if (line.empty())
        continue;

      std::stringstream ss;
      ss << line;
      ss >> nid >> x2 >> x3;

      PElementNodeDatum *p_ni = new PElementNodeDatum(nid);

      for (int i = 0; i < 3; i++) {
        ss >> value;
        p_ni->addData(value);
      }

      p_nd->addData(p_ni);
    }
  }
  ifen.close();

  state->setU(p_nd);

  pmessage->decreaseIndent();

  // return data;
  return 0;
}









// std::vector<PElementNodeData>
// readOutputDehomoElementVABS(const std::string &filename, Message *pmessage) {
int readVABSEle(const std::string &filename, LocalState *state, Message *pmessage) {
  pmessage->increaseIndent();

  // TODO: multiple load cases

  std::vector<std::string> col_name_e{
    "E11", "2E12", "2E13", "E22", "2E23", "E33"
  };
  std::vector<std::string> col_name_s{
    "S11", "S12", "S13", "S22", "S23", "S33"
  };
  std::vector<std::string> col_name_em{
    "EM11", "2EM12", "2EM13", "EM22", "2EM23", "EMN3"
  };
  std::vector<std::string> col_name_sm{
    "SM11", "SM12", "SM13", "SM22", "SM23", "SM33"
  };

  PElementNodeData *p_elm_e = new PElementNodeData(1, 2, "Strain (global)");
  PElementNodeData *p_elm_s = new PElementNodeData(1, 2, "Stress (global)");
  PElementNodeData *p_elm_em = new PElementNodeData(1, 2, "Strain (material)");
  PElementNodeData *p_elm_sm = new PElementNodeData(1, 2, "Stress (material)");

  p_elm_e->setComponentLabels(col_name_e);
  p_elm_s->setComponentLabels(col_name_s);
  p_elm_em->setComponentLabels(col_name_em);
  p_elm_sm->setComponentLabels(col_name_sm);

  // std::vector<std::vector<double>> data;
  // std::vector<PElementNodeData> elemdata_all;

  std::ifstream ifen;
  ifen.open(filename);
  if (ifen.fail()) {
    PLOG(error) << pmessage->message("unable to find the file: " + filename);
  } else {
    PLOG(info) << pmessage->message("reading VABS recovered data: " + filename);

    while (ifen) {

      std::string line;
      int eid;
      double value;

      getline(ifen, line);

      if (!line.empty() && line[line.size() - 1] == '\r')
        line.erase(line.size() - 1);
      if (line.empty())
        continue;

      std::stringstream ss;
      ss << line;
      ss >> eid;

      PElementNodeDatum *p_ei_e = new PElementNodeDatum(eid);
      PElementNodeDatum *p_ei_s = new PElementNodeDatum(eid);
      PElementNodeDatum *p_ei_em = new PElementNodeDatum(eid);
      PElementNodeDatum *p_ei_sm = new PElementNodeDatum(eid);

      for (int i = 0; i < 6; i++) {
        ss >> value;
        p_ei_e->addData(value);
      }
      for (int i = 0; i < 6; i++) {
        ss >> value;
        p_ei_s->addData(value);
      }
      for (int i = 0; i < 6; i++) {
        ss >> value;
        p_ei_em->addData(value);
      }
      for (int i = 0; i < 6; i++) {
        ss >> value;
        p_ei_sm->addData(value);
      }

      p_elm_e->addData(p_ei_e);
      p_elm_s->addData(p_ei_s);
      p_elm_em->addData(p_ei_em);
      p_elm_sm->addData(p_ei_sm);

      // if (ei != nelem)
      //   ei++; // next element
      // enei.clear();
    }
  }
  ifen.close();

  state->setE(p_elm_e);
  state->setS(p_elm_s);
  state->setEM(p_elm_em);
  state->setSM(p_elm_sm);

  pmessage->decreaseIndent();

  // return data;
  return 0;
}









int readSCSn(const std::string &filename, LocalState *state, Message *pmessage) {
  pmessage->increaseIndent();

  // TODO: multiple load cases

  std::vector<std::string> comp_names{
    "EN11", "EN22", "EN33", "2EN23", "2EN13", "2EN12",
    "SN11", "SN22", "SN33", "SN23", "SN13", "SN12"
  };

  // p_elm_e->setComponentLabels(col_name_e);
  // p_elm_s->setComponentLabels(col_name_s);

  std::ifstream ifen;
  ifen.open(filename);
  if (ifen.fail()) {
    PLOG(error) << pmessage->message("unable to find the file: " + filename);
  } else {
    PLOG(info) << pmessage->message("reading SwiftComp dehomogenization data: " + filename);

    int comp_id{0};
    PElementNodeData *p_elnd = new PElementNodeData(2, 0, comp_names[comp_id]);
    PLOG(info) << pmessage->message("  reading component " + comp_names[comp_id]);

    std::string line;
    // while (ifen) {
    while (getline(ifen, line)) {
      // getline(ifen, line);

      if (line.empty()) {
        // finish reading one component
        // add the data to the state
        if (comp_id < 6) {
          state->addEN(p_elnd);
        }
        else {
          state->addSN(p_elnd);
        }
        if (comp_id == 11) break;

        // create new empty data
        comp_id++;
        p_elnd = new PElementNodeData(2, 0, comp_names[comp_id]);
        PLOG(info) << pmessage->message("  reading component " + comp_names[comp_id]);
        continue;
      }

      int eid, nnode;
      double value;


      if (!line.empty() && line[line.size() - 1] == '\r')
        line.erase(line.size() - 1);  // remove '\r' at the end of the line

      std::stringstream ss;
      ss << line;
      ss >> eid >> nnode;

      PElementNodeDatum *p_ei = new PElementNodeDatum(eid);

      for (int i = 0; i < nnode; i++) {
        ss >> value;
        p_ei->addData(value);
      }

      p_elnd->addData(p_ei);

      // if (ei != nelem)
      //   ei++; // next element
      // enei.clear();
    }


  }
  ifen.close();


  pmessage->decreaseIndent();

  return 0;
}









int readMsgFi(const std::string &filename, LocalState *state, std::size_t nelem, Message *pmessage) {
  pmessage->increaseIndent();

  // TODO: multiple load cases

  // std::vector<std::string> col_name{"FI", "SR"};

  PElementNodeData *p_data_fi = new PElementNodeData(1, 0, "Failure index");
  PElementNodeData *p_data_sr = new PElementNodeData(1, 0, "Strength ratio");

  // std::cout << "sr data label: " << p_data_sr->getLabel() << std::endl;
  // std::cout << "fi data label: " << p_data_fi->getLabel() << std::endl;

  // p_nd->setComponentLabel(col_name);

  std::ifstream ifen;
  ifen.open(filename);
  double sr_min = 0;
  int eid_sr_min = 0;

  if (ifen.fail()) {
    PLOG(error) << pmessage->message("unable to find the file: " + filename);
  }
  else {
    PLOG(info) << pmessage->message("reading failure analysis result: " + filename);

    int ecount = 0;

    while (ifen) {

      std::string line;
      getline(ifen, line);

      if (!line.empty() && line[line.size() - 1] == '\r')
        line.erase(line.size() - 1);
      if (line.empty())
        continue;

      // Read element_id  failure_index  strength_ratio
      if (ecount < nelem) {
        // std::cout << line << std::endl;
        std::string first_word;
        int id;
        // double fi, sr;
        std::string sfi, ssr;

        std::stringstream ss;
        ss << line;
        ss >> first_word;

        if (first_word == "Failure" || first_word == "The") {
          continue;
        }
        // ss >> id;
        id = atoi(first_word.c_str());
        ecount++;

        PElementNodeDatum *p_datum_fi = new PElementNodeDatum(id);
        PElementNodeDatum *p_datum_sr = new PElementNodeDatum(id);

        // ss >> fi;
        ss >> sfi;
        // p_datum_fi->addData(fi);
        p_datum_fi->addStringData(sfi);
        // ss >> sr;
        ss >> ssr;
        // p_datum_sr->addData(sr);
        p_datum_sr->addStringData(ssr);

        p_data_fi->addData(p_datum_fi);
        p_data_sr->addData(p_datum_sr);

        if (eid_sr_min == 0) {
          eid_sr_min = id;
          sr_min = atof(ssr.c_str());
        }
        else if (sr_min > atof(ssr.c_str())) {
          eid_sr_min = id;
          sr_min = atof(ssr.c_str());
        }
      }
      // Read last line: minimum strength ratio
      // else {

      // }

    }
  }
  ifen.close();

  // std::cout << "minimum sr = " << sr_min << " at element " << eid_sr_min << std::endl;

  // for (auto data_fi : p_data_fi->getData()) {
  //   std::cout << data_fi->getMainId() << "    " << data_fi->getStringData()[0] << std::endl;
  // }

  state->setFI(p_data_fi);
  state->setSR(p_data_sr);
  state->setMinSR(sr_min);
  state->setMinSRElementId(eid_sr_min);

  // std::cout << "sr data label: " << p_data_sr->getLabel() << std::endl;
  // std::cout << "fi data label: " << p_data_fi->getLabel() << std::endl;

  pmessage->decreaseIndent();

  // return data;
  return 0;
}

