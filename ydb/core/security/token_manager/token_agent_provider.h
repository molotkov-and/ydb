#pragma once
#include <ydb/library/actors/core/actorid.h>
#include <ydb/core/protos/auth.pb.h>

#include "token_provider.h"

namespace NKikimr::NTokenManager {

struct TTokenProviderSettings;

} // NKikimr::NTokenManager

namespace NKikimrYndx {

struct TTokenAgentProvider : public NKikimr::NTokenManager::TTokenProvider {
    using TBase = NKikimr::NTokenManager::TTokenProvider;

    const NActors::TActorId TokenAgentId;
    const NKikimrProto::TTokenManager::TTokenAgentProvider::TTokenAgentInfo& ProviderInfo;

    TTokenAgentProvider(const NActors::TActorId& tokenManagerId,
                        const NKikimr::NTokenManager::TTokenProviderSettings& settings,
                        const NActors::TActorId tokenAgentId,
                        const NKikimrProto::TTokenManager::TTokenAgentProvider::TTokenAgentInfo& providerInfo);

    NActors::IActor* CreateTokenProviderHandler() override;
    TString GetId() const override;
};

} // NKikimrYndx
