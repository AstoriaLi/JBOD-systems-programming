# JBOD-systems-programming
# Original repository was part of CMPSC 311 coursework


Adding a cache to your mdadm system has significantly improved its latency and reduced the load on the JBOD. Before you finish your internship, however, the company wants you to add networking support to your mdadm implementation to increase the flexibility of their system. The JBOD systems purchased by the company can accept JBOD operations over a network using a proprietary networking protocol. Specifically, a JBOD system has an embedded server component that can be configured to have an IP address and listen for JBOD operations on a specific port. In this final step of your assignment, you are going to implement a client component of this protocol that will connect to the JBOD server and execute JBOD operations over the network. As the company scales, they plan to add multiple JBOD systems to their data center. Having networking support in mdadm will allow the company to avoid downtime in case a JBOD system malfunctions, by switching to another JBOD system on the fly.

Currently, your mdadm code has multiple calls to `jbod_operation`, which issue JBOD commands to a locally attached JBOD system. In your new implementation, you will replace all calls to `jbod_operation` with `jbod_client_operation`, which will send JBOD commands over a network to a JBOD server that can be anywhere on the Internet (but will most probably be in the data center of the company). You will also implement several support functions that will take care of connecting/disconnecting to/from the JBOD server.

### Protocol

The protocol defined by the JBOD vendor has two messages. The JBOD request message is sent from your client program to the JBOD server and contains an opcode and a buffer when needed (e.g., when your client needs to write a block of data to the server side jbod system). The JBOD response message is sent from the JBOD server to your client program and contains an opcode and a buffer when needed (e.g., when your client needs to read a block of data from the server side jbod system). Both messages use the same format:

| Bytes | Field | Description |
|-------|-------|-------------|
| 0-1 | length | The size of the packet in bytes |
| 2-5 | opcode | The opcode for the JBOD operation (format defined in Lab 2 README) |
| 6-7 | return code | Return code from the JBOD operation (i.e., returns 0 on success and -1 on failure) |
| 8-263 | block | Where needed, a block of size JBOD_BLOCK_SIZE |

**Table 1: JBOD protocol packet format**

In a nutshell, there are four steps:

1. The client side (inside the function `jbod_client_operation`) wraps all the parameters of a jbod operation into a JBOD request message and sends it as a packet to the server side
2. The server receives the request message, extract the relevant fields (e.g., opcode, block if needed), issues the `jbod_operation` function to its local jbod system and receives the return code
3. The server wraps the fields such as opcode, return code and block (if needed) into a JBOD response message and sends it to the client
4. The client (inside the function `jbod_client_operation`) next receives the response message, extracts the relevant fields from it, and returns the return code and fill the parameter "block" if needed.

Note that the first three fields (i.e., length, opcode and return code) of JBOD protocol messages can be considered as packet header, with the size `HEADER_LEN` predefined in `net.h`. The block field can be considered as the optional payload. You can set the length field accordingly in the protocol messages to help the server infer whether a payload exists (the server side implementation follows the same logic).

### Implementation

In addition to replacing all `jbod_operation` calls in `mdadm.c` with `jbod_client_operation`, you will implement functions defined in `net.h` in the provided `net.c` file. Specifically, you will implement `jbod_connect` function, which will connect to `JBOD_SERVER` at port `JBOD_PORT`, both defined in `net.h`, and `jbod_disconnect` function, which will close the connection to the JBOD server. Both of these functions will be called by the tester, not by your own code. The file `net.c` contains some other functions with empty bodies that can help with structuring your code, but you may implement your own helper functions as long as you implement those functions in `net.h` that will be directly called by `tester.c` and `mdadm.c`. That being said, following the structure would probably be the easiest way to debug/test/finish this project. Please refer to `net.c` for the detailed description on the purpose, parameters, and return value of each function.
