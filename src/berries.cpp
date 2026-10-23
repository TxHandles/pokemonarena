#include "berries.h"
#include "berries.inc"

int BerryCount() { return (int)(sizeof(kBerries) / sizeof(kBerries[0])); }
const BerryInfo& BerryAt(int i) { return kBerries[i < 0 ? 0 : i >= BerryCount() ? BerryCount() - 1 : i]; }
int BerryIndexOf(const std::string& name) {
    std::string n = name.size() > 6 && name.compare(name.size() - 6, 6, " Berry") == 0 ? name : name + " Berry";
    for (int i = 0; i < BerryCount(); i++) if (n == kBerries[i].name) return i;
    return -1;
}
const char* FlavorName(int k) {
    static const char* n[5] = {"Spicy", "Dry", "Sweet", "Bitter", "Sour"};
    return k >= 0 && k < 5 ? n[k] : "";
}
