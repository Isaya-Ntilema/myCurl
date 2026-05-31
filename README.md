This is the implementation of 'myCurl' application. But BOOST is fine (I, however, need to know what libraries you use, so that I can install/configure it in CodeGrade). 

The client should handle both HTTP and HTTPS; you must implement the GET method, and your solution needs to follow redirects for a limited number (<10) of times. You're free to implement additional methods, but I will only test GET. 

The content that is returned from the URI should be saved to a file (if -o  or  --output is provided as an argument) or just discarded once downloaded. We will use this to compare whether your solution adds or removes from the content that it transfers. 
