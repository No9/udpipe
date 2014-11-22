import os
import array
from ctypes import cdll
lib = cdll.LoadLibrary('ludpipe.so')
import io

class udpipeClient(object):
    def __init__(self, host = None, port = None):
        """ 
        Creates a new udpipeClient instance from shared object library
        """

        self.buffer_size = 67108864
        self.pipe_in = None
        self.pipe_out = None
        
        if host and port:
            host = str(host)
            port = str(port)
            self.obj = lib.udpipeClient_new_host_port(host, port)
        else:
            self.obj = lib.udpipeClient_new()

    def set_host(self, host):
        """
        Set the host address to which an un-connected client will connect to
        """

        host = str(host)
        lib.udpipeClient_set_host(self.obj, host)
        return self

    def set_port(self, port):
        """
        Set the host port to which an un-connected client will connect to
        """
        
        port = str(port)
        lib.udpipeClient_set_port(self.obj, port)
        return self

    def set_pipe_in(self, pipe_in):
        """
        Passes a system file descriptor to udpipe to read from 
        """

        assert isinstance(pipe_in, int), "Unable to set pipe_in to non-integer type"
        lib.udpipeClient_set_pipe_in(self.obj, pipe_in)
        return self

    def set_pipe_out(self, pipe_out):
        """
        Passes a system file descriptor to udpipe to write to
        """

        assert isinstance(pipe_out, int), "Unable to set pipe_out to non-integer type"
        lib.udpipeClient_set_pipe_out(self.obj, pipe_out)
        return self

    def write(self, msg):
        """
        Write to pipe into udpipe client
        """

        pipe_in = lib.udpipeClient_get_pipe_in(self.obj)
        pipe_out = lib.udpipeClient_get_pipe_out(self.obj)
        lib.udpipeClient_write_buffer(self.obj, msg, len(msg))
        return self

    def set_port(self, port):
        """
        Set the host port to which an un-connected client will connect to
        """
        
        port = str(port)
        lib.udpipeClient_set_port(self.obj, port)
        return self

    def send_file(self, path):
        """
        Write file to pipe into udpipe client
        """

        lib.udpipeClient_send_file(self.obj, path)
        return self

    def start(self, host = None, port = None):
        """
        Attempt to connect to a host and pipe data.  This command should be followed by a ``join()``
        """

        if host: self.set_host(host)
        if port: self.set_port(port)

        ret = lib.udpipeClient_start(self.obj)
        return self

    def join(self):
        """
        Wait for udpipeClient to join threads after transfer completion
        """

        ret = lib.udpipeClient_join(self.obj)
        return self 

