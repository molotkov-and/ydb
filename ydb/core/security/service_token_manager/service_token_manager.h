#pragma once

#include <ydb/library/actors/core/actor.h>
#include <ydb/library/actors/core/events.h>
#include <ydb/core/base/events.h>

namespace NKikimrProto {

class TServiceTokenManager;

} // NKikimrProto


namespace NKikimr {

struct TEvServiceTokenManager {
    enum EEv {
        EvSubscribeUpdateToken = EventSpaceBegin(TKikimrEvents::ES_SERVICE_TOKEN_MANAGER),
        EvUpdateToken,
        EvEnd
    };

    static_assert(EvEnd < EventSpaceEnd(TKikimrEvents::ES_SERVICE_TOKEN_MANAGER), "expect EvEnd < EventSpaceEnd(TKikimrEvents::ES_SERVICE_TOKEN_MANAGER)");

    struct TEvSubscribeUpdateToken : NActors::TEventLocal<TEvSubscribeUpdateToken, EvSubscribeUpdateToken> {
        TString Id;

        TEvSubscribeUpdateToken(const TString& id)
            : Id(id)
        {}
    };

    struct TStatus {
        enum class ECode {
            SUCCESS,
            NOT_READY,
            ERROR,
        };

        ECode Code;
        TString Message;
    };

    struct TEvUpdateToken : NActors::TEventLocal<TEvUpdateToken, EvUpdateToken> {
        TString Id;
        TString Token;
        TStatus Status;

        TEvUpdateToken(const TString& id, const TString& token, const TStatus& status)
            : Id(id)
            , Token(token)
            , Status(status)
        {}
    };
};

inline NActors::TActorId MakeServiceTokenManagerID() {
    static const char name[12] = "srvtokmngr";
    return NActors::TActorId(0, TStringBuf(name, 12));
}

NActors::IActor* CreateServiceTokenManager(const NKikimrProto::TServiceTokenManager& config);
NActors::IActor* CreateServiceTokenManager(const NKikimrProto::TServiceTokenManager& config, const NActors::TActorId& HttpProxyId);

} // NKikimr
