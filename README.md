UDTCAT
======

UDTCAT is a UDT data transfer device based off of the functionality of netcat.

CONTENT
-------
./src:     UDTCAT source code

./udt:	      UDT source code, documentation and license


TO MAKE
------- 
    make -e os=XXX arch=YYY 

XXX: [LINUX(default), BSD, OSX]   
YYY: [IA32(default), POWERPC, IA64, AMD64]  

### Dependencies:
OpenSSL (libssl and libcrypto)  

UDTCAT has only been tested for Linux.


USAGE
------

UDTCAT follows the same model as netcat.  The server side establishes a listener, and awaits an incoming connection.  The client side connects to an established server or times out.  Encryption is off by default. The encrypted option uses a multithreaded version of OpenSSL with aes-128.

### Basic usage:

Server side:
       uc [udtcat options] -l port

Client side:
       uc [udtcat options] host port

#### UDTCAT Options:

     -l							start a server
     -n n_crypto_threads 		set number of encryption threads per send/recv thread to n_crypto_threads
     -p key				specify key in-line
     -f path			        path to key file
     -v							verbose


### Basic exmple (unencrypted)

Client side:

       uc localhost 9000 < source/file

Server side:

       uc -l 9000 > output/file

### Basic exmple (encrypted)

Client side:

       uc -n 4 -p PASSword localhost 9000 < source/file

Server side:

       uc -n 4 -f file/contains/PASSword -l 9000 > output/file

This examples creates a connection to trasfer "source/file" to "output/file" over an encrypted stream on port 9000 which uses 4 threads to encrypt/decrypt each block.  The password used as a key for OpenSSL is "PASSword"


