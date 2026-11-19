#include "jrte_macroblock_decoder.h"

#include <iostream>
#include <string>

using namespace jrte::h264;

static int failures = 0;

void check(
    const char* name,
    const NeighborCoeffInfo& neighbors,
    int expectedNc,
    bool expectedSuccess
)
{
    int nc = -1;
    std::string error;

    const bool success = derive_nc(neighbors, nc, error);

    const bool passed =
        success == expectedSuccess &&
        (!expectedSuccess || nc == expectedNc);

    if (passed)
    {
        std::cout << "[PASS] " << name << '\n';
    }
    else
    {
        std::cout << "[FAIL] " << name
                  << " | nc=" << nc
                  << " | success=" << success
                  << " | error=" << error << '\n';
        ++failures;
    }
}

int main()
{
    std::cout << "=== JRTE MACROBLOCK DECODER TEST ===\n";

    NeighborCoeffInfo both;
    both.left_available = true;
    both.top_available = true;
    both.left_total_coeff = 4;
    both.top_total_coeff = 6;
    check("Moyenne de deux voisins", both, 5, true);

    NeighborCoeffInfo left;
    left.left_available = true;
    left.left_total_coeff = 7;
    check("Voisin gauche seulement", left, 7, true);

    NeighborCoeffInfo top;
    top.top_available = true;
    top.top_total_coeff = 5;
    check("Voisin haut seulement", top, 5, true);

    NeighborCoeffInfo none;
    check("Aucun voisin disponible", none, 0, true);

    NeighborCoeffInfo invalidLeft;
    invalidLeft.left_available = true;
    invalidLeft.left_total_coeff = 17;
    check("Valeur gauche invalide", invalidLeft, 0, false);

    NeighborCoeffInfo invalidTop;
    invalidTop.top_available = true;
    invalidTop.top_total_coeff = 17;
    check("Valeur haute invalide", invalidTop, 0, false);

    std::cout << "\nEchecs : " << failures << '\n';

    return failures == 0 ? 0 : 1;
}
