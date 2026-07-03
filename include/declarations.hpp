#pragma once

#include "PGeoClasses.hpp"
#include "PGeoLine.hpp"

#include "PBST.hpp"
#include "dcel/PDCEL.hpp"
#include "dcel/PDCELVertex.hpp"
#include "dcel/PDCELHalfEdge.hpp"
#include "dcel/PDCELHalfEdgeLoop.hpp"
#include "dcel/PDCELFace.hpp"

#include "Material.hpp"
#include "PBaseLine.hpp"
#include "CrossSection.hpp"

#include "PSegment.hpp"
#include "PArea.hpp"
#include "PComponent.hpp"
#include "PModel.hpp"

#include "PMeshClasses.hpp"
#include "PDataClasses.hpp"

#include "utilities.hpp"

class Material;
class Lamina;
class LayerType;
class Layer;
class Ply;
class Layup;

class Baseline;
class CrossSection;
class Segment;
class Filling;
class PArea;
class PComponent;
class PModel;

class PGeoPoint3;
class PGeoLineSegment;
class PGeoArc;

namespace dcel { class PDCEL; }
namespace dcel { class PDCELVertex; }
namespace dcel { class PDCELHalfEdge; }
namespace dcel { class PDCELHalfEdgeLoop; }
namespace dcel { class PDCELFace; }
class PAVLTreeVertex;

class PMesh;
class PElementNodeData;

class Message;
