#include "jrte_block_coeff_context.h"

#include <iostream>
#include <string>

using namespace jrte::h264;

static int failures = 0;

void expect(bool condition, const char* name)
{
    if (condition)
        std::cout << "[PASS] " << name << '\n';
    else
    {
        std::cout << "[FAIL] " << name << '\n';
        ++failures;
    }
}

int main()
{
    std::cout << "=== JRTE BLOCK COEFF CONTEXT TEST ===\n";

    BlockCoeffContext context;
    std::string error;

    expect(context.reset(4, 4, error), "Initialisation 4x4");

    expect(
        context.record_block(0, 1, 4, error),
        "Enregistrement voisin gauche"
    );

    expect(
        context.record_block(1, 0, 6, error),
        "Enregistrement voisin haut"
    );

    NeighborCoeffInfo neighbors;

    expect(
        context.get_neighbor_info(1, 1, neighbors, error),
        "Lecture des voisins"
    );

    expect(
        neighbors.left_available &&
        neighbors.top_available &&
        neighbors.left_total_coeff == 4 &&
        neighbors.top_total_coeff == 6,
        "Valeurs gauche et haut"
    );

    int nc = -1;

    expect(
        derive_nc(neighbors, nc, error) && nc == 5,
        "Calcul nC depuis le contexte"
    );

    expect(
        context.get_neighbor_info(0, 0, neighbors, error) &&
        !neighbors.left_available &&
        !neighbors.top_available,
        "Limites superieures et gauches"
    );

    expect(
        context.get_neighbor_info(2, 2, neighbors, error) &&
        !neighbors.left_available &&
        !neighbors.top_available,
        "Voisins non encore decodes indisponibles"
    );

    expect(
        !context.record_block(2, 2, 17, error),
        "Rejet TotalCoeff superieur a 16"
    );

    expect(
        !context.record_block(9, 0, 2, error),
        "Rejet coordonnee invalide"
    );

    expect(
        !context.reset(0, 4, error),
        "Rejet dimension nulle"
    );

    std::cout << "\nEchecs : " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
