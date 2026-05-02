# Changelog

All notable changes to the IPK-RDT project will be documented in this file.

## [1.0.0] - Initial Release

### Added
- **Core Networking:** Implemented basic UDP socket wrappers for both Client and Server modes.
- **Protocol Header:** Defined a 17-byte strict packed header structure (`rdt_packet`) supporting `connection_id`, sequence numbers, and dynamic payload length up to 1183 bytes.
- **CLI Parser:** Added argument parsing using `getopt` to handle `-s`, `-c`, `-p`, `-a`, `-i`, `-o`, and `-w` flags with robust error checking and mutually exclusive mode validation.
- **Handshake Mechanism:** Implemented explicit 2-way session establishment (`HELLO` -> `HELLO_ACK`) to synchronize the `connection_id`.
- **Teardown Mechanism:** Implemented safe session termination (`FINAL` -> `FINAL_ACK`) to ensure complete data transfer before exit.
- **Go-Back-N Pipelining:** Added a sliding window mechanism (Window Size = 25) for pipelined data transmission to maximize throughput over high-latency networks.
- **Data Integrity:** Integrated RFC 1071 16-bit Internet Checksum to detect and drop corrupted packets.
- **Timeout Management:** Implemented non-blocking socket waiting using `poll()` to handle local retransmission timeouts and global connection drops (`-w`).
- **File & Stream I/O:** Added support for reading/writing from/to standard streams (`stdin`/`stdout`) and standard files using C++ `fstream`.
- **Cross-Protocol Support:** Integrated `getaddrinfo` with `AF_UNSPEC` to seamlessly support both IPv4 and IPv6 environments.

### Fixed/Hardened
- Alien packet rejection: Server now strictly drops out-of-order connections and delayed packets from previous sessions using `connection_id` validation.