#include "Material.hpp"
#include "globalConstants.hpp"
#include "globalVariables.hpp"
#include "overloadOperator.hpp"
#include "utilities.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <cstdio>

std::ostream &operator<<(std::ostream &out, LayerType *lt) {
  if (scientific_format) {
    out << std::scientific;
  }
  out << std::setw(16) << lt->lid << std::setw(16) << lt->p_lmaterial->id()
      << std::setw(16) << lt->langle << std::setw(16)
      << lt->p_lmaterial->getName();
  return out;
}

void Material::print(Message *pmessage, int i_type, int i_indent) {
  std::string msg;
  pmessage->print(i_type, "name: " + mname);
  pmessage->print(i_type, "density = " + std::to_string(mdensity));
  pmessage->print(i_type, "type: " + mtype);

  if (mtype == "isotropic") {
    pmessage->print(i_type, "E = " + std::to_string(melastic[0]));
    pmessage->print(i_type, "nu = " + std::to_string(melastic[1]));
  } else if (mtype == "orthotropic") {
    pmessage->print(i_type, "E1 = " + std::to_string(melastic[0]));
    pmessage->print(i_type, "E2 = " + std::to_string(melastic[1]));
    pmessage->print(i_type, "E3 = " + std::to_string(melastic[2]));
    pmessage->print(i_type, "G12 = " + std::to_string(melastic[3]));
    pmessage->print(i_type, "G13 = " + std::to_string(melastic[4]));
    pmessage->print(i_type, "G23 = " + std::to_string(melastic[5]));
    pmessage->print(i_type, "nu12 = " + std::to_string(melastic[6]));
    pmessage->print(i_type, "nu13 = " + std::to_string(melastic[7]));
    pmessage->print(i_type, "nu23 = " + std::to_string(melastic[8]));
  } else if (mtype == "anisotropic") {
    for (std::size_t i = 0; i < elasticLabelAniso.size(); ++i) {
      pmessage->print(i_type, upperString(elasticLabelAniso[i]) + std::to_string(melastic[i]));
    }
  }

  // if (mfcriterion > 0) {
  //   pmessage->print(i_type, "failure criterion: " + std::to_string(mfcriterion));
  //   if (mtype == "isotropic") {
  //     if (mfcriterion == 1 || mfcriterion == 2) {
  //       pmessage->print(i_type, "xt = " + std::to_string(mstrength[0]));
  //       pmessage->print(i_type, "xc = " + std::to_string(mstrength[1]));
  //     } else if (mfcriterion == 3 || mfcriterion == 4) {
  //       pmessage->print(i_type, "s = " + std::to_string(mstrength[0]));
  //     } else if (mfcriterion == 5) {
  //       pmessage->print(i_type, "x = " + std::to_string(mstrength[0]));
  //     }
  //   } else if (mtype == "orthotropic" || mtype == "anisotropic") {
  //     if (mfcriterion == 1 || mfcriterion == 2 || mfcriterion == 4) {
  //       pmessage->print(i_type, "xt = " + std::to_string(mstrength[0]));
  //       pmessage->print(i_type, "yt = " + std::to_string(mstrength[1]));
  //       pmessage->print(i_type, "zt = " + std::to_string(mstrength[2]));
  //       pmessage->print(i_type, "xc = " + std::to_string(mstrength[3]));
  //       pmessage->print(i_type, "yc = " + std::to_string(mstrength[4]));
  //       pmessage->print(i_type, "zc = " + std::to_string(mstrength[5]));
  //       pmessage->print(i_type, "r = " + std::to_string(mstrength[6]));
  //       pmessage->print(i_type, "t = " + std::to_string(mstrength[7]));
  //       pmessage->print(i_type, "s = " + std::to_string(mstrength[8]));
  //     } else if (mfcriterion == 3) {
  //       pmessage->print(i_type, "x = " + std::to_string(mstrength[0]));
  //       pmessage->print(i_type, "y = " + std::to_string(mstrength[1]));
  //       pmessage->print(i_type, "z = " + std::to_string(mstrength[2]));
  //       pmessage->print(i_type, "r = " + std::to_string(mstrength[3]));
  //       pmessage->print(i_type, "t = " + std::to_string(mstrength[4]));
  //       pmessage->print(i_type, "s = " + std::to_string(mstrength[5]));
  //     } else if (mfcriterion == 5) {
  //       pmessage->print(i_type, "xt = " + std::to_string(mstrength[0]));
  //       pmessage->print(i_type, "yt = " + std::to_string(mstrength[1]));
  //       pmessage->print(i_type, "xc = " + std::to_string(mstrength[3]));
  //       pmessage->print(i_type, "yc = " + std::to_string(mstrength[4]));
  //       pmessage->print(i_type, "r = " + std::to_string(mstrength[6]));
  //       pmessage->print(i_type, "s = " + std::to_string(mstrength[8]));
  //     }
  //   }
  // }

  return;
}

void Material::printMaterial() {
  // std::string doubleLine(32, '=');
  if (scientific_format) {
    std::cout << std::scientific;
  }
  std::cout << doubleLine80 << std::endl;
  std::cout << std::setw(32) << "MATERIAL" << std::setw(32) << mname
            << std::endl;
  std::cout << std::setw(32) << "Density" << std::setw(32) << mdensity
            << std::endl;
  std::cout << std::setw(32) << "Type" << std::setw(32) << mtype << std::endl;
  std::cout << std::setw(32) << "Failure Criterion" << std::setw(32)
            << mfcriterion << std::endl;
  std::cout << std::setw(32) << "Characteristic Length" << std::setw(32)
            << mcharalength << std::endl;
  std::cout << singleLine80 << std::endl;


  std::cout << std::setw(32) << "Elastic" << std::endl;
  if (mtype == "isotropic") {
    std::cout << std::setw(16) << "E" << std::setw(32) << melastic[0]
              << std::endl;
    std::cout << std::setw(16) << "Nu" << std::setw(32) << melastic[1]
              << std::endl;
  } else if (mtype == "orthotropic") {
    std::cout << std::setw(16) << "E1" << std::setw(32) << melastic[0]
              << std::endl;
    std::cout << std::setw(16) << "E2" << std::setw(32) << melastic[1]
              << std::endl;
    std::cout << std::setw(16) << "E3" << std::setw(32) << melastic[2]
              << std::endl;
    std::cout << std::setw(16) << "G12" << std::setw(32) << melastic[3]
              << std::endl;
    std::cout << std::setw(16) << "G13" << std::setw(32) << melastic[4]
              << std::endl;
    std::cout << std::setw(16) << "G23" << std::setw(32) << melastic[5]
              << std::endl;
    std::cout << std::setw(16) << "Nu12" << std::setw(32) << melastic[6]
              << std::endl;
    std::cout << std::setw(16) << "Nu13" << std::setw(32) << melastic[7]
              << std::endl;
    std::cout << std::setw(16) << "Nu23" << std::setw(32) << melastic[8]
              << std::endl;
  } else if (mtype == "anisotropic") {
    for (std::size_t i = 0; i < elasticLabelAniso.size(); ++i) {
      std::cout << std::setw(16) << upperString(elasticLabelAniso[i])
                << std::setw(32) << melastic[i] << std::endl;
    }
  }


  if (mfcriterion > 0) {
    std::cout << singleLine80 << std::endl;
    std::cout << std::setw(32) << "Strength" << std::endl;

    if (_strength.t1 >= 0) {
      std::cout << std::setw(16) << "Xt (T1)" << std::setw(32) << _strength.t1
                << std::endl;
    }
    if (_strength.t2 >= 0) {
      std::cout << std::setw(16) << "Yt (T2)" << std::setw(32) << _strength.t2
                << std::endl;
    }
    if (_strength.t3 >= 0) {
      std::cout << std::setw(16) << "Zt (T3)" << std::setw(32) << _strength.t3
                << std::endl;
    }
    if (_strength.c1 >= 0) {
      std::cout << std::setw(16) << "Xc (C1)" << std::setw(32) << _strength.c1
                << std::endl;
    }
    if (_strength.c2 >= 0) {
      std::cout << std::setw(16) << "Yc (C2)" << std::setw(32) << _strength.c2
                << std::endl;
    }
    if (_strength.c3 >= 0) {
      std::cout << std::setw(16) << "Zc (C3)" << std::setw(32) << _strength.c3
                << std::endl;
    }
    if (_strength.s12 >= 0) {
      std::cout << std::setw(16) << "S (S12)" << std::setw(32) << _strength.s12
                << std::endl;
    }
    if (_strength.s13 >= 0) {
      std::cout << std::setw(16) << "T (S13)" << std::setw(32) << _strength.s13
                << std::endl;
    }
    if (_strength.s23 >= 0) {
      std::cout << std::setw(16) << "R (S23)" << std::setw(32) << _strength.s23
                << std::endl;
    }


    // if (mtype == "isotropic") {
    //   if (mfcriterion == 1 || mfcriterion == 2) {
    //     std::cout << std::setw(16) << "Xt" << std::setw(32) << mstrength[0]
    //               << std::endl;
    //     std::cout << std::setw(16) << "Xc" << std::setw(32) << mstrength[1]
    //               << std::endl;
    //   } else if (mfcriterion == 3 || mfcriterion == 4) {
    //     std::cout << std::setw(16) << "S" << std::setw(32) << mstrength[0]
    //               << std::endl;
    //   } else if (mfcriterion == 5) {
    //     std::cout << std::setw(16) << "X" << std::setw(32) << mstrength[0]
    //               << std::endl;
    //   }
    // } else if (mtype == "orthotropic" || mtype == "anisotropic") {
    //   if (mfcriterion == 1 || mfcriterion == 2 || mfcriterion == 4) {
    //     std::cout << std::setw(16) << "Xt" << std::setw(32) << mstrength[0]
    //               << std::endl;
    //     std::cout << std::setw(16) << "Yt" << std::setw(32) << mstrength[1]
    //               << std::endl;
    //     std::cout << std::setw(16) << "Zt" << std::setw(32) << mstrength[2]
    //               << std::endl;
    //     std::cout << std::setw(16) << "Xc" << std::setw(32) << mstrength[3]
    //               << std::endl;
    //     std::cout << std::setw(16) << "Yc" << std::setw(32) << mstrength[4]
    //               << std::endl;
    //     std::cout << std::setw(16) << "Zc" << std::setw(32) << mstrength[5]
    //               << std::endl;
    //     std::cout << std::setw(16) << "R" << std::setw(32) << mstrength[6]
    //               << std::endl;
    //     std::cout << std::setw(16) << "T" << std::setw(32) << mstrength[7]
    //               << std::endl;
    //     std::cout << std::setw(16) << "S" << std::setw(32) << mstrength[8]
    //               << std::endl;
    //   } else if (mfcriterion == 3) {
    //     std::cout << std::setw(16) << "X" << std::setw(32) << mstrength[0]
    //               << std::endl;
    //     std::cout << std::setw(16) << "Y" << std::setw(32) << mstrength[1]
    //               << std::endl;
    //     std::cout << std::setw(16) << "Z" << std::setw(32) << mstrength[2]
    //               << std::endl;
    //     std::cout << std::setw(16) << "R" << std::setw(32) << mstrength[3]
    //               << std::endl;
    //     std::cout << std::setw(16) << "T" << std::setw(32) << mstrength[4]
    //               << std::endl;
    //     std::cout << std::setw(16) << "S" << std::setw(32) << mstrength[5]
    //               << std::endl;
    //   } else if (mfcriterion == 5) {
    //     std::cout << std::setw(16) << "Xt" << std::setw(32) << mstrength[0]
    //               << std::endl;
    //     std::cout << std::setw(16) << "Yt" << std::setw(32) << mstrength[1]
    //               << std::endl;
    //     std::cout << std::setw(16) << "Xc" << std::setw(32) << mstrength[3]
    //               << std::endl;
    //     std::cout << std::setw(16) << "Yc" << std::setw(32) << mstrength[4]
    //               << std::endl;
    //     std::cout << std::setw(16) << "R" << std::setw(32) << mstrength[6]
    //               << std::endl;
    //     std::cout << std::setw(16) << "S" << std::setw(32) << mstrength[8]
    //               << std::endl;
    //   }
    // }
  }
}

void Material::setId(int id) { mid = id; }

void Material::setFailureCriterion(int fc) { mfcriterion = fc; }

void Material::setCharacteristicLength(double cl) { mcharalength = cl; }

void Material::setStrength(std::vector<double> strength) {
  mstrength = strength;
}

void Material::completeStrengthProperties() {
  if (_strength.t2 < 0) {
    _strength.t2 = _strength.t1;
  }
  if (_strength.t3 < 0) {
    _strength.t3 = _strength.t2;
  }

  if (_strength.c1 < 0) {
    _strength.c1 = _strength.t1;
  }
  if (_strength.c2 < 0) {
    _strength.c2 = _strength.t2;
  }
  if (_strength.c3 < 0) {
    _strength.c3 = _strength.t3;
  }

  if (_strength.s13 < 0) {
    _strength.s13 = _strength.s12;
  }
  if (_strength.s23 < 0) {
    _strength.s23 = (_strength.t2 + _strength.c2) / 4;
  }
}




void Material::writeStrengthProperties(FILE *file, Message *pmessage) {
  // Strength sp = m->getStrength();
  std::string type = _strength._type;
  // int fc = m->getFailureCriterion();
  // std::cout << "type = " << type << std::endl;

  completeStrengthProperties();
  if (type == "lamina") {
    type = "orthotropic";
  }

  fprintf(file, "%8d", mfcriterion);

  if (type == "isotropic") {
    if ((mfcriterion == 1) || (mfcriterion == 2)) {
      fprintf(file, "%8d\n", 2);
      if (config.analysis_tool == 2) fprintf(file, "%16e\n", mcharalength);
      fprintf(file, "%16e%16e\n", _strength.t1, _strength.c1);
    }
    else if ((mfcriterion == 3) || (mfcriterion == 4)) {
      fprintf(file, "%8d\n", 1);
      if (config.analysis_tool == 2) fprintf(file, "%16e\n", mcharalength);
      fprintf(file, "%16e\n", _strength.s12);
    }
    else if (mfcriterion == 5) {
      fprintf(file, "%8d\n", 1);
      if (config.analysis_tool == 2) fprintf(file, "%16e\n", mcharalength);
      fprintf(file, "%16e\n", _strength.t1);
    }
  }
  else if ((type == "orthotropic") || (type == "anisotropic")) {
    if ((mfcriterion == 1) || (mfcriterion == 2) || (mfcriterion == 4)) {
      fprintf(file, "%8d\n", 9);
      if (config.analysis_tool == 2) fprintf(file, "%16e\n", mcharalength);
      fprintf(file,
        "%16e%16e%16e%16e%16e%16e%16e%16e%16e\n",
        _strength.t1, _strength.t2, _strength.t3, _strength.c1, _strength.c2, _strength.c3, _strength.s23, _strength.s13, _strength.s12);
    }
    else if (mfcriterion == 3) {
      fprintf(file, "%8d\n", 6);
      if (config.analysis_tool == 2) fprintf(file, "%16e\n", mcharalength);
      fprintf(file,
        "%16e%16e%16e%16e%16e%16e\n",
        _strength.t1, _strength.t2, _strength.t3, _strength.s23, _strength.s13, _strength.s12);
    }
    else if (mfcriterion == 5) {
      fprintf(file, "%8d\n", 6);
      if (config.analysis_tool == 2) fprintf(file, "%16e\n", mcharalength);
      fprintf(file,
        "%16e%16e%16e%16e%16e%16e\n",
        _strength.t1, _strength.t2, _strength.c1, _strength.c2, _strength.s23, _strength.s12);
    }
  }
  fprintf(file, "\n");

  return;
}



















// ===================================================================
void Lamina::printLamina() {
  // std::string doubleLine(32, '=');
  std::cout << doubleLine << std::endl;
  std::cout << "    LAMINA: " << lname << std::endl;
  std::cout << "  Material: " << p_lmaterial->getName() << std::endl;
  std::cout << " Thickness: " << lthickness << std::endl;
  std::cout << doubleLine << std::endl;
}

void Lamina::setThickness(double thickness) { lthickness = thickness; }



















// ===================================================================
void LayerType::setId(int id) { lid = id; }
void LayerType::setMaterial(Material *p_material) { p_lmaterial = p_material; }
void LayerType::setAngle(double angle) { langle = angle; }



















// ===================================================================
// void Layer::print(int i_number, Message *pmessage, int i_type, int i_indent) {
//   std::stringstream ss;
//   ss << std::setw(4) <<
//   return;
// }

void Layer::printLayer(int number) {
  // std::string line(32, '=');
  std::cout << singleLine << std::endl;
  std::cout << "  LAYER: " << number << std::endl;
  std::cout << " Lamina: " << p_llamina->getName() << std::endl;
  std::cout << "  Angle: " << langle << std::endl;
  std::cout << "  Stack: " << lstack << std::endl;
  // std::cout << doubleLine << std::endl;
}

void Layer::setAngle(double angle) { langle = angle; }

void Layer::setStack(int stack) { lstack = stack; }

void Layer::setLayerType(LayerType *p_layertype) { p_llayertype = p_layertype; }

// ===================================================================
// void Ply::printPly(int number) {
//   std::cout << singleLine << std::endl;
//   std::cout << "       PLY: " << number << std::endl;
//   std::cout << "  Material: " << p_pmaterial->getName() << std::endl;
//   std::cout << " Thickness: " << pthickness << std::endl;
//   std::cout << "     Angle: " << pangle << std::endl;
// }



















// ===================================================================
void Layup::print(Message *pmessage, int i_type, int i_indent) {
  std::string msg;
  pmessage->print(i_type, "name: " + lname);
  pmessage->print(i_type, "layers:");
  std::stringstream ss;
  ss << std::setw(4) << "no." << std::setw(32) << "material"
     << std::setw(16) << "thickness"
     << std::setw(8) << "angle"
     << std::setw(8) << "plies";
  pmessage->print(i_type, ss.str());
  for (int i = 0; i < llayers.size(); i++) {
    std::stringstream ss;
    ss << std::setw(4) << (i+1) << llayers[i];
    pmessage->print(i_type, ss.str());
  }

  return;
}

void Layup::printLayup() {
  // std::string line(32, '=');
  std::cout << std::endl;
  std::cout << doubleLine80 << std::endl;
  std::cout << std::setw(16) << "LAYUP" << std::setw(16) << lname << std::endl;
  std::cout << std::setw(16) << "Thickness" << std::setw(16) << lthickness
            << std::endl;
  std::cout << std::setw(16) << "Plies" << std::setw(16) << lplies.size()
            << std::endl;
  std::cout << std::setw(16) << "Name" << std::setw(16) << "Thickness"
            << std::setw(16) << "Angle" << std::endl;
  std::cout << singleLine80 << std::endl;
  for (auto ply : lplies)
    std::cout << ply << std::endl;
  //   std::cout << doubleLine << std::endl;
  std::cout << singleLine80 << std::endl;
  std::cout << std::setw(16) << "Layers" << std::setw(16) << llayers.size()
            << std::endl;
  std::cout << std::setw(16) << "Name" << std::setw(16) << "Thickness"
            << std::setw(16) << "Angle" << std::setw(16) << "Stack"
            << std::endl;
  std::cout << singleLine80 << std::endl;
  for (auto layer : llayers)
    std::cout << layer << std::endl;
}

double Layup::getTotalThickness() const {
  double tt = 0;
  for (auto l : llayers) {
    tt += l.getStack() * l.getLamina()->getThickness();
  }
  return tt;
}

void Layup::addLayer(Lamina *p_lamina, double angle, int stack) {
  bool newlayer{false};
  if (llayers.size() == 0) {
    newlayer = true;
  } else {
    Layer *layer{&llayers.back()};
    if ((p_lamina == layer->getLamina()) && (angle == layer->getAngle())) {
      // The new layer is the same as the previous one
      layer->setStack(layer->getStack() + stack);
    } else {
      newlayer = true;
    }
  }

  if (newlayer)
    llayers.push_back(Layer{p_lamina, angle, stack});
}

void Layup::addLayer(Lamina *p_lamina, double angle, int stack,
                     LayerType *p_layertype) {
  bool newlayer{false};
  if (llayers.size() == 0) {
    newlayer = true;
  } else {
    Layer *layer{&llayers.back()};
    if ((p_lamina == layer->getLamina()) && (angle == layer->getAngle())) {
      // The new layer is the same as the previous one
      layer->setStack(layer->getStack() + stack);
    } else {
      newlayer = true;
    }
  }

  if (newlayer)
    llayers.push_back(Layer{p_lamina, angle, stack, p_layertype});
}

void Layup::addPly(Material *p_material, double thickness, double angle) {
  lplies.push_back(Ply{p_material, thickness, angle});
  lthickness += thickness;
}

int combineLayups(Layup *combined, std::vector<Layup *> layups) {
  for (auto lyp : layups) {
    for (auto lyr : lyp->getLayers()) {
      combined->addLayer(lyr);
    }
    for (auto ply : lyp->getPlies()) {
      combined->addPly(ply);
    }
  }
  return 0;
}
