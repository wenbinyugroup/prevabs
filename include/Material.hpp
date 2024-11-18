#pragma once

#include "declarations.hpp"
#include "utilities.hpp"

#include <string>
#include <vector>
#include <cstdio>

class Elastic {
private:
  double _e1;
  double _e2;
  double _e3;
  double _nu12;
  double _nu13;
  double _nu23;
  double _g12;
  double _g13;
  double _g23;
  std::vector<std::vector<double>> _c;
public:
  Elastic() = default;
};

struct Strength {
  std::string _type;
  double t1{-1};  // tensile strength in x1
  double t2{-1};  // tensile strength in x2
  double t3{-1};  // tensile strength in x3
  double c1{-1};  // compresive strength in x1
  double c2{-1};  // compresive strength in x2
  double c3{-1};  // compresive strength in x3
  double s12{-1};  // shear strength in x1-x2
  double s13{-1};  // shear strength in x1-x3
  double s23{-1};  // shear strength in x2-x3
};


/** @ingroup cs
 * A material class.
 */
class Material {
private:
  int mid;
  std::string _name;

  std::string _type; // isotropic, orthotropic, anisotropic

  double _density{1.0};
  std::vector<double> _elastic;

  double _specific_heat{0.0};
  std::vector<double> _cte;

  int mfcriterion{0};
  double mcharalength{0.0};
  std::vector<double> mstrength;

  Strength _strength;

public:
  Material() {
    mid = 0;
    mstrength = {};
  }
  Material(std::string name) : _name(name) {
    mid = 0;
    mstrength = {};
  }
  Material(std::string name, std::string type, double density,
           std::vector<double> elastic)
      : _name(name), _type(type), _density(density), _elastic(elastic) {
    mid = 0;
    mstrength = {};
  }
  Material(int id, std::string name, std::string type, double density,
           std::vector<double> elastic)
      : mid(id), _name(name), _type(type), _density(density),
        _elastic(elastic) {
    mstrength = {};
  }
  Material(int id, std::string name, std::string type, double density,
           std::vector<double> elastic, int fcriterion, double charalength,
           std::vector<double> strength)
      : mid(id), _name(name), _type(type), _density(density), _elastic(elastic),
        mfcriterion(fcriterion), mcharalength(charalength),
        mstrength(strength) {}

  void print(Message *, int, int = 0);
  void printMaterial(); // Print details

  int id() { return mid; }
  std::string getName() { return _name; }
  std::string getType() { return _type; }
  double getDensity() const { return _density; }
  std::vector<double> getElastic() { return _elastic; }
  std::vector<double> getCte() { return _cte; }
  double getSpecificHeat() const { return _specific_heat; }
  int getFailureCriterion() const { return mfcriterion; }
  double getCharacteristicLength() const { return mcharalength; }
  // std::vector<double> getStrength() { return mstrength; }
  Strength getStrength() const { return _strength; }

  void setId(int);
  void setName(std::string name) { _name = name; }
  void setType(std::string type) { _type = type; }
  void setDensity(double density) { _density = density; }
  void setElastic(std::vector<double> elastic) { _elastic = elastic; }
  void setCte(std::vector<double> cte) { _cte = cte; }
  void setSpecificHeat(double sh) { _specific_heat = sh; }
  void setFailureCriterion(int);
  void setCharacteristicLength(double);
  void setStrength(std::vector<double>);
  void setStrength(Strength strength) { _strength = strength; }

  void completeStrengthProperties();

  void writeStrengthProperties(FILE *, Message *);
};










// ===================================================================
/** @ingroup cs
 * A lamina class.
 */
class Lamina {
private:
  std::string lname;
  Material *p_lmaterial;
  double lthickness;

public:
  // Lamina();
  Lamina(std::string name, Material *p_material, double thickness)
      : lname(name), p_lmaterial(p_material), lthickness(thickness) {}

  void printLamina();

  std::string getName() { return lname; }
  Material *getMaterial() { return p_lmaterial; }
  double getThickness() { return lthickness; }

  void setMaterial(Material *p_material) { p_lmaterial = p_material; }
  void setThickness(double);
};










// ===================================================================
/** @ingroup cs
 * A layer type class.
 */
class LayerType {
private:
  int lid;
  Material *p_lmaterial;
  double langle;

public:
  LayerType() {}
  LayerType(int id, Material *p_material, double angle)
      : lid(id), p_lmaterial(p_material), langle(angle) {}

  friend std::ostream &operator<<(std::ostream &, LayerType *);

  int id() { return lid; }
  Material *material() { return p_lmaterial; }
  double angle() { return langle; }

  void setId(int);
  void setMaterial(Material *);
  void setAngle(double);
};










// ===================================================================
/** @ingroup cs
 * A layer class.
 */
class Layer {
private:
  Lamina *p_llamina;
  double langle;
  int lstack;
  LayerType *p_llayertype;

public:
  Layer() {}
  Layer(Lamina *p_lamina, double angle, int stack)
      : p_llamina(p_lamina), langle(angle), lstack(stack) {}
  Layer(Lamina *p_lamina, double angle, int stack, LayerType *p_layertype)
      : p_llamina(p_lamina), langle(angle), lstack(stack),
        p_llayertype(p_layertype) {}

  void print(int, Message *, int, int = 0);
  void printLayer(int);

  Lamina *getLamina() { return p_llamina; }
  double getAngle() { return langle; }
  int getStack() { return lstack; }
  LayerType *getLayerType() { return p_llayertype; }

  void setAngle(double);
  void setStack(int);
  void setLayerType(LayerType *);
};










// ===================================================================
/** @ingroup cs
 * A ply class.
 */
class Ply {
private:
  Material *p_pmaterial;
  double pthickness; // Lamina thickness
  double pangle;

public:
  Ply(Material *p_material, double thickness, double angle)
      : p_pmaterial(p_material), pthickness(thickness), pangle(angle) {}

  //   void printPly(int);

  Material *getMaterial() { return p_pmaterial; }
  double getThickness() { return pthickness; }
  double getAngle() { return pangle; }
};










// ===================================================================
/** @ingroup cs
 * A layup class.
 */
class Layup {
private:
  std::string lname;
  std::vector<Layer> llayers;
  std::vector<Ply> lplies;
  // int lnumplies;
  double lthickness;

public:
  Layup() = default;
  Layup(std::string name) : lname(name) {}
  Layup(std::string name, std::vector<Layer> layers, std::vector<Ply> plies,
        double thickness = 0.0)
      : lname(name), llayers(layers), lplies(plies), lthickness(thickness) {}

  void print(Message *, int, int = 0);
  void printLayup();

  std::string getName() const { return lname; }
  std::vector<Layer> getLayers() const { return llayers; }
  std::vector<Ply> getPlies() const { return lplies; }
  // int getNumberOfPlies() { return lnumplies; }
  double getThickness() const { return lthickness; }

  double getTotalThickness() const;

  void setName(std::string name) { lname = name; }

  void addLayer(Layer l) { llayers.push_back(l); }
  void addLayer(Lamina *, double, int);
  void addLayer(Lamina *, double, int, LayerType *);
  void addPly(Ply p) { lplies.push_back(p); }
  void addPly(Material *, double, double);
};

int combineLayups(Layup *, std::vector<Layup *>);
