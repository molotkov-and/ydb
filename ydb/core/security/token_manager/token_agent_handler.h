#pragma once

#include <ydb/library/actors/core/actor_bootstrapped.h>
#include <ydb/core/protos/auth.pb.h>

namespace NKikimr::NTokenManager {

struct TTokenProviderSettings;

} // NKikimr::NTokenManager

namespace NCloud::NEvTokenAgent {

struct TEvGetTokenResponse;
using TEvGetTokenResponse_HandlePtr = TAutoPtr<NActors::TEventHandle<TEvGetTokenResponse>>;

} // NCloud::NEvTokenAgent

namespace NKikimrYndx {

class TTokenAgentProviderHandler : public NActors::TActorBootstrapped<TTokenAgentProviderHandler> {
    using TBase = NActors::TActorBootstrapped<TTokenAgentProviderHandler>;

private:
    const NActors::TActorId Sender;
    const NActors::TActorId TokenAgentId;
    const NKikimrProto::TTokenManager::TTokenAgentProvider::TTokenAgentInfo& ProviderInfo;
    const NKikimr::NTokenManager::TTokenProviderSettings& Settings;

public:
    TTokenAgentProviderHandler(const NActors::TActorId& sender,
                                    const NActors::TActorId& tokenAgentId,
                                    const NKikimrProto::TTokenManager::TTokenAgentProvider::TTokenAgentInfo& providerInfo,
                                    const NKikimr::NTokenManager::TTokenProviderSettings& settings);
    void Bootstrap();
    void StateWork(TAutoPtr<NActors::IEventHandle>& ev);

private:
    void Handle(NCloud::NEvTokenAgent::TEvGetTokenResponse_HandlePtr& ev);
};

} // NKikimrYndx
