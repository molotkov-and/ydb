#pragma once

#include "token_manager.h"
#include "token_provider_settings.h"

namespace NKikimrYndx {

class TTokenManager : public NKikimr::TTokenManager {
    using TBase = NKikimr::TTokenManager;

public:
    TTokenManager(const TBase::TInitializer& initializer);

private:
    NKikimr::NTokenManager::TTokenProviderSettings TokenAgentProviderSettings;

protected:
    void BootstrapTokenProviders() override;
};

NActors::IActor* CreateTokenManager(const NKikimrProto::TTokenManager& config, const NActors::TActorId& HttpProxyId);

} // NKikimrYndx
