#pragma once
#include <ydb/library/ycloud/api/access_service.h>
#include <ydb/library/grpc/actor_client/grpc_service_settings.h>

namespace NCloud {

using namespace NKikimr;

struct TAccessServiceSettings : NGrpcActorClient::TGrpcClientSettings {};

IActor* CreateAccessService(const TAccessServiceSettings& settings);

inline IActor* CreateAccessService(const TString& endpoint) {
    TAccessServiceSettings settings;
    settings.Endpoint = endpoint;
    return CreateAccessService(settings);
}

IActor* CreateAccessServiceWithCache(const TAccessServiceSettings& settings); // for compatibility with older code

}
