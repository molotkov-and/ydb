#pragma once

#include <ydb/core/base/defs.h>
#include <ydb/core/base/events.h>
#include <ydb/public/api/client/yc_private/iam/token_agent.grpc.pb.h>
#include "events.h"

namespace NCloud {

using namespace NKikimr;

class TIamTokenService;

namespace NEvTokenAgent {
    enum EEv {
        // requests
        EvGetTokenRequest = EventSpaceBegin(TKikimrEvents::ES_TOKEN_AGENT),

        // replies
        EvGetTokenResponse = EventSpaceBegin(TKikimrEvents::ES_TOKEN_AGENT) + 1024,

        EvEnd
    };

    static_assert(EvEnd < EventSpaceEnd(TKikimrEvents::ES_TOKEN_AGENT), "expect EvEnd < EventSpaceEnd(TKikimrEvents::ES_TOKEN_AGENT)");

    struct TEvGetTokenRequest : TEvGrpcProtoRequest<TEvGetTokenRequest, EvGetTokenRequest, yandex::cloud::priv::iam::v1::GetTokenRequest> {};
    struct TEvGetTokenResponse : TEvGrpcProtoResponse<TEvGetTokenResponse, EvGetTokenResponse, yandex::cloud::priv::iam::v1::GetTokenResponse> {};
};

} // NCloud
