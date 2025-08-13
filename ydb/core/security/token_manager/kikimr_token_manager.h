#pragma once

#include "token_manager.h"
#include "token_provider_settings.h"

namespace NKikimr {

struct TTokenManagerSettings;

} // NKikimr
namespace NKikimrYndx {

class TTokenManager : public NKikimr::TTokenManager {
    using TBase = NKikimr::TTokenManager;

public:
    TTokenManager(const NKikimr::TTokenManagerSettings& settings);

private:
    NKikimr::NTokenManager::TTokenProviderSettings TokenAgentProviderSettings;

protected:
    void BootstrapTokenProviders() override;
};

NActors::IActor* CreateTokenManager(const NKikimr::TTokenManagerSettings& settings);

} // NKikimrYndx
