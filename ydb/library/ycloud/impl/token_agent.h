#pragma once

#include <ydb/library/ycloud/api/token_agent.h>
#include <ydb/library/grpc/actor_client/grpc_service_settings.h>

namespace NCloud {

using namespace NKikimr;

// struct TAccessServiceSettings : NGrpcActorClient::TGrpcClientSettings {};

IActor* CreateTokenAgent(const NGrpcActorClient::TGrpcClientSettings& settings);

inline IActor* CreateTokenAgent(const TString& endpoint) {
    NGrpcActorClient::TGrpcClientSettings settings;
    settings.Endpoint = endpoint;
    return CreateTokenAgent(settings);
}


// IActor* CreateAccessServiceWithCache(const TAccessServiceSettings& settings); // for compatibility with older code

}
