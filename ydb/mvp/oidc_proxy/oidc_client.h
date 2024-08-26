#pragma once

#include <ydb/mvp/core/core_ydb.h>

namespace NOIDC {

struct TOpenIdConnectSettings;

void InitOIDC(NActors::TActorSystem& actorSystem, const NActors::TActorId& httpProxyId, const NOIDC::TOpenIdConnectSettings& settings);

} // NOIDC
