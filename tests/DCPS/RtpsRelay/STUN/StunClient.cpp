/*
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "dds/DCPS/RTPS/ICE/Stun.h"
#include "dds/DCPS/Message_Block_Ptr.h"

#include "ace/ACE.h"
#include "ace/SOCK_Dgram.h"
#include "ace/Argv_Type_Converter.h"
#include "ace/Arg_Shifter.h"

#include <cstdlib>
#include <iostream>

#ifdef OPENDDS_SECURITY

namespace {
  const OpenDDS::DCPS::Encoding encoding_unaligned_big(OpenDDS::DCPS::Encoding::KIND_CDR_UNALIGNED,
                                                     OpenDDS::DCPS::ENDIAN_BIG);
}

void generate_transaction_id(OpenDDS::STUN::Message& message)
{
  for (size_t idx = 0; idx != 12; ++idx) {
    message.transaction_id.data[idx] = rand();
  }
}

bool send(int& status,
          ACE_SOCK_Dgram& socket,
          const ACE_INET_Addr& remote,
          OpenDDS::STUN::Message& request)
{
  OpenDDS::DCPS::Message_Block_Shared_Ptr block(new ACE_Message_Block(OpenDDS::STUN::HEADER_SIZE + request.length()));
  request.block = block.get();
  OpenDDS::DCPS::Serializer serializer(block.get(), encoding_unaligned_big);
  if (!(serializer << request)) {
    std::cerr << "ERROR: Failed to serialize request" << std::endl;
    status = EXIT_FAILURE;
    return false;
  }

  const ssize_t bytes_sent = socket.send(block->rd_ptr(), block->length(), remote);
  if (bytes_sent != static_cast<ssize_t>(block->length())) {
    std::cerr << "ERROR: Failed to send: " << strerror(errno) << std::endl;
    status = EXIT_FAILURE;
    return false;
  }

  return true;
}

bool recv(int& status,
          ACE_SOCK_Dgram& socket,
          OpenDDS::STUN::Message& response)
{
  iovec iov;
  ACE_INET_Addr addr;
  const ssize_t bytes_received = socket.recv(&iov, addr);
  if (bytes_received < 0) {
    std::cerr << "ERROR: Failed to recv: " << strerror(errno) << std::endl;
    status = EXIT_FAILURE;
    return false;
  }

  const auto data_block =
    new (ACE_Allocator::instance()->malloc(sizeof(ACE_Data_Block))) ACE_Data_Block(bytes_received, ACE_Message_Block::MB_DATA,
                                                                                   static_cast<const char*>(iov.iov_base), 0, 0, 0, 0);
  OpenDDS::DCPS::Message_Block_Shared_Ptr buffer(new ACE_Message_Block(data_block));
  buffer->length(bytes_received);

  response.block = buffer.get();
  OpenDDS::DCPS::Serializer deserializer(buffer.get(), encoding_unaligned_big);
  if (!(deserializer >> response)) {
    std::cerr << "ERROR: Failed to deserialize response" << std::endl;
    status = EXIT_FAILURE;
    return false;
  }

  return true;
}

bool test_success(int& status,
                  ACE_SOCK_Dgram& socket,
                  const ACE_INET_Addr& remote)
{
  std::cerr << __func__ << std::endl;
  bool retval = true;
  OpenDDS::STUN::Message request;
  request.class_ = OpenDDS::STUN::REQUEST;
  request.method = OpenDDS::STUN::BINDING;
  generate_transaction_id(request);
  request.append_attribute(OpenDDS::STUN::make_fingerprint());

  if (!send(status, socket, remote, request)) {
    return false;
  }

  OpenDDS::STUN::Message response;
  if (!recv(status, socket, response)) {
    return false;
  }

  if (response.class_ != OpenDDS::STUN::SUCCESS_RESPONSE) {
    std::cerr << "ERROR: Incorrect message class" << std::endl;
    status = EXIT_FAILURE;
    retval = false;
  }

  if (response.method != request.method) {
    std::cerr << "ERROR: Incorrect method" << std::endl;
    status = EXIT_FAILURE;
    retval = false;
  }

  if (response.transaction_id != request.transaction_id) {
    std::cerr << "ERROR: Incorrect transaction id" << std::endl;
    status = EXIT_FAILURE;
    retval = false;
  }

  ACE_INET_Addr a;
  if (!response.get_mapped_address(a)) {
    std::cerr << "ERROR: no mapped address" << std::endl;
    status = EXIT_FAILURE;
    retval = false;
  }

  ACE_TCHAR buffer[256];
  a.addr_to_string(buffer, 256);
  std::cout << "Mapped address = " << ACE_TEXT_ALWAYS_CHAR(buffer) << std::endl;

  if (!response.has_fingerprint()) {
    std::cerr << "ERROR: no fingerprint" << std::endl;
    status = EXIT_FAILURE;
    retval = false;
  }

  return retval;
}

bool test_unknown_method(int& status,
                         ACE_SOCK_Dgram& socket,
                         const ACE_INET_Addr& remote)
{
  std::cerr << __func__ << std::endl;
  bool retval = true;
  OpenDDS::STUN::Message request;
  request.class_ = OpenDDS::STUN::REQUEST;
  request.method = static_cast<OpenDDS::STUN::Method>(2);
  generate_transaction_id(request);
  request.append_attribute(OpenDDS::STUN::make_fingerprint());

  if (!send(status, socket, remote, request)) {
    return false;
  }

  OpenDDS::STUN::Message response;
  if (!recv(status, socket, response)) {
    return false;
  }

  if (response.class_ != OpenDDS::STUN::ERROR_RESPONSE) {
    std::cerr << "ERROR: Incorrect message class" << std::endl;
    status = EXIT_FAILURE;
    retval = false;
  }

  if (response.method != request.method) {
    std::cerr << "ERROR: Incorrect method" << std::endl;
    status = EXIT_FAILURE;
    retval = false;
  }

  if (response.transaction_id != request.transaction_id) {
    std::cerr << "ERROR: Incorrect transaction id" << std::endl;
    status = EXIT_FAILURE;
    retval = false;
  }

  if (!response.has_error_code()) {
    std::cerr << "ERROR: Response does not have error code" << std::endl;
    status = EXIT_FAILURE;
    retval = false;
  }

  if (response.get_error_code() != OpenDDS::STUN::BAD_REQUEST) {
    std::cerr << "ERROR: Response has incorrect error code" << std::endl;
    status = EXIT_FAILURE;
    retval = false;
  }

  if (response.get_error_reason() != "Bad Request: Unknown method") {
    std::cerr << "ERROR: Response has incorrect error reason" << std::endl;
    status = EXIT_FAILURE;
    retval = false;
  }

  if (!response.has_fingerprint()) {
    std::cerr << "ERROR: no fingerprint" << std::endl;
    status = EXIT_FAILURE;
    retval = false;
  }

  return retval;
}

bool test_no_fingerprint(int& status,
                         ACE_SOCK_Dgram& socket,
                         const ACE_INET_Addr& remote)
{
  std::cerr << __func__ << std::endl;
  bool retval = true;
  OpenDDS::STUN::Message request;
  request.class_ = OpenDDS::STUN::REQUEST;
  request.method = OpenDDS::STUN::BINDING;
  generate_transaction_id(request);

  if (!send(status, socket, remote, request)) {
    return false;
  }

  OpenDDS::STUN::Message response;
  if (!recv(status, socket, response)) {
    return false;
  }

  if (response.class_ != OpenDDS::STUN::ERROR_RESPONSE) {
    std::cerr << "ERROR: Incorrect message class" << std::endl;
    status = EXIT_FAILURE;
    retval = false;
  }

  if (response.method != request.method) {
    std::cerr << "ERROR: Incorrect method" << std::endl;
    status = EXIT_FAILURE;
    retval = false;
  }

  if (response.transaction_id != request.transaction_id) {
    std::cerr << "ERROR: Incorrect transaction id" << std::endl;
    status = EXIT_FAILURE;
    retval = false;
  }

    if (!response.has_error_code()) {
    std::cerr << "ERROR: Response does not have error code" << std::endl;
    status = EXIT_FAILURE;
    retval = false;
  }

  if (response.get_error_code() != 400) {
    std::cerr << "ERROR: Response has incorrect error code" << std::endl;
    status = EXIT_FAILURE;
    retval = false;
  }

  if (response.get_error_reason() != "Bad Request: FINGERPRINT must be pesent") {
    std::cerr << "ERROR: Response has incorrect error reason" << std::endl;
    status = EXIT_FAILURE;
    retval = false;
  }

  if (!response.has_fingerprint()) {
    std::cerr << "ERROR: no fingerprint" << std::endl;
    status = EXIT_FAILURE;
    retval = false;
  }

  return retval;
}
#endif

int ACE_TMAIN(int argc, ACE_TCHAR *argv[])
{
  bool ipv6 = false;

  ACE_Argv_Type_Converter atc(argc, argv);
  ACE_Arg_Shifter_T<char> args(atc.get_argc(), atc.get_ASCII_argv());
  while (args.is_anything_left()) {
    const char* arg = nullptr;
    if ((arg = args.get_the_parameter("ipv6"))) {
      ipv6 = ACE_OS::atoi(arg);
      args.consume_arg();
    } else {
      args.ignore_arg();
    }
  }

  int status = EXIT_SUCCESS;
#ifdef OPENDDS_SECURITY
  ACE_INET_Addr local(0, ipv6 ? "::" : "0.0.0.0", ipv6 ? AF_INET6 : AF_INET);
  ACE_SOCK_Dgram socket(local, ipv6 ? AF_INET6 : AF_INET);

  ACE_INET_Addr remote(3478, ipv6 ? "::1" : "127.0.0.1");

  if (!test_success(status, socket, remote)) {
    std::cerr << "ERROR: test_success failed" << std::endl;
  }
  if (!test_unknown_method(status, socket, remote)) {
    std::cerr << "ERROR: test_unknown_method failed" << std::endl;
  }
  if (!test_no_fingerprint(status, socket, remote)) {
    std::cerr << "ERROR: test_no_fingerprint failed" << std::endl;
  }
#endif
  return status;
}
