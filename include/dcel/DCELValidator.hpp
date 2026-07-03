#pragma once

namespace dcel {
class PDCEL;

bool validateDCEL(const PDCEL &dcel);
void fixDCELGeometry(PDCEL &dcel, double geo_tol);
}  // namespace dcel
