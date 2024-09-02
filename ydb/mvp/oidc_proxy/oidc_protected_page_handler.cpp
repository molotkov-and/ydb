#include "oidc_protected_page_handler.h"
#include "oidc_protected_page_nebius.h"
#include "oidc_protected_page_yandex.h"

namespace NOIDC {

TProtectedPageHandler::TProtectedPageHandler(const NActors::TActorId& httpProxyId, const TOpenIdConnectSettings& settings, const TYdbLocation& location)
    : TBase(&TProtectedPageHandler::StateWork)
    , HttpProxyId(httpProxyId)
    , Settings(settings)
    , Location(location)
{}

void TProtectedPageHandler::Handle(NHttp::TEvHttpProxy::TEvHttpIncomingRequest::TPtr event, const NActors::TActorContext& ctx) {
    switch (Settings.AccessServiceType) {
        case NMvp::yandex_v2:
            ctx.Register(new THandlerSessionServiceCheckYandex(event->Sender, event->Get()->Request, HttpProxyId, Settings, Location));
            break;
        case NMvp::nebius_v1:
            ctx.Register(new THandlerSessionServiceCheckNebius(event->Sender, event->Get()->Request, HttpProxyId, Settings, Location));
            break;
    }
}

}  // NOIDC
