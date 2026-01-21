#pragma once
#include <util/system/types.h>
#include <util/generic/ptr.h>
#include <util/generic/hash.h>
#include <util/stream/input.h>
#include <util/stream/output.h>
#include <util/thread/pool.h>
#include <util/network/pair.h>
#include <util/network/poller.h>

#include "server_utils.h"
#include "ldap_defines.h"

class TInetStreamSocket;
class TStreamSocket;

namespace LdapMock {

struct TTlsSettings;

class TLdapSimpleServer {
public:
    using TRequestHandler = std::function<void(TAtomicSharedPtr<TStreamSocket> socket)>;

public:
    TLdapSimpleServer(ui16 port, const std::pair<TLdapMockResponses, TLdapMockResponses>& responses, const TTlsSettings& tlsSettings);
    TLdapSimpleServer(ui16 port, const TLdapMockResponses& responses, const TTlsSettings& tlsSettings);
    ~TLdapSimpleServer();

    void Stop();

    int GetPort() const;
    TString GetAddress() const;

    void UpdateResponses();

private:
    const int Port;
    THolder<IThreadPool> ThreadPool;
    THolder<IThreadFactory::IThread> ListenerThread;
    THolder<TInetStreamSocket> SendFinishSocket;

    std::pair<TLdapMockResponses, TLdapMockResponses> Responses;
    std::atomic_bool UseFirstSetResponses = true;
    THashMap<TString, TString> MtlsAuthMap;
};

}
