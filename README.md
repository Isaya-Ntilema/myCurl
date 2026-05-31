This is the implementation of 'myCurl' application. It uses BOOST libraries, libssl and libcrypto for HTTPS.

The client implements the GET method and handles both HTTP and HTTPS. It can follow redirects for a limited number of times. 

The content that is returned from the URI should be saved to a file (if -o  or  --output is provided as an argument) or just discarded once downloaded. 
