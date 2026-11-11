#include "jrte_cabac_decoder.h"
#include "jrte_cabac_tables.h"
#include "jrte_cabac_context_tables.h"

#include <limits>

namespace jrte {

bool CabacDecoder::initialize(const uint8_t* data, size_t size)
{
    data_ = data;
    size_ = size;
    bit_position_ = 0;

    range_ = 510;
    offset_ = 0;

    initialized_ = false;
    good_ = false;
    contexts_.clear();
    error_.clear();

    if (data == nullptr || size == 0) {
        error_ = "Flux CABAC vide ou pointeur nul.";
        return false;
    }

    if (size > std::numeric_limits<size_t>::max() / 8) {
        error_ = "Taille du flux CABAC invalide.";
        return false;
    }

    // Initialisation de codIOffset avec les 9 premiers bits.
    for (int i = 0; i < 9; ++i) {
        uint32_t bit = 0;

        if (!read_bit(bit)) {
            error_ = "Flux trop court pour initialiser codIOffset.";
            return false;
        }

        offset_ = (offset_ << 1) | bit;
    }

    if (offset_ >= range_) {
        error_ = "Valeur initiale codIOffset invalide.";
        return false;
    }

    initialized_ = true;
    good_ = true;
    return true;
}

bool CabacDecoder::read_bit(uint32_t& bit)
{
    if (data_ == nullptr || bit_position_ >= size_ * 8) {
        error_ = "Fin du flux CABAC.";
        good_ = false;
        return false;
    }

    const size_t byte_index = bit_position_ / 8;
    const size_t bit_index = 7 - (bit_position_ % 8);

    bit = (data_[byte_index] >> bit_index) & 1u;
    ++bit_position_;

    return true;
}

bool CabacDecoder::renormalize()
{
    while (range_ < 256) {
        uint32_t bit = 0;

        if (!read_bit(bit)) {
            return false;
        }

        range_ <<= 1;
        offset_ = (offset_ << 1) | bit;
    }

    return true;
}

bool CabacDecoder::decode_bypass(uint32_t& bin)
{
    if (!initialized_ || !good_) {
        error_ = "Décodeur CABAC non initialisé ou en erreur.";
        return false;
    }

    uint32_t bit = 0;

    if (!read_bit(bit)) {
        return false;
    }

    offset_ = (offset_ << 1) | bit;

    if (offset_ >= range_) {
        offset_ -= range_;
        bin = 1;
    } else {
        bin = 0;
    }

    return true;
}

bool CabacDecoder::decode_terminate(uint32_t& bin)
{
    if (!initialized_ || !good_) {
        error_ = "Décodeur CABAC non initialisé ou en erreur.";
        return false;
    }

    if (range_ < 2) {
        error_ = "codIRange invalide pendant decode_terminate.";
        good_ = false;
        return false;
    }

    range_ -= 2;

    if (offset_ >= range_) {
        bin = 1;
        return true;
    }

    bin = 0;
    return renormalize();
}

bool CabacDecoder::decode_decision(CabacContext& context, uint32_t& bin)
{
    if (!initialized_ || !good_) {
        error_ = "Décodeur CABAC non initialisé ou en erreur.";
        return false;
    }

    if (context.state_idx > 63 || context.val_mps > 1) {
        error_ = "État de contexte CABAC invalide.";
        good_ = false;
        return false;
    }

    const uint32_t range_index = (range_ >> 6) & 3u;

    const uint32_t range_lps =
        cabac_tables::RANGE_LPS[context.state_idx][range_index];

    if (range_lps == 0 || range_lps >= range_) {
        error_ = "Valeur rangeLPS invalide.";
        good_ = false;
        return false;
    }

    range_ -= range_lps;

    if (offset_ >= range_) {
        // Décodage d'un LPS.
        bin = 1u - context.val_mps;
        offset_ -= range_;
        range_ = range_lps;

        if (context.state_idx == 0) {
            context.val_mps ^= 1u;
        }

        context.state_idx =
            cabac_tables::STATE_TRANS[context.state_idx][0];
    } else {
        // Décodage d'un MPS.
        bin = context.val_mps;

        context.state_idx =
            cabac_tables::STATE_TRANS[context.state_idx][1];
    }

    return renormalize();
}

bool CabacDecoder::initialize_contexts(
    bool intra_slice,
    int cabac_init_idc,
    int slice_qpy,
    size_t context_count)
{
    (void)intra_slice;
    (void)cabac_init_idc;
    (void)slice_qpy;
    (void)context_count;

    error_ =
        "Initialisation des contextes H.264 non integree : "
        "les tables normatives m et n sont encore necessaires.";

    return false;
}

uint32_t CabacDecoder::range() const noexcept
{
    return range_;
}

uint32_t CabacDecoder::offset() const noexcept
{
    return offset_;
}

bool CabacDecoder::good() const noexcept
{
    return good_;
}

const std::string& CabacDecoder::error() const noexcept
{
    return error_;
}

const std::vector<CabacContext>& CabacDecoder::contexts() const noexcept
{
    return contexts_;
}

} // namespace jrte
