# Reliable Transport Layer Implementation

## Overview
This project create a reliability layer on top of UDP. The goal is to ensure reliable, in-order delivery of data packets over an unreliable network using concepts like acknowledgments, retransmissions, and sequence numbering. The implementation includes a **client** and a **server**, both of which communicate using a custom packet structure over BSD sockets.


## Design Choices

1. **Three-Way Handshake:**  
   - Implemented a handshake to synchronize initial sequence numbers between client and server.
   - Randomized initial sequence numbers to avoid conflicts.

2. **Sliding Window Protocol:**  
   - A window size of 20 packets (20,240 bytes) allows efficient transmission.
   - Packets are kept in a **sending buffer** until acknowledged by the receiver.
   - The **receiving buffer** stores out-of-order packets to ensure in-order delivery.

3. **Packet Structure:**  
   - Used a fixed packet format with fields for sequence number, acknowledgment number, length, and flags (SYN, ACK).
   - Maximum payload size is 1012 bytes, adhering to the 1024-byte packet size constraint.

4. **Retransmission Logic:**  
   - Packets are retransmitted after 1 second of no acknowledgment.
   - Implemented fast retransmission upon receiving 3 duplicate ACKs.


## Problems Encountered

1. **Data Loss and Duplication:**  
   During testing with dropped and reordered packets, out-of-order delivery caused data corruption.

   **Solution:**  
   - Used the sequence number to reassemble packets correctly.
   - Added logic to discard duplicate packets in the receiving buffer.

2. **Timeout Handling:**  
   Ensuring accurate retransmission without blocking caused synchronization issues.

   **Solution:**  
   - Used non-blocking sockets with a timeout mechanism.
   - Carefully tracked the oldest unacknowledged packet for retransmission.

3. **Big/Little Endian Conversion:**  
   Inconsistent results arose when interpreting numbers across different machines.

   **Solution:**  
   - Ensured all sequence numbers, acknowledgment numbers, and lengths were converted to **network byte order** before sending.

## Testing
The implementation was tested using:
- **Local Autograder**: Verified functionality under loss, reordering, and large file scenarios.
- **Custom Tests**: Transferred files of various sizes and ensured integrity using the `diff` command.
