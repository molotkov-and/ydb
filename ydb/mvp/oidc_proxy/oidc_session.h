#pragma once

#include <util/generic/string.h>
#include <functional>
#include <ydb/library/actors/http/http.h>
// #include <ydb/mvp/core/core_ydb.h>

namespace NYdb {
class TStatus;
}
namespace NOIDC {

struct TOpenIdConnectSettings;


class TOidcSession {
public:
    static const TDuration STATE_LIFE_TIME;

private:
    static constexpr size_t COOKIE_MAX_AGE_SEC = 420;

    TString State;
    bool IsAjaxRequest = false;
    TString RedirectUrl;

public:
    TOidcSession(const TString& state = "", const TString& redirectUrl = "", bool isAjaxRequest = false);
    TOidcSession(const NHttp::THttpIncomingRequestPtr& request);
    TOidcSession(const TString& state, const NHttp::THttpIncomingRequestPtr& request, bool isAjaxRequest);

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
