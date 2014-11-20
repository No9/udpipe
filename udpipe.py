import os
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

    def set_port(self, port):
        """
        Set the host port to which an un-connected client will connect to
        """
        
        port = str(port)
        lib.udpipeClient_set_port(self.obj, port)

    def set_pipe_in(self, pipe_in):
        """
        Passes a system file descriptor to udpipe to read from 
        """

        assert isinstance(pipe_in, int), "Unable to set pipe_in to non-integer type"
        lib.udpipeClient_set_pipe_in(self.obj, pipe_in)

    def set_pipe_out(self, pipe_out):
        """
        Passes a system file descriptor to udpipe to write to
        """

        assert isinstance(pipe_out, int), "Unable to set pipe_out to non-integer type"
        lib.udpipeClient_set_pipe_out(self.obj, pipe_out)

    def write(self, msg):
        """
        Write to pipe into udpipe client
        """

        assert self.pipe_for_writing, "Unable to write to client. No pipe exists"
        os.write(self.pipe_for_writing, str(msg))

    def send_file(self, path):
        """
        Write file to pipe into udpipe client
        """

        with open(path, "rb", buffering=self.buffer_size) as f:
            os.write(self.pipe_for_writing, f.read(self.buffer_size))

    def start(self, host = None, port = None):
        """
        Attempt to connect to a host and pipe data.  This command should be followed by a ``join()``
        """

        self.pipe_for_reading, self.pipe_for_writing = os.pipe()
        print self.pipe_for_reading, self.pipe_for_writing
        self.set_pipe_in(self.pipe_for_reading)

        if host: self.set_host(host)
        if port: self.set_port(port)

        return lib.udpipeClient_start(self.obj)

    def join(self):
        """
        Wait for udpipeClient to join threads after transfer completion
        """

        # if self.pipe_for_writing: os.close(self.pipe_for_writing)
        # if self.pipe_for_reading: os.close(self.pipe_for_reading)

        return lib.udpipeClient_join(self.obj)

