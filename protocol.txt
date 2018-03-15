KCP PROTOCOL SPECIFICATION


1. Packet (aka. segment) Structure

KCP has only one kind of segment: both the data and control messages are 
encoded into the same structure and share the same header.

The KCP packet (aka. segment) structure is as following:

0               4   5   6       8 (BYTE)
+---------------+---+---+-------+
|     conv      |cmd|frg|  wnd  |
+---------------+---+---+-------+   8
|     ts        |     sn        |
+---------------+---------------+  16
|     una       |     len       |
+---------------+---------------+  24
|                               |
|        DATA (optional)        |
|                               |
+-------------------------------+


- conv: conversation id (32 bits integer)

The conversation id is used to identify each connection, which will not change
during the connection life-time.

It is represented by a 32 bits integer which is given at the moment the KCP
control block (aka. struct ikcpcb, or kcp object) has been created. Each
packet sent out will carry the conversation id in the first 4 bytes and a
packet from remote endpoint will not be accepted if it has a different
conversation id.

The value can be any random number, but in practice, both side between a
connection will have many KCP objects (or control block) storing in the
containers like a map or an array. A index is used as the key to look up one
KCP object from the container. 

So, the higher 16 bits of conversation id can be used as caller's index while
the lower 16 bits can be used as callee's index. KCP will not handle
handshake, and the index in both side can be decided and exchanged after 
connection establish.

When you receive and accept a remote packet, the local index can be extracted
from the conversation id and the kcp object which is in charge of this
connection can be find out from your map or array.


- cmd: command

- frg: fragment count

- wnd: window size

- ts: timestamp

- sn: serial number

- una: un-acknowledged serial number


# vim: set ts=4 sw=4 tw=0 noet cc=78 wrap textwidth=78 :

