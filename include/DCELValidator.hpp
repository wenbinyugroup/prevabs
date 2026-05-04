#pragma once

struct BuilderConfig;
class PDCEL;

bool validateDCEL(const PDCEL &dcel);
void fixDCELGeometry(PDCEL &dcel, const BuilderConfig &bcfg);
