#include <library/cpp/string_utils/base64/base64.h>
#include <util/random/random.h>
#include <util/string/hex.h>
#include <util/datetime/base.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <ydb/public/sdk/cpp/client/ydb_table/table.h>
#include <ydb/public/sdk/cpp/client/ydb_params/params.h>
#include <ydb/public/sdk/cpp/client/ydb_types/status/status.h>
#include "oidc_session.h"
#include "openid_connect.h"

namespace NOIDC {

const TDuration TOidcSession::STATE_LIFE_TIME = TDuration::Minutes(10);

TOidcSession::TOidcSession(const TString& state, const TString& redirectUrl, bool isAjaxRequest)
    : State(state)
    , IsAjaxRequest(isAjaxRequest)
    , RedirectUrl(redirectUrl)
{}

TOidcSession::TOidcSession(const NHttp::THttpIncomingRequestPtr& request)
    : State(GenerateState())
    , IsAjaxRequest(DetectAjaxRequest(request))
    , RedirectUrl(GetRedirectUrl(request, IsAjaxRequest))
{}

TOidcSession::TOidcSession(const TString& state, const NHttp::THttpIncomingRequestPtr& request, bool isAjaxRequest)
    : State(state)
    , IsAjaxRequest(isAjaxRequest)
    , RedirectUrl(GetRedirectUrl(request, IsAjaxRequest))
{}

TString TOidcSession::CreateOidcSessionCookie(const TString& secret) const {
    return TStringBuilder() << CreateNameYdbOidcCookie(secret, State)
                            << "=" << GenerateCookie(secret)
                            << "; Path=" << GetAuthCallbackUrl() << "; Max-Age=" << COOKIE_MAX_AGE_SEC <<"; SameSite=None; Secure";
}

void TOidcSession::CheckSessionStoredOnServerSide(const TString&, std::function<void(const TString& redirectUrl, bool isAjaxRequest)>) {
    // NYdb::NTable::TClientSettings clientTableSettings;
    // clientTableSettings.Database(MetaLocation.RootDomain)
    // .AuthToken(MVPAppData()->Tokenator->GetToken(MetaAccessTokenName));
    // auto tableClient = MetaLocation.GetTableClient(clientTableSettings);
    // tableClient.CreateSession().Subscribe([state, cb = std::move(cb)] (const NYdb::NTable::TAsyncCreateSessionResult& result) {
    //     auto resultCopy = result;
    //     auto res = resultCopy.ExtractValue();
    //     if (res.IsSuccess()) {
    //         auto session = res.GetSession();
    //         TStringBuilder query;
    //         query << "DECLARE $STATE AS Text;\n";
    //         query << "SELECT * FROM `ydb/OidcSessions` WHERE state=$STATE AND CurrentUtcDatetime() < expire_time;\n";
    //         auto txControl = NYdb::NTable::TTxControl::BeginTx(NYdb::NTable::TTxSettings::OnlineRO(NYdb::NTable::TTxOnlineSettings().AllowInconsistentReads(true))).CommitTx();
    //         NYdb::TParamsBuilder params;
    //         params.AddParam("$STATE", NYdb::TValueBuilder().Utf8(state).Build());
    //         auto executeDataQueryResult = session.ExecuteDataQuery(query, txControl, params.Build());
    //         executeDataQueryResult.Subscribe([state, cb = std::move(cb), session] (const NYdb::NTable::TAsyncDataQueryResult& result) mutable {
    //             NYdb::NTable::TAsyncDataQueryResult resultCopy = result;
    //             auto res = resultCopy.ExtractValue();
    //             if (res.IsSuccess()) {
    //                 Cerr << "CheckSessionStoredOnServerSide - success:\n" << (NYdb::TStatus&)res << Endl;
    //                 try {
    //                     auto resultSet = res.GetResultSet(0);
    //                     NYdb::TResultSetParser rsParser(resultSet);
    //                     const auto& columnsMeta = resultSet.GetColumnsMeta();
    //                     if (rsParser.TryNextRow()) {
    //                         // TString forward = (rsParser.ColumnParser(0).GetOptionalUtf8()).GetRef();
    //                         // TInstant deadline = (rsParser.ColumnParser(1).GetOptionalTimestamp()).GetRef();
    //                         TString redirectUrl;
    //                         bool isAjaxRequest = false;;
    //                         for (size_t columnNum = 0; columnNum < columnsMeta.size(); ++columnNum) {
    //                             const NYdb::TColumn& columnMeta = columnsMeta[columnNum];
    //                             if (columnMeta.Name == "redirect_url") {
    //                                 redirectUrl = rsParser.ColumnParser(columnNum).GetOptionalUtf8().GetRef();
    //                             }
    //                             if (columnMeta.Name == "is_ajax_request") {
    //                                 isAjaxRequest = rsParser.ColumnParser(columnNum).GetOptionalBool().GetRef();
    //                             }
    //                         }
    //                         cb(redirectUrl, isAjaxRequest);
    //                     } else {
    //                         Cerr << "CheckSessionStoredOnServerSide - no data for " << state << Endl;
    //                     }
    //                 } catch (const std::exception& e) {
    //                     Cerr << "CheckSessionStoredOnServerSide - failed to get result:\n" << e.what() << Endl;
    //                 }
    //             } else {
    //                 // no result
    //                 Cerr << "CheckSessionStoredOnServerSide - failed to get result:\n" << (NYdb::TStatus&)res << Endl;
    //             }
    //             session.Close();
    //         });
    //     } else {
    //         Cerr << "CheckSessionStoredOnServerSide - failed to get session:\n" << (NYdb::TStatus&)res << Endl;
    //     }
    // });
}

TString TOidcSession::GetState() const {
    return State;
}

bool TOidcSession::GetIsAjaxRequest() const {
    return IsAjaxRequest;
}

TString TOidcSession::GetRedirectUrl() const {
    return RedirectUrl;
}

TString TOidcSession::CreateNameYdbOidcCookie(TStringBuf key, TStringBuf state) {
    static const TString YDB_OIDC_COOKIE = "ydb_oidc_cookie";
    return YDB_OIDC_COOKIE + "_" + HexEncode(HmacSHA256(key, state));
}

TString TOidcSession::GenerateCookie(const TString& secret) const {
    TInstant expirationTime = TInstant::Now() + STATE_LIFE_TIME;
    TStringBuilder stateStruct;
    stateStruct << "{\"state\":\"" << State
                << "\",\"redirect_url\":\"" << RedirectUrl
                << "\",\"expiration_time\":" << ToString(expirationTime.TimeT())
                << ",\"ajax_request\":" << (IsAjaxRequest ? "true" : "false") << "}";
    TString digest = HmacSHA256(secret, stateStruct);
    TString cookieStruct {"{\"state_struct\":\"" + Base64Encode(stateStruct) + "\",\"digest\":\"" + Base64Encode(digest) + "\"}"};
    return Base64Encode(cookieStruct);
}

TString TOidcSession::GenerateState() {
    TStringBuilder sb;
    const size_t CHAR_NUMBER = 15;
    for (size_t i{0}; i < CHAR_NUMBER; i++) {
        sb << RandomNumber<char>();
    }
    return Base64EncodeUrlNoPadding(sb);
}

TStringBuf TOidcSession::GetRedirectUrl(const NHttp::THttpIncomingRequestPtr& request, bool isAjaxRequest) {
    NHttp::THeaders headers(request->Headers);
    TStringBuf requestedUrl = headers.Get("Referer");
    if (!isAjaxRequest || requestedUrl.empty()) {
        return request->URL;
    }
    return requestedUrl;
}

bool TOidcSession::DetectAjaxRequest(const NHttp::THttpIncomingRequestPtr& request) {
    const static THashMap<TStringBuf, TStringBuf> expectedHeaders {
        {"Accept", "application/json"}
    };
    NHttp::THeaders headers(request->Headers);
    for (const auto& el : expectedHeaders) {
        TStringBuf headerValue = headers.Get(el.first);
        if (!headerValue || headerValue.find(el.second) == TStringBuf::npos) {
            return false;
        }
    }
    return true;
}

} // NOIDC
