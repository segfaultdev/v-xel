// msgbox.c
//
// https://github.com/tylerneylon/msgbox
//

#include "msgbox.h"

// Universal (windows/mac/linux) headers.

// Windows has weird dependency issues with the order of includes between
// windows.h and winsock.h. Defining this macro allows us to include
// windows.h first. See here:
// http://stackoverflow.com/a/1372836
#define _WINSOCKAPI_

#include "cstructs.h"
#include "dbgcheck.h"

#include <stdio.h>

// Universal forward declarations for os-specific code.
static Array conns    = NULL;  // msg_Conn * items.
static Array removals = NULL;  // int items; runloop removes these conns.

static void array__remove_and_fill (Array array, int index);
static void array__remove_last     (Array array);

typedef enum {
  poll_mode_read  = 1,
  poll_mode_write = 2,
  poll_mode_err   = 4
} PollMode;


///////////////////////////////////////////////////////////////////////////////
//  Debug mode setup.

static int verbosity = 0;

// This can be used in cases of emergency debugging.
#define prline \
    printf("<pid %d> %s:%d(%s)\n", getpid(), __FILE__, __LINE__, __FUNCTION__)

// This array is only used when DEBUG is defined.
static int net_allocs[] = {0};  // Indexed by class.

#ifdef DEBUG

// TODO Remove this section as its functionality is being replaced by dbgcheck.

#include "../cstructs/memprofile.h"
#include <assert.h>

#else // non-DEBUG mode

#define assert(x)
#define alloc_class(bytes, class) malloc(bytes)
#define free_class(ptr, class) free(ptr)

#endif


///////////////////////////////////////////////////////////////////////////////
//  OS-specific code.

#ifndef _WIN32

// Non-windows setup.

#include <alloca.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// EWOULDBLOCK is the same as EAGAIN on mac.
#define err_would_block   EWOULDBLOCK
#define err_in_progress   EINPROGRESS
#define err_fault         EFAULT
#define err_invalid       EINVAL
#define err_bad_sock      EBADF
#define err_intr          EINTR
#define err_conn_reset    ECONNRESET
#define err_conn_refused  ECONNREFUSED
#define err_timed_out     ETIMEDOUT
// This has an impossible value as it's a windows-only error.
// The EMSGSIZE error has a similar name, but different meaning.
#define err_win_msg_size 1.5

#define library_init

// This does nothing on non-windows, but sets up a callback
// calling convention when compiled on windows.
#define ms_call_conv

typedef int socket_t;

typedef Array poll_fds_t;

#define closesocket close
#define poll_fn_name "poll"

// This array tracks sockets for run loop use.
// Index-matched to the conns array.
static poll_fds_t poll_fds;

// mac/linux version
static int get_errno() {
  return errno;
}

static void set_errno(int err) {
  errno = err;
}

// mac/linux version
static char *err_str() {
  return strerror(errno);
}

// mac/linux version
static void remove_last_polling_conn() {
  array__remove_last(conns);
  array__remove_last(poll_fds);
}

// mac/linux version
static void init_poll_fds() {
  poll_fds = array__new(8, sizeof(struct pollfd));
}

// mac/linux version
static void remove_from_poll_fds(int index) {
  array__remove_and_fill(poll_fds, index);
}

// mac/linux version
static void add_to_poll_fds(int new_sock, PollMode poll_mode) {
  // TODO Update this for other possible poll_mode inputs.
  short events = POLLIN;
  struct pollfd *new_poll_fd = (struct pollfd *)array__new_ptr(poll_fds);
  new_poll_fd->fd      = new_sock;
  new_poll_fd->events  = events;

  // Important since we may check this before we call poll.
  new_poll_fd->revents = 0;
}

// Returns NULL on success, otherwise the name of the failing system call.
// mac/linux version
static const char *make_non_blocking(int sock) {
  int flags = fcntl(sock, F_GETFL, 0);
  if (flags == -1) return "fcntl";

  int ret_val = fcntl(sock, F_SETFL, flags | O_NONBLOCK);
  if (ret_val == -1) return "fcntl";

  return NULL;  // Indicate success.
}

/////
// This section is all about avoiding SIGPIPE on sends to a broken socket.

#ifdef __APPLE__

// mac version
static int avoid_sigpipe(int sock) {
  int set = 1;
  return setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, (void *)&set, sizeof(int));
}

#define send_flags 0

#else

// linux version
static int avoid_sigpipe(int sock) {
  // On linux, the send flags will avoid SIGPIPE for us.
  return 0;  // Indicates success.
}

#define send_flags MSG_NOSIGNAL

#endif

// End SIGPIPE section.
/////

// mac/linux version
static void set_conn_to_poll_mode(int index, PollMode poll_mode) {
  struct pollfd *poll_fd = array__item_ptr(poll_fds, index);
  poll_fd->events = ((poll_mode & poll_mode_read) ? POLLIN : POLLOUT);
}

// mac/linux version
static int check_poll_fds(int timeout_in_ms) {
  nfds_t num_fds = poll_fds->count;
  return poll((struct pollfd *)poll_fds->items, num_fds, timeout_in_ms);
}

// mac/linux version
static PollMode poll_fds_mode(int sock, int index) {
  PollMode poll_mode = 0;
  struct pollfd *poll_fd = (struct pollfd *)array__item_ptr(poll_fds, index);
  if (poll_fd->revents & POLLIN)        poll_mode |= poll_mode_read;
  if (poll_fd->revents & POLLOUT)       poll_mode |= poll_mode_write;
  if (poll_fd->revents &
      (POLLERR | POLLNVAL | POLLHUP))   poll_mode |= poll_mode_err;
  return poll_mode;
}

#else

// Windows setup.

#include <process.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "winutil.h"

// Allow void return values; useful for one-liners
// that make a call and exit a function.
#pragma warning (disable : 4098)

#define err_would_block   WSAEWOULDBLOCK
#define err_in_progress   WSAEINPROGRESS
#define err_bad_sock      WSAENOTSOCK
#define err_intr          WSAEINTR
#define err_conn_reset    WSAECONNRESET
#define err_win_msg_size  WSAEMSGSIZE
#define err_conn_refused  WSAECONNREFUSED
#define err_timed_out     WSAETIMEDOUT

// Consider adding this to winutil.h.
#define getpid _getpid

#define library_init library_init_()
#define ms_call_conv __stdcall
#define poll_fn_name "select"

typedef struct {
  Array poll_modes;  // Same index as conns; PollMode items.
  fd_set   read_fds;
  fd_set  write_fds;
  fd_set except_fds;
} poll_fds_t;

typedef int    socklen_t;
typedef int    nfds_t;
typedef SOCKET socket_t;

// Most winsock api (WSA) errors are contiguous with codes 10035-10071.
// This is the array of their names. The non-contiguous cases must be
// handled manually. These error names are from this msdn page:
// http://msdn.microsoft.com/en-us/library/windows/desktop/ms740668(v=vs.85).aspx
static const char *err_strs[] = {
  "WSAEWOULDBLOCK",  "WSAEINPROGRESS",   "WSAEALREADY",         // 10035 - 10037
  "WSAENOTSOCK",     "WSAEDESTADDRREQ",  "WSAEMSGSIZE",         // 10038 - 10040
  "WSAEPROTOTYPE",   "WSAENOPROTOOPT",   "WSAEPROTONOSUPPORT",  // 10041 - 10043
  "WSAESOCKTNOSUPPORT", "WSAEOPNOTSUPP", "WSAEPFNOSUPPORT",     // 10044 - 10046
  "WSAEAFNOSUPPORT", "WSAEADDRINUSE",    "WSAEADDRNOTAVAIL",    // 10047 - 10049
  "WSAENETDOWN",     "WSAENETUNREACH",   "WSAENETRESET",        // 10050 - 10052
  "WSAECONNABORTED", "WSAECONNRESET",    "WSAENOBUFS",          // 10053 - 10055
  "WSAEISCONN",      "WSAENOTCONN",      "WSAESHUTDOWN",        // 10056 - 10058
  "WSAETOOMANYREFS", "WSAETIMEDOUT",     "WSAECONNREFUSED",     // 10059 - 10061
  "WSAELOOP",        "WSAENAMETOOLONG",  "WSAEHOSTDOWN",        // 10062 - 10064
  "WSAEHOSTUNREACH", "WSAENOTEMPTY",     "WSAEPROCLIM",         // 10065 - 10067
  "WSAEUSERS",       "WSAEDQUOT",        "WSAESTALE",           // 10068 - 10070
  "WSAEREMOTE"                                                  // 10071
};

// windows version
static int get_errno() {
  return WSAGetLastError();
}

// windows version
static void set_errno(int err) {
  WSASetLastError(err);
}

// windows version
static const char *err_str() {
  int last_err = WSAGetLastError();
  int unique_err_nums[] = { 10004, 10009, 10013, 10014, 10022, 10024 };
  const char *unique_err_strs[] = {
    "WSAEINTR", "WSAEBADF", "WSAEACCES", "WSAEFAULT", "WSAEINVAL", "WSAEMFILE"
  };
  int num_unique_errs = sizeof(unique_err_nums) / sizeof(unique_err_nums[0]);
  for (int i = 0; i < num_unique_errs; ++i) {
    if (last_err == unique_err_nums[i]) return unique_err_strs[i];
  }
  if (10035 <= last_err && last_err <= 10071) {
    return err_strs[last_err - 10035];
  }
  static char err_msg[64];
  snprintf(err_msg, 64, "Unknown error code: %d", last_err);
  return err_msg;
}

static void library_init_() {
  WORD version_requested = MAKEWORD(2, 0);
  WSADATA wsa_data;
  int err = WSAStartup(version_requested, &wsa_data);

  if (err) fprintf(stderr, "Error: received error %d from WSAStartup.\n", err);
}

// This structure tracks sockets for run loop use.
static poll_fds_t poll_fds;

// windows version
static void remove_last_polling_conn() {
  array__remove_last(conns);
  array__remove_last(poll_fds.poll_modes);
}

// windows version
static void init_poll_fds() {
  poll_fds.poll_modes = array__new(16, sizeof(PollMode));
  // The fd_set items are set before each select call within check_poll_fds.
}

// windows version
static void remove_from_poll_fds(int index) {
  array__remove_and_fill(poll_fds.poll_modes, index);
}

// windows version
static void add_to_poll_fds(int new_sock, PollMode poll_mode) {
  array__new_val(poll_fds.poll_modes, PollMode) = poll_mode;
}

// Returns NULL on success, otherwise the name of the failing system call.
// windows version
static const char *make_non_blocking(int sock) {
  u_long nonblocking_mode = 1;
  int ret_val = ioctlsocket(sock, FIONBIO, &nonblocking_mode);
  if (ret_val != 0) return "ioctlsocket";
  return NULL;  // Indicate success.
}

// windows version
static int avoid_sigpipe(int sock) {
  // Do nothing; windows doesn't throw SIGPIPE on a broken socket.
  return 0;  // Indicates success.
}

#define send_flags 0

// windows version
static void set_conn_to_poll_mode(int index, PollMode poll_mode) {
  array__item_val(poll_fds.poll_modes, index, PollMode) = poll_mode;
}

// windows version
static int check_poll_fds(int timeout_in_ms) {

  // Set up the fd_set data.
  FD_ZERO(&poll_fds.read_fds);
  FD_ZERO(&poll_fds.write_fds);
  FD_ZERO(&poll_fds.except_fds);
  array__for(PollMode *, poll_mode, poll_fds.poll_modes, i) {
    msg_Conn *conn = array__item_val(conns, i, msg_Conn *);
    FD_SET(conn->socket, &poll_fds.except_fds);
    FD_SET(conn->socket, (*poll_mode == poll_mode_read ?
                          &poll_fds.read_fds :
                          &poll_fds.write_fds));
  }

  // Set up the timeout and call select.
  const struct timeval timeout = { timeout_in_ms / 1000,
                                  (timeout_in_ms % 1000) * 1000 };
  return select(
    0,  // This is nfds, but is unused so the value doesn't matter.
    &poll_fds.read_fds,
    &poll_fds.write_fds,
    &poll_fds.except_fds,

    // -1 from caller means to block w/o timeout; NULL to select means the same.
    timeout_in_ms == -1 ? NULL : &timeout);  
}

// windows version
static PollMode poll_fds_mode(int sock, int index) {
  PollMode poll_mode = 0;
  if (FD_ISSET(sock, &poll_fds.read_fds))   poll_mode |= poll_mode_read;
  if (FD_ISSET(sock, &poll_fds.write_fds))  poll_mode |= poll_mode_write;
  if (FD_ISSET(sock, &poll_fds.except_fds)) poll_mode |= poll_mode_err;
  return poll_mode;
}

#endif

// Windows has dependencies around the order of included header files making
// the code simpler if we include msgbox_now here rather than before the
// windows-specific section.
#include "msgbox_now.h"


///////////////////////////////////////////////////////////////////////////////
//  Future work.

// **. By default, all data objects sent to callbacks are dynamic and owned by
//     us. We free them when the callback returns. Add a way for msgbox to
//     retain ownership of a data buffer so that buffer doesn't have to
//     be copied by the user. This doesn't have to be in v1.
//
// **. Try to eliminate unnamed function call parameters.
//
// **. Make the address info in msg_Conn an official Address object.
//
// **. Add timeouts for msg_get.
//
// **. When we receive a request, ensure that next_reply_id is above its
//     reply_id. (This would be in read_from_socket.)
//
// **. Avoid blocking in send_all; instead cache data that needs to be sent and
//     dole it out from the run loop. In practice, I can track how often this
//     ends up as a problem (maybe how often send returns 0) and use that to
//     prioritize things.
//
// **. Clean up use of num_bytes in the header for udp, as it is not used
//     consistently now.
//
// **. Encapsulate out all the msg_Data handling. There's too much coupling with
//     it now.
//
// **. Better behavior when a connect is attempted to an unavailable server.
//
// **. We currently send a tcp packet to indicate closure; modify this to use
//     the standard tcp closing protocal - i.e. getting a 0 back from a valid
//     recv call.
//
// **. In read_header, be able to handle a partial head read
//     (tcp only, I believe).


#define true 1
#define false 0

// This is a possible return value for functions that return
// an error string when an error is encountered.
#define no_error NULL

#define sock_in_size sizeof(struct sockaddr_in)

static msg_Data msg_no_data = { .num_bytes = 0, .bytes = NULL };

typedef struct {
  msg_Conn *conn;
  msg_Event event;
  msg_Data data;
  void *to_free;
  const char *set_name;
} PendingCall;

#define free_nothing NULL
#define no_set_name NULL

static Array immediate_callbacks = NULL;

// Possible values for message_type.
enum {
  msg_type_one_way,
  msg_type_request,
  msg_type_reply,
  msg_type_heartbeat,
  msg_type_close
};

typedef struct {
  uint16_t message_type;
  uint16_t reply_id;
  uint32_t num_bytes;
} Header;

#define header_len (sizeof(Header))

typedef struct {
  uint32_t ip;    // Stored in network byte-order.
  uint16_t port;  // Stored in host    byte-order.
  uint16_t protocol_type;
} Address;

// Metadata is the preamble for a msg_Data buffer.
// The non-header fields are used by listening udp sockets,
// for which we must hold state across many remotes.
typedef struct {
  void *  reply_context;
  Address remote_address;
  Header  header;
} Metadata;

#define metadata_len (sizeof(Metadata))


///////////////////////////////////////////////////////////////////////////////
//  Connection status map.

Address *address_of_conn(msg_Conn *conn) {
  return (Address *)(&conn->remote_ip);
}

char *address_as_str(Address *address) {
  struct in_addr in;
  in.s_addr = address->ip;
  static char address_str[32];
  char *protocol = address->protocol_type == msg_udp ? "udp" : "tcp";
  snprintf(address_str, 32, "%s://%s:%d",
           protocol, inet_ntoa(in), address->port);
  return address_str;
}

int address_hash(void *address) {
  char *bytes = (char *)address;
  int hash = 0;
  for (int i = 0; i < sizeof(Address); ++i) {
    hash *= 234;
    hash += bytes[i];
  }
  return hash;
}

int address_eq(void *addr1, void *addr2) {
  return memcmp(addr1, addr2, sizeof(Address)) == 0;
}

int reply_id_hash(void *reply_id) {
  return (int)(intptr_t)reply_id;
}

int reply_id_eq(void *reply_id1, void *reply_id2) {
  uint16_t id1 = (intptr_t)reply_id1;
  uint16_t id2 = (intptr_t)reply_id2;
  return id1 == id2;
}

typedef struct {
  double   last_seen_at;
  Map      reply_contexts;  // Map reply_id -> reply_context.
  void *   conn_context;    // Useful for listening udp conns.
  uint16_t next_reply_id;
  Address  remote_address;

  // These overlap; waiting_buffer is a suffix of total_buffer.
  msg_Data total_buffer;
  msg_Data waiting_buffer;
} ConnStatus;

ConnStatus *new_conn_status(double now, Address *address) {
  ConnStatus *status     = dbgcheck__calloc(sizeof(ConnStatus), "ConnStatus");
  status->last_seen_at   = now;
  status->reply_contexts = map__new(reply_id_hash, reply_id_eq);
  status->next_reply_id  = 1;
  status->remote_address = *address;
  return status;
}

static void new_conn_status_buffer(ConnStatus *status, Header *header) {
  status->total_buffer = status->waiting_buffer =
      msg_new_data_space(header->num_bytes);
  memcpy(status->total_buffer.bytes - header_len, header, header_len);
}

static void delete_conn_status_buffer(ConnStatus *status) {
  msg_delete_data(status->total_buffer);
  status->total_buffer = status->waiting_buffer =
      (msg_Data) { .num_bytes = 0, .bytes = NULL };
}

static void delete_conn_status(void *status_v_ptr, void *context) {
  ConnStatus *status = (ConnStatus *)status_v_ptr;
  // This should be empty since we need to give the user a chance to free all
  // contexts.
  assert(status->reply_contexts->count == 0);
  map__delete(status->reply_contexts);
  // TODO Should we delete the ConnStatus itself here?
  // If yes, do it. Otherwise leave a comment explaining why not.
}

// This maps Address -> ConnStatus.
// The actual keys & values are pointers to those types,
// and the releasers free them.
// TODO Once heartbeats is added, let heartbeats own the ConnStatus objects.
static Map conn_status = NULL;

// Returns NULL if the given remote address has no associated status.
ConnStatus *status_of_conn(msg_Conn *conn) {
  Address *address = (Address *)(&conn->remote_ip);
  map__key_value *pair = map__get(conn_status, address);
  return pair ? (ConnStatus *)pair->value : NULL;
}


///////////////////////////////////////////////////////////////////////////////
//  Timeout functionality.

#define udp_timeout_sec 1

typedef struct {
  double      at;
  msg_Conn *  conn;
  ConnStatus *status;
  uint16_t    reply_id;
} Timeout;

static Array timeouts = NULL;  // Items have type Timeout.

#define make_timeout(a, c, s, r) \
    ((Timeout){ .at = a, .conn = c, .status = s, .reply_id = r })

static void add_timeout(msg_Conn *conn, ConnStatus *status, uint16_t reply_id) {
  // This is called from msg_get, which takes responsibility for making sure
  // status exists.
  double timeout_at = now() + udp_timeout_sec;
  array__new_val(timeouts, Timeout) = make_timeout(timeout_at, conn,
                                                   status, reply_id);
}

// Remove the given timeout if it can be found.
static void remove_timeout(ConnStatus *status, uint16_t reply_id) {
  array__for(Timeout *, timeout, timeouts, i) {
    if (timeout->status != status || timeout->reply_id != reply_id) continue;
    // At this point, we've found the given timeout.
    array__remove_item(timeouts, timeout);
    break;
  }
}


///////////////////////////////////////////////////////////////////////////////
//  Debugging functions.

static void print_bytes(char *bytes, size_t num_to_print) {
  printf("bytes (%zd) :", num_to_print);
  for (size_t i = 0; i < num_to_print; ++i) {
    printf(" 0x%02X", bytes[i]);
  }
  printf("\n");
}

// This is purposefully *not* static, so it may be called externally by
// programs that know of it. It only gives useful data when msgbox is
// compiled with DEBUG defined.
int net_allocs_for_class(int class) { return net_allocs[class]; }


///////////////////////////////////////////////////////////////////////////////
//  Internal functions.

static void set_sockaddr_for_conn(struct sockaddr_in *sockaddr,
                                  msg_Conn *conn) {
  memset(sockaddr, 0, sock_in_size);
  sockaddr->sin_family = AF_INET;
  sockaddr->sin_port = htons(conn->remote_port);
  sockaddr->sin_addr.s_addr = conn->remote_ip;
}

// Returns -1 on error; 0 on success, similar to a system call.
static int send_all(int socket, msg_Data data) {
  data.bytes     -= header_len;
  data.num_bytes += header_len;

  while (data.num_bytes > 0) {
    long just_sent = send(socket, data.bytes, data.num_bytes, send_flags);
    if (just_sent == -1 && get_errno() == err_would_block) continue;
    if (just_sent == -1) return -1;
    data.bytes     += just_sent;
    data.num_bytes -= just_sent;
  }

  return 0;
}

// Returns no_error (NULL) on success;
// returns the name of the failing system call on error,
// and get_errno() returns the error code.
static char *send_data(msg_Conn *conn, msg_Data data) {
  if (conn->protocol_type == msg_tcp) {
    return send_all(conn->socket, data) ? "send" : no_error;
  }

  // At this point we expect protocol_type to be udp.
  if (conn->for_listening) {
    struct sockaddr_in sockaddr;
    set_sockaddr_for_conn(&sockaddr, conn);
    long bytes_sent = sendto(conn->socket,
        data.bytes - header_len, data.num_bytes + header_len, send_flags,
        (struct sockaddr *)&sockaddr, sock_in_size);
    if (bytes_sent == -1) return "sendto";
  } else {
    long bytes_sent = send(conn->socket,
        data.bytes - header_len, data.num_bytes + header_len, send_flags);
    if (bytes_sent == -1) return "send";
  }
  return no_error;
}

static void array__remove_last(Array array) {
  array__remove_item(array, array__item_ptr(array, array->count - 1));
}

// This releases array[index], sets array[index] = array[count - 1], and
// shortens the array by 1. Effectively, it quickly removes an element.
static void array__remove_and_fill(Array array, int index) {
  void *elt = array__item_ptr(array, index);
  void *last_elt = array__item_ptr(array, array->count - 1);
  if (array->releaser) array->releaser(elt, NULL);
  if (elt != last_elt) memcpy(elt, last_elt, array->item_size);
  array->count--;
}

static msg_Conn *new_connection(void *conn_context, msg_Callback callback) {
  msg_Conn *conn = dbgcheck__malloc(sizeof(msg_Conn), "msg_Conn");
  memset(conn, 0, sizeof(msg_Conn));
  conn->conn_context = conn_context;
  conn->callback = callback;
  return conn;
}

static void address_releaser(void *address_vp, void *context) {
  dbgcheck__free(address_vp, "Address");
}

static void init_if_needed() {
  static int init_done = false;
  if (init_done) return;

  library_init;

  immediate_callbacks = array__new(16, sizeof(PendingCall));
  conns    = array__new(8, sizeof(msg_Conn *));
  removals = array__new(8, sizeof(int));
  timeouts = array__new(8, sizeof(Timeout));
  init_poll_fds();

  conn_status = map__new(address_hash, address_eq);
  conn_status->key_releaser   = address_releaser;
  conn_status->value_releaser = delete_conn_status;

  init_done = true;
}

static void send_callback(msg_Conn *conn, msg_Event event, msg_Data data,
                          void *to_free, const char *set_name) {
  PendingCall pending_callback = {
    .conn = conn,
    .event = event,
    .data = { data.num_bytes, data.bytes },
    .to_free = to_free,
    .set_name = set_name };
  array__add_item_val(immediate_callbacks, pending_callback);
}

static void send_callback_error(msg_Conn *conn, const char *msg,
                                void *to_free, const char *set_name) {
  send_callback(conn, msg_error, msg_new_data(msg), to_free, set_name);
}

static void send_callback_os_error(msg_Conn *conn, const char *msg,
                                   void *to_free, const char *set_name) {
  static char err_msg[1024];
  snprintf(err_msg, 1024, "%s: %s", msg, err_str());
  send_callback_error(conn, err_msg, to_free, set_name);
}

static void make_call(PendingCall *call) {
  msg_Conn *   conn   = call->conn;
  ConnStatus * status = NULL;  // We'll set this if needed in the udp case.

  char *addr_str = "<uninitialized address>";

  // Copy metadata from msg_Data/status to msg_Conn for udp messages.
  if (conn->protocol_type == msg_udp && call->data.bytes) {
    msg_Data data          = call->data;
    Metadata *metadata     = (Metadata *)(data.bytes - metadata_len);
    conn->reply_context    = metadata->reply_context;
    *address_of_conn(conn) = metadata->remote_address;
    status                 = status_of_conn(conn);

    if (verbosity >= 3) {
      addr_str = address_as_str(&metadata->remote_address);
    }

    // Unless this is a msg_error, we expect a udp callback to have a status.
    assert(call->event == msg_error || status);
    if (status) {
      if (verbosity >= 3) {
        printf("<pid %d> restoring conn_context=%p for address %s "
               "(status=%p)\n",
               getpid(), status->conn_context, addr_str, status);
      }
      conn->conn_context = status->conn_context;
    } else if (verbosity >= 3) {
      printf("<pid %d> no status to restore conn_context from; address=%s\n",
          getpid(), addr_str);
    }
  }

  conn->callback(conn, call->event, call->data);

  // Save the user's conn_context in case they changed it.
  if (conn->protocol_type == msg_udp && status) {
    status->conn_context = conn->conn_context;
    if (verbosity >= 3) {
      printf("<pid %d> saving conn_context=%p for address %s (status=%p)\n",
          getpid(), conn->conn_context, addr_str, status);
    }
  }

  if (call->data.bytes) msg_delete_data(call->data);
  if (call->to_free) dbgcheck__free(call->to_free, call->set_name);
}

// Returns no_error (NULL) on success, and sets the protocol_type,
// remote_ip, and remote_port of the given conn.
// Returns an error string if there was an error.
static const char *parse_address_str(const char *address, msg_Conn *conn) {
  assert(conn != NULL);
  static char err_msg[1024];

  // TODO once v1 functionality is done, see if I can
  // encapsulate the error pattern into a one-liner; eg with a macro.

  // Parse the protocol type; either tcp or udp.
  const char *tcp_prefix = "tcp://";
  const char *udp_prefix = "udp://";
  size_t prefix_len = 6;
  if (strncmp(address, tcp_prefix, prefix_len) == 0) {
    conn->protocol_type = SOCK_STREAM;
  } else if (strncmp(address, udp_prefix, prefix_len) == 0) {
    conn->protocol_type = SOCK_DGRAM;
  } else {
    // The address has no recognizable prefix.
    snprintf(err_msg, 1024, "Failing due to unrecognized prefix: %s", address);
    return err_msg;
  }

  // Parse the ip substring in three steps.
  // 1. Find the start and end of the ip substring.
  const char *ip_start = address + prefix_len;
  char *colon = strchr(ip_start, ':');
  if (colon == NULL) {
    snprintf(err_msg, 1024,
             "Can't parse address '%s'; missing colon after ip", address);
    return err_msg;
  }

  // 2. Check substring length and copy over so we can hand inet_pton a
  // null-terminated version.
  long ip_len = colon - ip_start;
  if (ip_len > 15 || ip_len < 1) {
    snprintf(err_msg, 1024,
        "Failing because ip length=%ld; expected to be 1-15 (in addresss '%s')",
        ip_len, address);
    return err_msg;
  }
  char ip_str[16];
  char *ip_str_end = stpncpy(ip_str, ip_start, ip_len);
  *ip_str_end = '\0';

  // 3. Let inet_pton handle the actual parsing.
  if (strcmp(ip_str, "*") == 0) {
    conn->remote_ip = INADDR_ANY;
  } else {
    struct in_addr ip;
    if (inet_pton(AF_INET, ip_str, &ip) != 1) {
      snprintf(err_msg, 1024, "Couldn't parse ip string '%s'.", ip_str);
      return err_msg;
    }
    conn->remote_ip = ip.s_addr;
  }

  // Parse the port.
  int base_ten = 10;
  char *end_ptr = NULL;
  if (*(colon + 1) == '\0') {
    snprintf(err_msg, 1024, "Empty port string in address '%s'", address);
    return err_msg;
  }
  conn->remote_port = (int)strtol(colon + 1, &end_ptr, base_ten);
  if (*end_ptr != '\0') {
    snprintf(err_msg, 1024, "Invalid port string in address '%s'", address);
    return err_msg;
  }

  return no_error;
}

static void set_header(msg_Data data,
                       uint16_t msg_type,
                       uint16_t reply_id,
                       uint32_t num_bytes) {

  Header *header = (Header *)(data.bytes - header_len);
  *header = (Header) {
    .message_type = htons(msg_type),
    .reply_id     = htons(reply_id),
    .num_bytes    = htonl(num_bytes) };
}

static void remove_conn_at(int index) {
  array__remove_and_fill(conns, index);
  if (index < conns->count) {
    msg_Conn *filled_conn = array__item_val(conns, index, msg_Conn *);
    filled_conn->index = index;
  }

  remove_from_poll_fds(index);
}

// Drops the conn from conn_status and sends the given event, which
// should be one of msg_connection_{closed,lost}.
static void local_disconnect(msg_Conn *conn, msg_Event event) {
  Address *address = (Address *)(&conn->remote_ip);
  map__unset(conn_status, address);

  // A listening udp conn is a special case as it lives until an unlisten call.
  int is_listening_udp = (conn->for_listening &&
                          conn->protocol_type == msg_udp);

  void *to_free = is_listening_udp ? NULL : conn;
  const char *set_name = is_listening_udp ? NULL : "msg_Conn";
  send_callback(conn, event, msg_no_data, to_free, set_name);

  if (is_listening_udp) return;

  closesocket(conn->socket);
  array__add_item_val(removals, conn->index);
}

// Reads the header of a message.
// For udp packets, the next recv will still include the header.
// For tcp packets, the next recv will be just after the header.
// Returns true on success; false on failure.
static int read_header(int sock, msg_Conn *conn, Header *header) {
  // A (char *) header pointer works for all versions of recv, which take
  // either char * or void *.
  long bytes_recvd = recv(sock, (char *)header, header_len, MSG_PEEK);

  // In some cases, a tcp message header may be cut off, so we want to
  // asynchronously wait. Poor header.
  if (bytes_recvd > 0 && bytes_recvd < header_len) return false;

  if (bytes_recvd == 0 ||
      (bytes_recvd == -1 && get_errno() == err_conn_reset)) {
    local_disconnect(conn, msg_connection_lost);
    return false;
  }
  
  // We ignore err_win_msg_size; it only tells us that we didn't get the full
  // udp message.
  if (bytes_recvd == -1 && get_errno() != err_win_msg_size) {
    if (get_errno() == err_would_block) return false;
    send_callback_os_error(conn, "recv", free_nothing, no_set_name);
    return false;
  }
  
  // Mark the header as read in the tcp case.
  if (conn->protocol_type == msg_tcp) {
    int default_options = 0;
    recv(sock, (char *)header, header_len, default_options);
  }

  // Convert each field from network to host byte ordering.
  header->message_type = ntohs(header->message_type);
  header->reply_id     = ntohs(header->reply_id);
  header->num_bytes    = ntohl(header->num_bytes);

  conn->reply_id       = header->reply_id;

  if (false) {
    printf("%s called; header has ", __FUNCTION__);
    print_bytes((char *)header, sizeof(Header));
  }

  if (false) {
    char *type_names[] = {
      "msg_type_one_way",
      "msg_type_request",
      "msg_type_reply",
      "msg_type_heartbeat",
      "msg_type_close"
    };
    printf("pid %d: Read in a header: type=%s #bytes=%d\n",
           getpid(),
           type_names[header->message_type],
           header->num_bytes);
  }

  return true;
}

// This creates a new ConnStatus struct if none exists for the remote address.
static ConnStatus *remote_address_seen(msg_Conn *conn) {

  ConnStatus *status = status_of_conn(conn);

  if (status == NULL) {
    // It's a new remote address; set up a new owned Address.
    status = new_conn_status(0.0, address_of_conn(conn));  // TODO set to now.

    status->conn_context = conn->conn_context;

    Address *address = dbgcheck__malloc(sizeof(Address), "Address");
    *address = *address_of_conn(conn);

    map__set(conn_status, address, status);

    // Send in the correct remote address with the callback.
    msg_Data data = msg_new_data_space(0);
    Metadata *metadata = (Metadata *)(data.bytes - metadata_len);
    metadata->remote_address = *address;

    send_callback(conn, msg_connection_ready, data, free_nothing, no_set_name);
  }

  // TODO Update the timing data for this remote address.

  return status;
}

// Returns true when the entire message is received;
// returns false when more data remains but no error occurred;
// returns -1 when there was an error - the caller must respond to it;
// returns -2 when a message was interrupted by a connection close.
static int continue_recv(msg_Conn *conn, ConnStatus *status) {
  int sock = conn->socket;
  msg_Data *buffer = &status->waiting_buffer;
  int default_options = 0;
  long bytes_in = recv(sock, buffer->bytes, buffer->num_bytes, default_options);
  if (bytes_in == 0 || (bytes_in == -1 && get_errno() == err_conn_reset)) {
    local_disconnect(conn, msg_connection_lost);
    return -2;
  }
  if (bytes_in == -1) {
    if (get_errno() == err_would_block) return false;  // More data is pending.
    // This is an error we must report; treat the current data as lost.
    return -1;
  } 

  buffer->bytes     += bytes_in;
  buffer->num_bytes -= bytes_in;
  return buffer->num_bytes == 0;
}

// Returns true iff the caller may immediately call this again with the same
// parameters to check for additional messages waiting in the socket.
// TODO Make this function shorter or break it up.
static int read_from_socket(int sock, msg_Conn *conn) {
  if (verbosity >= 1) {
    fprintf(stderr, "%s(%d, %s)\n",
            __FUNCTION__, sock, address_as_str(address_of_conn(conn)));
  }
  ConnStatus *status = NULL;
  Header *header = NULL;
  Metadata *metadata = NULL;
  msg_Data data;

  // Read in any tcp data.
  if (conn->protocol_type == msg_tcp) {

    if (conn->for_listening) {

      // Accept a new incoming connection.
      struct sockaddr_in remote_addr;
      socklen_t addr_len = sizeof(remote_addr);
      int new_sock = accept(conn->socket,
                            (struct sockaddr *)&remote_addr, &addr_len);
      if (new_sock == -1) {
        if (get_errno() == err_would_block) return false;
        send_callback_os_error(conn, "accept", free_nothing, no_set_name);
        return false;
      }
      
      if (avoid_sigpipe(new_sock) != 0) {
        send_callback_os_error(conn, "setsockopt", free_nothing, no_set_name);
        return false;
      }

      const char *failing_fn = make_non_blocking(new_sock);
      if (failing_fn) {
        send_callback_os_error(conn, failing_fn, free_nothing, no_set_name);
        return false;
      }

      msg_Conn *new_conn      = new_connection(conn->conn_context,
                                               conn->callback);
      new_conn->socket        = new_sock;
      new_conn->remote_ip     = remote_addr.sin_addr.s_addr;
      new_conn->remote_port   = ntohs(remote_addr.sin_port);
      new_conn->protocol_type = conn->protocol_type;
      new_conn->index         = conns->count;
      array__add_item_val(conns, new_conn);

      add_to_poll_fds(new_sock, poll_mode_read);

      // This sets up a ConnStatus and sends msg_connection_ready.
      remote_address_seen(new_conn);
      return false;
    }

    status = remote_address_seen(conn);
    if (status->waiting_buffer.num_bytes == 0) {

      // Begin a new recv.
      header = alloca(sizeof(Header));
      if (!read_header(sock, conn, header)) return false;
      if (header->message_type == msg_type_close) {
        local_disconnect(conn, msg_connection_closed);
        return false;
      }
      new_conn_status_buffer(status, header);
    } else {

      // Load header from the buffer we'll continue.
      header = (Header *)(status->total_buffer.bytes - header_len);
    }
    int ret_val = continue_recv(conn, status);
    if (ret_val == -2) return false;  // The message was interrupted by a close.
    if (ret_val == -1) {
      send_callback_os_error(conn, "recv", free_nothing, no_set_name);
      delete_conn_status_buffer(status);
      return false;
    }
    if (ret_val == false) return false;  // It will finish later.
    data = status->total_buffer;

    if (0) {
      printf("After continue_recv, data has ");
      print_bytes(data.bytes, data.num_bytes);
    }

    status->total_buffer = status->waiting_buffer =
        (msg_Data) { .num_bytes = 0, .bytes = NULL };

  } else {

    // New udp message: read the header.
    header = alloca(sizeof(Header));
    if (!read_header(sock, conn, header)) return false;
  }

  if (verbosity >= 2) {  // Debug code.
    char *msg_type_str[] = {
      "msg_type_one_way",
      "msg_type_request",
      "msg_type_reply",
      "msg_type_heartbeat",
      "msg_type_close"
    };
    if (header->message_type < (sizeof(msg_type_str) / sizeof(char *))) {
      printf("Received message of type '%s'.\n",
             msg_type_str[header->message_type]);
    } else {
      printf("Received message of unknown type %d.\n",
             header->message_type);
    }
  }

  // Set up the appropriate reaction event.
  msg_Event event;
  switch (header->message_type) {
    case msg_type_one_way:
      event = msg_message;
      // Avoid confusion about whether or not this is a reply.
      conn->reply_id = 0;
      break;
    case msg_type_request:
      event = msg_request;
      break;
    case msg_type_reply:
      event = msg_reply;
      break;
    case msg_type_heartbeat:
      assert(0);
      break;
    case msg_type_close:
      if (conn->protocol_type == msg_tcp) msg_delete_data(data);
      local_disconnect(conn, msg_connection_closed);
      return false;
  }

  // Read in any udp data.
  if (conn->protocol_type == msg_udp) {
    data = msg_new_data_space(header->num_bytes);
    struct sockaddr_in remote_sockaddr;
    socklen_t remote_sockaddr_size = sock_in_size;
    int default_options = 0;
    long bytes_recvd = recvfrom(sock, data.bytes - header_len,
        data.num_bytes + header_len, default_options,
        (struct sockaddr *)&remote_sockaddr, &remote_sockaddr_size);

    if (bytes_recvd == -1) {
      send_callback_os_error(conn, "recvfrom", free_nothing, no_set_name);
      return false;
    }

    // We don't save the current conn_context because the user may have
    // reasonably changed the remote address without changing the conn_context
    // in order to send a message from the same socket - specifically, this is
    // tricky for a listening udp socket. So we don't know at this point that
    // conn_context is correctly associated with the conn's current remote
    // address.

    // Save this data's status with the data itself, since this is udp.
    conn->remote_ip = remote_sockaddr.sin_addr.s_addr;
    conn->remote_port = ntohs(remote_sockaddr.sin_port);
    status = remote_address_seen(conn);

    metadata = (Metadata *)(data.bytes - metadata_len);
    metadata->reply_context = NULL;  // reply_context is set for replies below.
    metadata->remote_address = *address_of_conn(conn);
  }

  // Look up a reply_context if it's a reply.
  if (header->message_type == msg_type_reply) {
    void *reply_id_key = (void *)(intptr_t)header->reply_id;
    map__key_value *pair = map__get(status->reply_contexts, reply_id_key);
    if (pair == NULL) {
      send_callback_error(
          conn,
          "Unrecognized reply_id",
          data.bytes - metadata_len,  // Pointer to free.
          "msg_Data bytes");          // Set name for dbgcheck free.
      return false;
    }
    remove_timeout(status, header->reply_id);
    conn->reply_context = pair->value;
    if (metadata) metadata->reply_context = pair->value;  // The udp case.
    map__unset(status->reply_contexts, reply_id_key);
    // Clear reply_id so a nested msg_send isn't interpreted as a reply itself.
    conn->reply_id = 0;
  } else {
    conn->reply_context = NULL;
  }

  send_callback(conn, event, data, free_nothing, no_set_name);
  return true;
}

// Sets up sockaddr based on address. If an error occurs, the error callback
// is scheduled.  Returns true on success. conn and its polling socket are
// added to the conns and poll_fds data structures on success.
static int setup_sockaddr(struct sockaddr_in *sockaddr,
                          const char *address, msg_Conn *conn) {
  const char *err_msg = parse_address_str(address, conn);
  if (err_msg) {
    send_callback_error(conn, err_msg, conn, "msg_Conn");
    return false;
  }

  int use_default_protocol = 0;
  int sock = socket(AF_INET, conn->protocol_type, use_default_protocol);
  if (sock == -1) {
    send_callback_os_error(conn, "socket", conn, "msg_Conn");
    return false;
  }

  // We have a real socket, so add entries to both poll_fds and conns.
  conn->socket = sock;
  conn->index  = conns->count;
  array__add_item_val(conns, conn);

  add_to_poll_fds(sock, poll_mode_read);

  // Initialize the sockaddr_in struct.
  memset(sockaddr, 0, sock_in_size);
  sockaddr->sin_family = AF_INET;
  sockaddr->sin_port = htons(conn->remote_port);
  sockaddr->sin_addr.s_addr = conn->remote_ip;

  return true;
}

typedef int (ms_call_conv *SocketOpener)(socket_t,
                                         const struct sockaddr *,
                                         socklen_t);

static void open_socket(const char *address, void *conn_context,
    msg_Callback callback, int for_listening) {
  init_if_needed();

  msg_Conn *conn = new_connection(conn_context, callback);
  conn->for_listening = for_listening;
  struct sockaddr_in *sockaddr = alloca(sock_in_size);
  if (!setup_sockaddr(sockaddr, address, conn)) {
    return;  // Error; setup_sockaddr now owns conn.
  }

  // Make the socket non-blocking so a connect call won't block.
  const char *failing_fn = make_non_blocking(conn->socket);
  if (failing_fn) {
    send_callback_os_error(conn, failing_fn, conn, "msg_Conn");
    return remove_last_polling_conn();
  }

  // On tcp, turn on SO_REUSEADDR for easier server restarts.
  if (conn->protocol_type == msg_tcp) {
    int optval = 1;
    // Send (char *)&optval as windows takes a char*; mac/linux takes a void*.
    setsockopt(conn->socket, SOL_SOCKET, SO_REUSEADDR,
               (char *)&optval, sizeof(optval));
  }

  char *sys_call_name = for_listening ? "bind" : "connect";
  SocketOpener sys_open_sock = for_listening ? bind : connect;
  int ret_val = sys_open_sock(conn->socket,
                              (struct sockaddr *)sockaddr,
                              sock_in_size);
  if (ret_val == -1) {
    int in_progress = (get_errno() == err_in_progress ||
                       get_errno() == err_would_block);
    if (!for_listening && conn->protocol_type == msg_tcp && in_progress) {
      // Being in progress is ok in this case; we'll send
      // msg_connection_ready later.
      set_conn_to_poll_mode(conns->count - 1, poll_mode_write);
      return;
    }
    send_callback_os_error(conn, sys_call_name, conn, "msg_Conn");
    return remove_last_polling_conn();
  }

  if (for_listening) {
    if (conn->protocol_type == msg_tcp) {
      ret_val = listen(conn->socket, SOMAXCONN);
      if (ret_val == -1) {
        send_callback_os_error(conn, "listen", conn, "msg_Conn");
        return remove_last_polling_conn();
      }
    }
    send_callback(conn, msg_listening, msg_no_data, free_nothing, no_set_name);
  } else {
    remote_address_seen(conn);  // Sends the msg_connection_ready event.
  }
}


///////////////////////////////////////////////////////////////////////////////
//  Public functions.

void msg_runloop(int timeout_in_ms) {
  init_if_needed();

  // Don't delay pending calls.
  if (immediate_callbacks->count) { timeout_in_ms = 0; }

  // Clear any conns marked for removal. Public functions work this way so
  // they behave well if called by user functions invoked as callbacks.
  array__for(int *, index, removals, i) remove_conn_at(*index);
  array__clear(removals);
  nfds_t num_fds = conns->count;

  // Begin debug code.
  if (verbosity >= 1) {
    static char last_poll_state[4096];
    char poll_state[4096];
    if (num_fds == 0) {
      strncpy(poll_state, "<nothing to poll>\n", 4096);
    } else {
      char *s = poll_state;
      char *s_end = s + 4096;
      s += snprintf(s, s_end - s, "Polling %d socket%s:\n",
                    (int)num_fds, num_fds > 1 ? "s" : "");
      s += snprintf(s, s_end - s, "  %-5s %-25s %-5s %s\n",
                    "sock", "address", "type", "listening?");
      array__for(msg_Conn **, conn_ptr, conns, i) {
        msg_Conn *conn = *conn_ptr;
        int   sock     = conn->socket;
        char *address  = address_as_str(address_of_conn(conn));
        char *type_str = conn->protocol_type == msg_tcp ? "tcp" : "udp";
        char *listn    = conn->for_listening ? "yes" : "no";
        s += snprintf(s, s_end - s, "  %-5d %-25s %-5s %s\n",
                      sock, address, type_str, listn);
      }
    }
    if (strcmp(last_poll_state, poll_state) != 0) printf("%s", poll_state);
    strncpy(last_poll_state, poll_state, 4096);
  }
  // End debug code.

  int ret = 0;
  if (num_fds) ret = check_poll_fds(timeout_in_ms);

  if (ret == -1) {
    // It's difficult to send a standard error callback to the user here because
    // we don't know which connection (and therefore which callback pointer)
    // to use; also, critical errors should only happen here due to bugs in
    // msgbox itself.

    if (get_errno() != err_intr && get_errno() != err_in_progress) {
      // This error case can theoretically only be my fault; still, let the
      // user know.
      fprintf(stderr, "Internal msgbox error during '%s' call: %s\n",
              poll_fn_name, err_str());
    }
  } else if (ret > 0) {
    array__for(msg_Conn **, conn_ptr, conns, i) {
      msg_Conn *conn = *conn_ptr;
      PollMode poll_mode = poll_fds_mode(conn->socket, i);

      // I'm including these since I'm not sure how important they are to track.
      if (verbosity >= 1) {
        if (poll_mode & poll_mode_err) {
          fprintf(stderr,
                  "Error response from socket %d on poll or select call.\n",
                  conn->socket);
        }
      }
      if (poll_mode & poll_mode_err) {
        int error;
        socklen_t error_len = sizeof(error);
        // Send in (char *)&error as windows takes type char*; mac/linux
        // takes type void*.
        getsockopt(conn->socket, SOL_SOCKET,
                   SO_ERROR, (char *)&error, &error_len);
        if (error == err_conn_refused || error == err_timed_out) {
          array__add_item_val(removals, conn->index);
          set_errno(error);
          send_callback_os_error(conn, "connect", conn, "msg_Conn");
          continue;
        }
        // When the error is neither err_conn_refused nor err_timed_out, then
        // we let the code continue as we may get something useful out of a
        // possible poll_mode_read bit. For example, the error may have been
        // from trying to send something to a remotely closed connection.
      }
      if (poll_mode & poll_mode_write) {
        // We only listen for this event when waiting for a tcp connect to
        // complete.
        remote_address_seen(conn);  // Sends msg_connection_ready.
        set_conn_to_poll_mode(i, poll_mode_read);
      }
      if (poll_mode & poll_mode_read) {
        // TODO Why are the two params to read_from_socket separate, since
        //      conn->socket should always = the given fd?
        while ((read_from_socket(conn->socket, conn)));
      }
    }
    array__for(int *, index, removals, i) remove_conn_at(*index);
    array__clear(removals);
  }

  // Check for any unreplied-to udp requests that have timed out.
  double time_now = now();
  array__for(Timeout *, timeout, timeouts, i) {
    // The timeouts are soonest-first; stop as soon as one is not in the past.
    if (timeout->at > time_now) break;

    // Remove the pending status information and inform the user of the timeout.
    void *reply_id_key = (void *)(intptr_t)timeout->reply_id;
    map__key_value *pair = map__get(timeout->status->reply_contexts,
                                    reply_id_key);
    // Since we set up the timeout ourselves, it should exist in the status.
    assert(pair);
    msg_Conn *conn = timeout->conn;  // Save conn as timeout will soon be freed.
    conn->reply_context = pair->value;
    map__unset(timeout->status->reply_contexts, reply_id_key);
    array__remove_item(timeouts, timeout);
    i--;  // Back up one item so the next iteration gets the next item.
    const char *msg = (conn->protocol_type == msg_tcp ? "tcp get timed out" :
                                                        "udp get timed out");
    
    // Set up metadata as it overrides data in conn within make_call.
    msg_Data data = msg_new_data(msg);
    Metadata *metadata = (Metadata *)(data.bytes - metadata_len);
    metadata->reply_context  = conn->reply_context;
    metadata->remote_address = timeout->status->remote_address;

    send_callback(conn, msg_error, data, free_nothing, no_set_name);
  }

  // Save the state of pending callbacks so that users can add new callbacks
  // from within their callbacks.
  Array saved_immediate_callbacks = immediate_callbacks;
  immediate_callbacks = array__new(16, sizeof(PendingCall));

  array__for(PendingCall *, call, saved_immediate_callbacks, i) {
    make_call(call);
  }

  // TODO Handle timed callbacks - such as heartbeats - and get timeouts.
  array__delete(saved_immediate_callbacks);
}

void msg_listen(const char *address, msg_Callback callback) {
  int for_listening = true;
  open_socket(address, msg_no_context, callback, for_listening);
}

void msg_connect(const char *address, msg_Callback callback,
                 void *conn_context) {
  int for_listening = false;
  open_socket(address, conn_context, callback, for_listening);
}

void msg_unlisten(msg_Conn *conn) {
  if (conn == NULL) {
    fprintf(stderr, "Error: msg_unlisten called on NULL connection.\n");
    return;
  }
  if (!conn->for_listening) {
    const char *err_str = "msg_unlisten called on non-listening connection";
    return send_callback_error(conn, err_str, free_nothing, no_set_name);
  }
  // Tell local_disconnect to free the conn object, even on udp.
  conn->for_listening = false;
  if (closesocket(conn->socket) == -1) {
    int saved_errno = get_errno();
    // TODO Make the fn name here more accurate (it's close on mac/linux and
    //      closesocket on windows).
    send_callback_os_error(conn, "close", free_nothing, no_set_name);
    if (saved_errno == err_bad_sock) {
      return;  // Don't send msg_listening_ended since it didn't.
    }
  }
  local_disconnect(conn, msg_listening_ended);
}

void msg_disconnect(msg_Conn *conn) {
  msg_Data data = msg_new_data_space(0);
  int num_bytes = 0, reply_id = 0;
  set_header(data, msg_type_close, reply_id, num_bytes);

  char *failed_sys_call = send_data(conn, data);
  if (failed_sys_call) send_callback_os_error(conn, failed_sys_call,
                                              free_nothing, no_set_name);
  msg_delete_data(data);

  local_disconnect(conn, msg_connection_closed);
}

void msg_send(msg_Conn *conn, msg_Data data) {
  // Set up the header.
  int msg_type = conn->reply_id ? msg_type_reply : msg_type_one_way;
  set_header(data, msg_type, conn->reply_id, (uint32_t)data.num_bytes);

  char *failed_sys_call = send_data(conn, data);
  if (failed_sys_call) {
    send_callback_os_error(conn, failed_sys_call, free_nothing, no_set_name);
  }
}

void msg_get(msg_Conn *conn, msg_Data data, void *reply_context) {
  // Look up the next reply id.
  ConnStatus *status = status_of_conn(conn);
  if (status == NULL) {
    static char err_msg[1024];
    snprintf(err_msg, 1024, "No known connection with %s",
             address_as_str(address_of_conn(conn)));
    return send_callback_error(conn, err_msg, free_nothing, no_set_name);
  }
  uint16_t reply_id = status->next_reply_id++;
  map__set(status->reply_contexts, (void *)(intptr_t)reply_id, reply_context);

  // Set up the header.
  set_header(data, msg_type_request, reply_id, (uint32_t)data.num_bytes);

  char *failed_sys_call = send_data(conn, data);
  if (failed_sys_call) {
    send_callback_os_error(conn, failed_sys_call, free_nothing, no_set_name);
  } else {
    add_timeout(conn, status, reply_id);
  }
}

char *msg_as_str(msg_Data data) {
  return data.bytes;
}

msg_Data msg_new_data(const char *str) {
  // Allocate room for the string with +1 for the null terminator.
  size_t data_size = strlen(str) + 1;
  msg_Data data = msg_new_data_space(data_size);
  dbgcheck__inner_ptr_size(data.bytes, data.bytes - metadata_len,
                           "msg_Data bytes", data_size);
  strncpy(data.bytes, str, data_size);
  return data;
}

msg_Data msg_new_data_space(size_t num_bytes) {
  msg_Data data = {.num_bytes = num_bytes,
                   .bytes     = dbgcheck__malloc(num_bytes + metadata_len,
                                                 "msg_Data bytes")};
  data.bytes += metadata_len;
  return data;
}

void msg_delete_data(msg_Data data) {
  dbgcheck__free(data.bytes - metadata_len, "msg_Data bytes");
}

char *msg_ip_str(msg_Conn *conn) {
  return inet_ntoa((struct in_addr) { .s_addr = conn->remote_ip});
}

char *msg_address_str(msg_Conn *conn) {
  return address_as_str(address_of_conn(conn));
}

char *msg_error_str(msg_Data data) {
  return msg_as_str(data);
}

void *msg_no_context = NULL;

const int msg_tcp = SOCK_STREAM;
const int msg_udp = SOCK_DGRAM;
