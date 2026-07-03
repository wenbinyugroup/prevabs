#pragma once

struct BuilderConfig;

namespace dcel {
class PDCEL;

bool validateDCEL(const PDCEL &dcel);
void fixDCELGeometry(PDCEL &dcel, const BuilderConfig &bcfg);
}  // namespace dcel
