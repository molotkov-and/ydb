#pragma once

#include <ydb/library/ycloud/api/token_agent.h>
#include <ydb/library/grpc/actor_client/grpc_service_settings.h>


namespace NCloud {

using namespace NKikimr;

struct TTokenAgentClientSettings : public NGrpcActorClient::TGrpcClientSettings {};

IActor* CreateTokenAgent(const NGrpcActorClient::TGrpcClientSettings& settings);

IActor* CreateTokenAgent(const TString& endpoint);


// IActor* CreateAccessServiceWithCache(const TAccessServiceSettings& settings); // for compatibility with older code

}
