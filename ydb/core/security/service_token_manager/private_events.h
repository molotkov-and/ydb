#pragma once

#include "service_token_manager.h"

#include <util/datetime/base.h>

#include <ydb/library/actors/core/events.h>

namespace NKikimr::NServiceTokenManager {

struct TEvPrivate {
    enum EEv {
        EvUpdateToken = EventSpaceBegin(NActors::TEvents::ES_PRIVATE),
        EvErrorUpdateToken,
        EvEnd
    };

    static_assert(EvEnd < EventSpaceEnd(NActors::TEvents::ES_PRIVATE), "expect EvEnd < EventSpaceEnd(TEvents::ES_PRIVATE)");

    struct TEvUpdateToken : NActors::TEventLocal<TEvUpdateToken, EvUpdateToken> {
        TString Id;
        TEvServiceTokenManager::TStatus Status;
        TString Token;
        TDuration RefreshPeriod;

        TEvUpdateToken(const TString& id, TEvServiceTokenManager::TStatus status, const TString& token, const TDuration& refreshPeriod)
            : Id(id)
            , Status(status)
            , Token(token)
            , RefreshPeriod(refreshPeriod)
        {}
    };
};

} // NKikimr::NServiceTokenManager
