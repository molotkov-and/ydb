#pragma once

#include <util/generic/string.h>
#include <functional>
#include <ydb/library/actors/http/http.h>
#include <ydb/mvp/core/core_ydb.h>

namespace NYdb {
class TStatus;
}
namespace NOIDC {

struct TOpenIdConnectSettings;


class TOidcSession {
private:
    static constexpr size_t COOKIE_MAX_AGE_SEC = 420;
    static const TDuration STATE_LIFE_TIME;

    TString State;
    bool IsAjaxRequest = false;
    TString RedirectUrl;
    TYdbLocation MetaLocation;
    TString MetaAccessTokenName;

public:
    TOidcSession(const TString& state = "", const TString& redirectUrl = "", bool isAjaxRequest = false);
    TOidcSession(const TOpenIdConnectSettings& settings);
    TOidcSession(const NHttp::THttpIncomingRequestPtr& request, const TOpenIdConnectSettings& settings);
    TOidcSession(const TString& state, const NHttp::THttpIncomingRequestPtr& request, const TOpenIdConnectSettings& settings, bool isAjaxRequest);

    void SaveSessionOnServerSide(std::function<void(const NYdb::TStatus& status, const TString& error)> cb) const;
    TString CreateOidcSessionCookie(const TString& secret) const;
    void CheckSessionStoredOnServerSide(const TString& state, std::function<void(const TString& redirectUrl, bool isAjaxRequest)> cb);

    TString GetState() const;
    bool GetIsAjaxRequest() const;
    TString GetRedirectUrl() const;

    static TString CreateNameYdbOidcCookie(TStringBuf key, TStringBuf state);

private:
    static TString GenerateState();
    static TStringBuf GetRedirectUrl(const NHttp::THttpIncomingRequestPtr& request, bool isAjaxRequest);
    static bool DetectAjaxRequest(const NHttp::THttpIncomingRequestPtr& request);

    TString GenerateCookie(const TString& secret) const;
};

} // NOIDC
