GO_LIBRARY()
IF (OS_DARWIN AND ARCH_ARM64 AND RACE AND CGO_ENABLED OR OS_DARWIN AND ARCH_ARM64 AND RACE AND NOT CGO_ENABLED OR OS_DARWIN AND ARCH_ARM64 AND NOT RACE AND CGO_ENABLED OR OS_DARWIN AND ARCH_ARM64 AND NOT RACE AND NOT CGO_ENABLED OR OS_DARWIN AND ARCH_X86_64 AND RACE AND CGO_ENABLED OR OS_DARWIN AND ARCH_X86_64 AND RACE AND NOT CGO_ENABLED OR OS_DARWIN AND ARCH_X86_64 AND NOT RACE AND CGO_ENABLED OR OS_DARWIN AND ARCH_X86_64 AND NOT RACE AND NOT CGO_ENABLED)
    SRCS(
        addrselect.go
        cgo_darwin.go
        cgo_unix.go
        cgo_unix_syscall.go
        conf.go
        dial.go
        dnsclient.go
        dnsclient_unix.go
        dnsconfig.go
        dnsconfig_unix.go
        error_posix.go
        error_unix.go
        fd_posix.go
        fd_unix.go
        file.go
        file_unix.go
        hook.go
        hook_unix.go
        hosts.go
        interface.go
        interface_bsd.go
        interface_darwin.go
        ip.go
        iprawsock.go
        iprawsock_posix.go
        ipsock.go
        ipsock_posix.go
        lookup.go
        lookup_unix.go
        mac.go
        mptcpsock_stub.go
        net.go
        netcgo_off.go
        netgo_off.go
        nss.go
        parse.go
        pipe.go
        port.go
        port_unix.go
        rawconn.go
        rlimit_unix.go
        sendfile_unix_alt.go
        sock_bsd.go
        sock_posix.go
        sockaddr_posix.go
        sockopt_bsd.go
        sockopt_posix.go
        sockoptip_bsdvar.go
        sockoptip_posix.go
        splice_stub.go
        sys_cloexec.go
        tcpsock.go
        tcpsock_posix.go
        tcpsock_unix.go
        tcpsockopt_darwin.go
        tcpsockopt_posix.go
        udpsock.go
        udpsock_posix.go
        unixsock.go
        unixsock_posix.go
        unixsock_readmsg_cloexec.go
        writev_unix.go
    )
ELSEIF (OS_LINUX AND ARCH_AARCH64 AND RACE AND CGO_ENABLED OR OS_LINUX AND ARCH_AARCH64 AND NOT RACE AND CGO_ENABLED OR OS_LINUX AND ARCH_X86_64 AND RACE AND CGO_ENABLED OR OS_LINUX AND ARCH_X86_64 AND NOT RACE AND CGO_ENABLED OR OS_LINUX AND ARCH_ARM6 AND RACE AND CGO_ENABLED OR OS_LINUX AND ARCH_ARM6 AND NOT RACE AND CGO_ENABLED OR OS_LINUX AND ARCH_ARM7 AND RACE AND CGO_ENABLED OR OS_LINUX AND ARCH_ARM7 AND NOT RACE AND CGO_ENABLED)
    SRCS(
        addrselect.go
        cgo_unix.go
        conf.go
        dial.go
        dnsclient.go
        dnsclient_unix.go
        dnsconfig.go
        dnsconfig_unix.go
        error_posix.go
        error_unix.go
        fd_posix.go
        fd_unix.go
        file.go
        file_unix.go
        hook.go
        hook_unix.go
        hosts.go
        interface.go
        interface_linux.go
        ip.go
        iprawsock.go
        iprawsock_posix.go
        ipsock.go
        ipsock_posix.go
        lookup.go
        lookup_unix.go
        mac.go
        mptcpsock_linux.go
        net.go
        netcgo_off.go
        netgo_off.go
        nss.go
        parse.go
        pipe.go
        port.go
        port_unix.go
        rawconn.go
        rlimit_unix.go
        sendfile_linux.go
        sock_cloexec.go
        sock_linux.go
        sock_posix.go
        sockaddr_posix.go
        sockopt_linux.go
        sockopt_posix.go
        sockoptip_linux.go
        sockoptip_posix.go
        splice_linux.go
        tcpsock.go
        tcpsock_posix.go
        tcpsock_unix.go
        tcpsockopt_posix.go
        tcpsockopt_unix.go
        udpsock.go
        udpsock_posix.go
        unixsock.go
        unixsock_posix.go
        unixsock_readmsg_cmsg_cloexec.go
        writev_unix.go
    )
    
IF (CGO_ENABLED)
    CGO_SRCS(
                    cgo_linux.go
            cgo_resnew.go
            cgo_socknew.go
            cgo_unix_cgo.go
            cgo_unix_cgo_res.go
    )
ENDIF()
ELSEIF (OS_LINUX AND ARCH_AARCH64 AND RACE AND NOT CGO_ENABLED OR OS_LINUX AND ARCH_AARCH64 AND NOT RACE AND NOT CGO_ENABLED OR OS_LINUX AND ARCH_X86_64 AND RACE AND NOT CGO_ENABLED OR OS_LINUX AND ARCH_X86_64 AND NOT RACE AND NOT CGO_ENABLED OR OS_LINUX AND ARCH_ARM6 AND RACE AND NOT CGO_ENABLED OR OS_LINUX AND ARCH_ARM6 AND NOT RACE AND NOT CGO_ENABLED OR OS_LINUX AND ARCH_ARM7 AND RACE AND NOT CGO_ENABLED OR OS_LINUX AND ARCH_ARM7 AND NOT RACE AND NOT CGO_ENABLED)
    SRCS(
        addrselect.go
        cgo_stub.go
        conf.go
        dial.go
        dnsclient.go
        dnsclient_unix.go
        dnsconfig.go
        dnsconfig_unix.go
        error_posix.go
        error_unix.go
        fd_posix.go
        fd_unix.go
        file.go
        file_unix.go
        hook.go
        hook_unix.go
        hosts.go
        interface.go
        interface_linux.go
        ip.go
        iprawsock.go
        iprawsock_posix.go
        ipsock.go
        ipsock_posix.go
        lookup.go
        lookup_unix.go
        mac.go
        mptcpsock_linux.go
        net.go
        netcgo_off.go
        netgo_off.go
        nss.go
        parse.go
        pipe.go
        port.go
        port_unix.go
        rawconn.go
        rlimit_unix.go
        sendfile_linux.go
        sock_cloexec.go
        sock_linux.go
        sock_posix.go
        sockaddr_posix.go
        sockopt_linux.go
        sockopt_posix.go
        sockoptip_linux.go
        sockoptip_posix.go
        splice_linux.go
        tcpsock.go
        tcpsock_posix.go
        tcpsock_unix.go
        tcpsockopt_posix.go
        tcpsockopt_unix.go
        udpsock.go
        udpsock_posix.go
        unixsock.go
        unixsock_posix.go
        unixsock_readmsg_cmsg_cloexec.go
        writev_unix.go
    )
ELSEIF (OS_WINDOWS AND ARCH_X86_64 AND RACE AND CGO_ENABLED OR OS_WINDOWS AND ARCH_X86_64 AND RACE AND NOT CGO_ENABLED OR OS_WINDOWS AND ARCH_X86_64 AND NOT RACE AND CGO_ENABLED OR OS_WINDOWS AND ARCH_X86_64 AND NOT RACE AND NOT CGO_ENABLED)
    SRCS(
        addrselect.go
        conf.go
        dial.go
        dnsclient.go
        dnsclient_unix.go
        dnsconfig.go
        dnsconfig_windows.go
        error_posix.go
        error_windows.go
        fd_posix.go
        fd_windows.go
        file.go
        file_windows.go
        hook.go
        hook_windows.go
        hosts.go
        interface.go
        interface_windows.go
        ip.go
        iprawsock.go
        iprawsock_posix.go
        ipsock.go
        ipsock_posix.go
        lookup.go
        lookup_windows.go
        mac.go
        mptcpsock_stub.go
        net.go
        netcgo_off.go
        netgo_off.go
        nss.go
        parse.go
        pipe.go
        port.go
        rawconn.go
        sendfile_windows.go
        sock_posix.go
        sock_windows.go
        sockaddr_posix.go
        sockopt_posix.go
        sockopt_windows.go
        sockoptip_posix.go
        sockoptip_windows.go
        splice_stub.go
        tcpsock.go
        tcpsock_posix.go
        tcpsock_windows.go
        tcpsockopt_posix.go
        tcpsockopt_windows.go
        udpsock.go
        udpsock_posix.go
        unixsock.go
        unixsock_posix.go
        unixsock_readmsg_other.go
    )
ENDIF()
END()
