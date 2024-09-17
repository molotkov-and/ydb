#include <ydb/mvp/core/mvp_log.h>
#include "context_storage_refresher.h"
#include "context_storage.h"

namespace NMVP {
namespace NOIDC {

TContextStorageRefresher::TContextStorageRefresher(TContextStorage* const contextStorage)
    : ContextStorage(contextStorage)
{}

void TContextStorageRefresher::Bootstrap() {
    Become(&TContextStorageRefresher::StateWork, PERIODIC_CHECK, new NActors::TEvents::TEvWakeup());
}

void TContextStorageRefresher::HandleRefresh() {
    LOG_DEBUG_S(*NActors::TlsActivationContext, NMVP::EService::MVP, "+++ Refresh storage");
    ContextStorage->Refresh(TInstant::Now());
    Schedule(PERIODIC_CHECK, new NActors::TEvents::TEvWakeup());
}

TContextStorageRefresher* TContextStorageRefresher::CreateRestoreContextRefresher(TContextStorage* const contextStorage) {
    return new TContextStorageRefresher(contextStorage);
}

} // NOIDC
} // NMVP
