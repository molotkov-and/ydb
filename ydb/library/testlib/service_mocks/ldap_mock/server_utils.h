#pragma once
#include <ydb/core/security/certificate_check/cert_auth_utils.h>
#include <util/generic/string.h>
#include <util/generic/hash.h>

namespace LdapMock {

struct TTlsSettings {
    bool EnableSecureConnection = false;
    NKikimr::TCertAndKey CaCert;
    THashMap<TString, TString> MtlsAuthMap = {};
};

} // LdapMock
