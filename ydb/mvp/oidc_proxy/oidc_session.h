#pragma once

#include <util/generic/string.h>
#include <functional>
#include <ydb/library/actors/http/http.h>
#include <ydb/mvp/core/core_ydb.h>

namespace NOIDC {

struct TOpenIdConnectSettings;

class TOidcSession {
private:
    static constexpr size_t COOKIE_MAX_AGE_SEC = 420;
    // static const TString YDB_OIDC_COOKIE = "ydb_oidc_cookie";

    TString State;
    TString RedirectUrl;
    bool IsAjaxRequest = false;
    TString OidcClientSecret;
    TYdbLocation MetaLocation;
    TString MetaAccessTokenName;

public:
    TOidcSession(const TOpenIdConnectSettings& settings);
    TOidcSession(const NHttp::THttpIncomingRequestPtr& request, const TOpenIdConnectSettings& settings, bool isAjaxRequest);
    TOidcSession(const TString& state, const NHttp::THttpIncomingRequestPtr& request, const TOpenIdConnectSettings& settings, bool isAjaxRequest);
    void SaveSessionOnServerSide(std::function<void(const TString& error)> cb) const;
    TString CreateOidcSessionCookie() const;
    TString Check(const TString& state, const NHttp::TCookies& cookies);

    TString GetState() const;
    bool GetIsAjaxRequest() const;
    TString GetRedirectUrl() const;

    static TString CreateNameYdbOidcCookie(TStringBuf key, TStringBuf state);

private:
    static TString GenerateState();
    static TStringBuf GetRedirectUrl(const NHttp::THttpIncomingRequestPtr& request, bool isAjaxRequest);

    TString GenerateCookie() const;
};

} // NOIDC
