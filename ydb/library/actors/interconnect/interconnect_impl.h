#pragma once

#include "interconnect.h"
#include <ydb/library/actors/protos/interconnect.pb.h>
#include <ydb/library/actors/core/event_pb.h>
#include <library/cpp/monlib/dynamic_counters/counters.h>

namespace NActors {
    // register node
    struct TEvInterconnect::TEvRegisterNode: public TEventBase<TEvInterconnect::TEvRegisterNode, TEvInterconnect::EvRegisterNode> {
    };

    // reply on register node
    struct TEvInterconnect::TEvRegisterNodeResult: public TEventBase<TEvInterconnect::TEvRegisterNodeResult, TEvInterconnect::EvRegisterNodeResult> {
    };

    // disconnect
    struct TEvInterconnect::TEvDisconnect: public TEventLocal<TEvInterconnect::TEvDisconnect, TEvInterconnect::EvDisconnect> {
    };

}
