#include "jrte_block_coeff_context.h"

#include <limits>

namespace jrte::h264
{
    bool BlockCoeffContext::reset(
        uint32_t width,
        uint32_t height,
        std::string& error)
    {
        error.clear();

        width_ = 0;
        height_ = 0;
        total_coeff_.clear();
        decoded_.clear();

        if (width == 0 || height == 0)
        {
            error = "Dimensions du contexte invalides.";
            return false;
        }

        const size_t w = static_cast<size_t>(width);
        const size_t h = static_cast<size_t>(height);

        if (h > std::numeric_limits<size_t>::max() / w)
        {
            error = "Dimensions du contexte trop grandes.";
            return false;
        }

        const size_t count = w * h;

        total_coeff_.assign(count, 0);
        decoded_.assign(count, 0);

        width_ = width;
        height_ = height;

        return true;
    }

    size_t BlockCoeffContext::index(
        uint32_t x,
        uint32_t y) const
    {
        return static_cast<size_t>(y) *
            static_cast<size_t>(width_) +
            static_cast<size_t>(x);
    }

    bool BlockCoeffContext::record_block(
        uint32_t x,
        uint32_t y,
        uint8_t total_coeff,
        std::string& error)
    {
        error.clear();

        if (width_ == 0 || height_ == 0)
        {
            error = "Contexte non initialise.";
            return false;
        }

        if (x >= width_ || y >= height_)
        {
            error = "Coordonnees du bloc hors limites.";
            return false;
        }

        if (total_coeff > 16)
        {
            error = "TotalCoeff doit etre compris entre 0 et 16.";
            return false;
        }

        const size_t position = index(x, y);

        total_coeff_[position] = total_coeff;
        decoded_[position] = 1;

        return true;
    }

    bool BlockCoeffContext::get_neighbor_info(
        uint32_t x,
        uint32_t y,
        NeighborCoeffInfo& neighbors,
        std::string& error) const
    {
        error.clear();
        neighbors = {};

        if (width_ == 0 || height_ == 0)
        {
            error = "Contexte non initialise.";
            return false;
        }

        if (x >= width_ || y >= height_)
        {
            error = "Coordonnees du bloc hors limites.";
            return false;
        }

        if (x > 0)
        {
            const size_t left = index(x - 1, y);

            if (decoded_[left])
            {
                neighbors.left_available = true;
                neighbors.left_total_coeff = total_coeff_[left];
            }
        }

        if (y > 0)
        {
            const size_t top = index(x, y - 1);

            if (decoded_[top])
            {
                neighbors.top_available = true;
                neighbors.top_total_coeff = total_coeff_[top];
            }
        }

        return true;
    }

    bool BlockCoeffContext::decode_block(
        uint32_t x,
        uint32_t y,
        BitReader& reader,
        std::array<int32_t, 16>& coefficients,
        std::string& error)
    {
        error.clear();
        coefficients.fill(0);

        if (width_ == 0 || height_ == 0)
        {
            error = "Contexte non initialise.";
            return false;
        }

        if (x >= width_ || y >= height_)
        {
            error = "Coordonnees du bloc hors limites.";
            return false;
        }

        if (decoded_[index(x, y)])
        {
            error = "Ce bloc a deja ete decode.";
            return false;
        }

        NeighborCoeffInfo neighbors;

        if (!get_neighbor_info(x, y, neighbors, error))
            return false;

        uint8_t total_coeff = 0;

        if (!decode_macroblock_residual_block(
                reader,
                neighbors,
                coefficients,
                total_coeff,
                error))
        {
            coefficients.fill(0);
            return false;
        }

        if (!record_block(x, y, total_coeff, error))
        {
            coefficients.fill(0);
            return false;
        }

        return true;
    }
    uint32_t BlockCoeffContext::width() const
    {
        return width_;
    }

    uint32_t BlockCoeffContext::height() const
    {
        return height_;
    }
}


