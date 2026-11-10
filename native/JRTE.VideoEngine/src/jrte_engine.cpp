
#include "jrte_api.h"

#include <atomic>

namespace
{
    std::atomic<bool> initialized{false};

    constexpr uint32_t API_VERSION = 1;
    constexpr const char* ENGINE_NAME = "JRTE Native Video Engine";
}

extern "C"
{
    uint32_t jrte_get_api_version()
    {
        return API_VERSION;
    }

    const char* jrte_get_engine_name()
    {
        return ENGINE_NAME;
    }

    int32_t jrte_initialize()
    {
        bool expected = false;

        if (initialized.compare_exchange_strong(expected, true))
            return 0;

        return 1;
    }

    void jrte_shutdown()
    {
        initialized.store(false);
    }
}
