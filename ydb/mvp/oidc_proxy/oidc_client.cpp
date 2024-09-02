#include "oidc_client.h"
#include "oidc_protected_page_handler.h"
#include "oidc_session_create_handler.h"
#include <ydb/mvp/core/core_ydb.h>

namespace NOIDC {

namespace {

TYdbLocation MetaLocation {
        "oidc-proxy",
        "oidc-proxy",
        {},
        {}
    };

} // namespace

void InitOIDC(NActors::TActorSystem& actorSystem,
              const NActors::TActorId& httpProxyId,
              const NOIDC::TOpenIdConnectSettings& settings) {
    MetaLocation.Endpoints.emplace_back("cluster-api", settings.StoreSessionsOnServerSideSetting.Endpoint);
    MetaLocation.RootDomain = settings.StoreSessionsOnServerSideSetting.Database;
    actorSystem.Send(httpProxyId, new NHttp::TEvHttpProxy::TEvRegisterHandler(
                         "/auth/callback",
                         actorSystem.Register(new NOIDC::TSessionCreateHandler(httpProxyId, settings, MetaLocation))
                         )
                     );

    actorSystem.Send(httpProxyId, new NHttp::TEvHttpProxy::TEvRegisterHandler(
                        "/",
                        actorSystem.Register(new NOIDC::TProtectedPageHandler(httpProxyId, settings, MetaLocation))
                        )
                    );
}

} // NOIDC
