#include "token_agent_provider.h"
#include "token_agent_handler.h"

namespace NKikimrYndx {

TTokenAgentProvider::TTokenAgentProvider(const NActors::TActorId& tokenManagerId,
                        const NKikimr::NTokenManager::TTokenProviderSettings& settings,
                        const NActors::TActorId tokenAgentId,
                        const NKikimrProto::TTokenManager::TTokenAgentProvider::TTokenAgentInfo& providerInfo)
    : TBase(tokenManagerId, settings)
    , TokenAgentId(tokenAgentId)
    , ProviderInfo(providerInfo)
{}

NActors::IActor* TTokenAgentProvider::CreateTokenProviderHandler() {
    return new TTokenAgentProviderHandler(TokenManagerId, TokenAgentId, ProviderInfo, Settings);
}

TString TTokenAgentProvider::GetId() const {
    return ProviderInfo.GetId();
}

} // NKikimrYndx
