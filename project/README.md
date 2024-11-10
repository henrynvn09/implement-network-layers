# Design choice

- Use circular array for sending buffer of size 21 for the windows of size 20
- use linked list for receving buffer because we don't know about its size

# bug - solution

- failing test case of only from client

```
Your client didn't send data back to our server correctly.
We inputted 20000 bytes in your client and we received 0 bytes with a percent difference of 100.0%
```

    - solution: check the matching of the client seq during the handshake

- passed reference server <-> client and server <-> reference client. However, fail between client - client
  - solution: manually simulate requests to see the output between those
  - Found that the handshake seq is off by 1, turns out I was wrong when incremented client_seq before sending the ack
