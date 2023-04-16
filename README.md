# rft_protocol_csc2035_assessment2_2022

This project was created for the second assignment of the CSC2035 Software Systems Design and Implementation module at Newcastle University.

Task: A Reliable File Transfer protocol that works on top of UDP

"This coursework gives you the opportunity to develop your skills in code comprehension and in systems and network socket programming using the C programming language. 
The aim is to build a Reliable File Transfer (RFT) protocol that works on top of UDP to provide reliable delivery of a file between a client and a server. This will give 
you some insight into the implementation of reliable communications protocols. The library you develop will also show how a framework for communications can be 
parameterised by different protocol implementations."

This coursework involves the completion of two file transfer protocols:
- Reliable file transfer (RFT) with positive acknowledgement in the absence of errors (implemented in the function send_file_normal)
- Reliable file transfer (RFT) with positive acknowledgement and retransmission – an example of a PAR protocol (implemented in the function send_file_with_timeout)
