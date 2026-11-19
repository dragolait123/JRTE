#include "jrte_macroblock_decoder.h"
#include "jrte_cavlc_decoder.h"

namespace jrte::h264
{
    bool derive_nc(
        const NeighborCoeffInfo& neighbors,
        int& nc,
        std::string& error
    )
    {
        error.clear();
        nc = 0;

        if (neighbors.left_available &&
            neighbors.left_total_coeff > 16)
        {
            error = "Nombre de coefficients gauche invalide.";
            return false;
        }

        if (neighbors.top_available &&
            neighbors.top_total_coeff > 16)
        {
            error = "Nombre de coefficients haut invalide.";
            return false;
        }

        if (neighbors.left_available && neighbors.top_available)
        {
            nc = (
                static_cast<int>(neighbors.left_total_coeff) +
                static_cast<int>(neighbors.top_total_coeff) +
                1
            ) / 2;
        }
        else if (neighbors.left_available)
        {
            nc = neighbors.left_total_coeff;
        }
        else if (neighbors.top_available)
        {
            nc = neighbors.top_total_coeff;
        }

        return true;
    }

    bool decode_macroblock_residual_block(
        BitReader& reader,
        const NeighborCoeffInfo& neighbors,
        std::array<int32_t, 16>& coefficients,
        uint8_t& total_coeff,
        std::string& error
    )
    {
        coefficients.fill(0);
        total_coeff = 0;
        error.clear();

        int nc = 0;

        if (!derive_nc(neighbors, nc, error))
        {
            return false;
        }

        if (!decode_residual_block_4x4(
                reader,
                nc,
                coefficients,
                error))
        {
            return false;
        }

        for (const int32_t coefficient : coefficients)
        {
            if (coefficient != 0)
            {
                ++total_coeff;
            }
        }

        return true;
    }
}
