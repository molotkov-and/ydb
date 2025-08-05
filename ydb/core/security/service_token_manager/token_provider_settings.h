#pragma once

#include <util/datetime/base.h>

namespace NKikimr::NServiceTokenManager {

struct TTokenProviderSettings {
    TDuration SuccessRefreshPeriod;
    TDuration ErrorRefreshPeriod;
};

} // NKikimr::NServiceTokenManager
