/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include "socketpair.hh"
#include "exception.hh"

using namespace std;

template <typename T> void zero( T & x ) { memset( &x, 0, sizeof( x ) ); }

pair<UnixDomainSocket, UnixDomainSocket> UnixDomainSocket::make_pair( void )
{
    int pipe[ 2 ];
    SystemCall( "socketpair", socketpair( AF_UNIX, SOCK_DGRAM, 0, pipe ) );
    return ::make_pair( UnixDomainSocket( pipe[ 0 ] ), UnixDomainSocket( pipe[ 1 ] ) );
}

void UnixDomainSocket::send_fd( FileDescriptor & fd )
{
    msghdr message_header;
    zero( message_header );

    char control_buffer[ CMSG_SPACE( sizeof( fd.fd_num() ) ) ];
    message_header.msg_control = control_buffer;
    message_header.msg_controllen = sizeof( control_buffer );

    cmsghdr * const control_message = CMSG_FIRSTHDR( &message_header );
    control_message->cmsg_level = SOL_SOCKET;
    control_message->cmsg_type = SCM_RIGHTS;
    control_message->cmsg_len = CMSG_LEN( sizeof( fd.fd_num() ) );
    *reinterpret_cast<int *>( CMSG_DATA( control_message ) ) = fd.fd_num();
    message_header.msg_controllen = control_message->cmsg_len;

    if ( 0 != SystemCall( "sendmsg", sendmsg( fd_num(), &message_header, 0 ) ) ) {
        throw runtime_error( "send_fd: sendmsg unexpectedly sent data" );
    }

    register_write();
}

FileDescriptor UnixDomainSocket::recv_fd( void )
{
    msghdr message_header;
    zero( message_header );

    char control_buffer[ CMSG_SPACE( sizeof( int ) ) ];
    message_header.msg_control = control_buffer;
    message_header.msg_controllen = sizeof( control_buffer );

    if ( 0 != SystemCall( "recvmsg", recvmsg( fd_num(), &message_header, 0 ) ) ) {
        throw runtime_error( "recv_fd: recvmsg unexpectedly received data" );
    }

    if ( message_header.msg_flags & MSG_CTRUNC ) {
        throw runtime_error( "recvmsg: control data was truncated" );
    }

    const cmsghdr * const control_message = CMSG_FIRSTHDR( &message_header );
    if ( (not control_message)
         or (control_message->cmsg_level != SOL_SOCKET)
         or (control_message->cmsg_type != SCM_RIGHTS) ) {
        throw runtime_error( "recvmsg: unexpected control message" );
    }

    if ( control_message->cmsg_len != CMSG_LEN( sizeof( int ) ) ) {
        throw runtime_error( "recvmsg: unexpected control message length" );
    }

    register_read();

    return *reinterpret_cast<int *>( CMSG_DATA( control_message ) );
}
