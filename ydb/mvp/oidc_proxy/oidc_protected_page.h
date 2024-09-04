#pragma once

#include <util/generic/string.h>
#include <util/generic/strbuf.h>
#include <ydb/library/actors/core/actor_bootstrapped.h>
#include <ydb/library/actors/core/actorid.h>
#include <ydb/library/actors/http/http.h>
#include <ydb/library/actors/http/http_proxy.h>
#include "oidc_settings.h"

namespace NMVP {
namespace NOIDC {

class THandlerSessionServiceCheck : public NActors::TActorBootstrapped<THandlerSessionServiceCheck> {
protected:
    using TBase = NActors::TActorBootstrapped<THandlerSessionServiceCheck>;

    const NActors::TActorId Sender;
    const NHttp::THttpIncomingRequestPtr Request;
    NActors::TActorId HttpProxyId;
    const TOpenIdConnectSettings Settings;
    TString ProtectedPageUrl;
    TString RequestedPageScheme;
    bool IsAjaxRequest = false;

    const static inline TStringBuf IAM_TOKEN_SCHEME = "Bearer ";
    const static inline TStringBuf IAM_TOKEN_SCHEME_LOWER = "bearer ";
    const static inline TStringBuf AUTH_HEADER_NAME = "Authorization";

public:
    THandlerSessionServiceCheck(const NActors::TActorId& sender,
                                const NHttp::THttpIncomingRequestPtr& request,
                                const NActors::TActorId& httpProxyId,
                                const TOpenIdConnectSettings& settings);


    // virtual void Bootstrap(const NActors::TActorContext& ctx) {
    //     if (!CheckRequestedHost()) {
    //         ctx.Send(Sender, new NHttp::TEvHttpProxy::TEvHttpOutgoingResponse(CreateResponseForbiddenHost()));
    //         Die(ctx);
    //         return;
    //     }
    //     NHttp::THeaders headers(Request->Headers);
    //     IsAjaxRequest = DetectAjaxRequest(headers);
    //     TStringBuf authHeader = headers.Get(AUTH_HEADER_NAME);
    //     if (Request->Method == "OPTIONS" || IsAuthorizedRequest(authHeader)) {
    //         ForwardUserRequest(TString(authHeader), ctx);
    //     } else {
    //         StartOidcProcess(ctx);
    //     }
    // }

    // void HandleProxy(NHttp::TEvHttpProxy::TEvHttpIncomingResponse::TPtr event, const NActors::TActorContext& ctx) {
    //     NHttp::THttpOutgoingResponsePtr httpResponse;
    //     if (event->Get()->Response != nullptr) {
    //         NHttp::THttpIncomingResponsePtr response = event->Get()->Response;
    //         LOG_DEBUG_S(ctx, EService::MVP, "Incoming response for protected resource: " << response->Status);
    //         if (NeedSendSecureHttpRequest(response)) {
    //             SendSecureHttpRequest(response, ctx);
    //             return;
    //         }
    //         NHttp::THeadersBuilder headers = GetResponseHeaders(response);
    //         TStringBuf contentType = headers.Get("Content-Type").NextTok(';');
    //         if (contentType == "text/html") {
    //             TString newBody = FixReferenceInHtml(response->Body, response->GetRequest()->Host);
    //             httpResponse = Request->CreateResponse( response->Status, response->Message, headers, newBody);
    //         } else {
    //             httpResponse = Request->CreateResponse( response->Status, response->Message, headers, response->Body);
    //         }
    //     } else {
    //         static constexpr size_t MAX_LOGGED_SIZE = 1024;
    //         LOG_DEBUG_S(ctx, EService::MVP, "Can not process request to protected resource:\n" << event->Get()->Request->GetRawData().substr(0, MAX_LOGGED_SIZE));
    //         httpResponse = CreateResponseForNotExistingResponseFromProtectedResource(event->Get()->GetError());
    //     }
    //     ctx.Send(Sender, new NHttp::TEvHttpProxy::TEvHttpOutgoingResponse(httpResponse));
    //     Die(ctx);
    // }
    virtual void Bootstrap(const NActors::TActorContext& ctx);

    void HandleProxy(NHttp::TEvHttpProxy::TEvHttpIncomingResponse::TPtr event, const NActors::TActorContext& ctx);

protected:
    virtual void StartOidcProcess(const NActors::TActorContext& ctx) = 0;
    virtual void ForwardUserRequest(TStringBuf authHeader, const NActors::TActorContext& ctx, bool secure = false);
    virtual bool NeedSendSecureHttpRequest(const NHttp::THttpIncomingResponsePtr& response) const = 0;

    bool CheckRequestedHost();
    void ForwardRequestHeaders(NHttp::THttpOutgoingRequestPtr& request) const;

    static bool IsAuthorizedRequest(TStringBuf authHeader);
    static TString FixReferenceInHtml(TStringBuf html, TStringBuf host, TStringBuf findStr);
    static TString FixReferenceInHtml(TStringBuf html, TStringBuf host);

private:
    NHttp::THeadersBuilder GetResponseHeaders(const NHttp::THttpIncomingResponsePtr& response) {
        static const TVector<TStringBuf> HEADERS_WHITE_LIST = {
            "Content-Type",
            "Connection",
            "X-Worker-Name",
            "Set-Cookie",
            "Access-Control-Allow-Origin",
            "Access-Control-Allow-Credentials",
            "Access-Control-Allow-Headers",
            "Access-Control-Allow-Methods"
        };
        NHttp::THeadersBuilder headers(response->Headers);
        NHttp::THeadersBuilder resultHeaders;
        for (const auto& header : HEADERS_WHITE_LIST) {
            if (headers.Has(header)) {
                resultHeaders.Set(header, headers.Get(header));
            }
        }
        static const TString LOCATION_HEADER_NAME = "Location";
        if (headers.Has(LOCATION_HEADER_NAME)) {
            resultHeaders.Set(LOCATION_HEADER_NAME, GetFixedLocationHeader(headers.Get(LOCATION_HEADER_NAME)));
        }
        return resultHeaders;
    }

    void SendSecureHttpRequest(const NHttp::THttpIncomingResponsePtr& response, const NActors::TActorContext& ctx) {
        NHttp::THttpOutgoingRequestPtr request = response->GetRequest();
        LOG_DEBUG_S(ctx, EService::MVP, "Try to send request to HTTPS port");
        NHttp::THeadersBuilder headers {request->Headers};
        ForwardUserRequest(headers.Get(AUTH_HEADER_NAME), ctx, true);
    }

    TString GetFixedLocationHeader(TStringBuf location) {
        TStringBuf scheme, host, uri;
        NHttp::CrackURL(ProtectedPageUrl, scheme, host, uri);
        if (location.StartsWith("//")) {
            return TStringBuilder() << '/' << (scheme.empty() ? "" : TString(scheme) + "://") << location.SubStr(2);
        } else if (location.StartsWith('/')) {
            return TStringBuilder() << '/'
                                    << (scheme.empty() ? "" : TString(scheme) + "://")
                                    << host << location;
        } else {
            TStringBuf locScheme, locHost, locUri;
            NHttp::CrackURL(location, locScheme, locHost, locUri);
            if (!locScheme.empty()) {
                return TStringBuilder() << '/' << location;
            }
        }
        return TString(location);
    }

    NHttp::THttpOutgoingResponsePtr CreateResponseForbiddenHost() {
        NHttp::THeadersBuilder headers;
        headers.Set("Content-Type", "text/html");
        SetCORS(Request, &headers);

        TStringBuf scheme, host, uri;
        NHttp::CrackURL(ProtectedPageUrl, scheme, host, uri);
        TStringBuilder html;
        html << "<html><head><title>403 Forbidden</title></head><body bgcolor=\"white\"><center><h1>";
        html << "403 Forbidden host: " << host;
        html << "</h1></center></body></html>";

        return Request->CreateResponse("403", "Forbidden", headers, html);
    }

    NHttp::THttpOutgoingResponsePtr CreateResponseForNotExistingResponseFromProtectedResource(const TString& errorMessage) {
        NHttp::THeadersBuilder headers;
        headers.Set("Content-Type", "text/html");
        SetCORS(Request, &headers);

        TStringBuilder html;
        html << "<html><head><title>400 Bad Request</title></head><body bgcolor=\"white\"><center><h1>";
        html << "400 Bad Request. Can not process request to protected resource: " << errorMessage;
        html << "</h1></center></body></html>";
        return Request->CreateResponse("400", "Bad Request", headers, html);
    }
    NHttp::THeadersBuilder GetResponseHeaders(const NHttp::THttpIncomingResponsePtr& response);
    void SendSecureHttpRequest(const NHttp::THttpIncomingResponsePtr& response, const NActors::TActorContext& ctx);
    TString GetFixedLocationHeader(TStringBuf location);
};

}  // NOIDC
}  // NMVP
