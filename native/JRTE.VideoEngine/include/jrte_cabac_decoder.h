#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace jrte {

struct CabacContext {
    uint8_t state_idx = 0;
    uint8_t val_mps = 0;
};

class CabacDecoder {
public:
    CabacDecoder() = default;

    bool initialize(const uint8_t* data, size_t size);

    bool decode_bypass(uint32_t& bin);
    bool decode_terminate(uint32_t& bin);

    // Décodage d'une décision avec un contexte CABAC.
    bool decode_decision(CabacContext& context, uint32_t& bin);

    // Initialise les contextes à partir des paramètres H.264.
    bool initialize_contexts(
        bool intra_slice,
        int cabac_init_idc,
        int slice_qpy,
        size_t context_count
    );

    uint32_t range() const noexcept;
    uint32_t offset() const noexcept;
    bool good() const noexcept;
    const std::string& error() const noexcept;

    const std::vector<CabacContext>& contexts() const noexcept;

private:
    bool read_bit(uint32_t& bit);
    bool renormalize();

    const uint8_t* data_ = nullptr;
    size_t size_ = 0;
    size_t bit_position_ = 0;

    uint32_t range_ = 510;
    uint32_t offset_ = 0;

    bool initialized_ = false;
    bool good_ = false;

    std::vector<CabacContext> contexts_;
    std::string error_;
};

} // namespace jrte
