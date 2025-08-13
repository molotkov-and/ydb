#pragma once

#include <ydb/library/ycloud/api/token_agent.h>

namespace NGrpcActorClient {

struct TGrpcClientSettings;

} // NGrpcActorClient

namespace NCloud {

using namespace NKikimr;

IActor* CreateTokenAgent(const NGrpcActorClient::TGrpcClientSettings& settings);

IActor* CreateTokenAgent(const TString& endpoint);


// IActor* CreateAccessServiceWithCache(const TAccessServiceSettings& settings); // for compatibility with older code

}
