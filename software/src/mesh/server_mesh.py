#!/usr/bin/env python

import sys
import binascii
import struct

global HOST, PORT
HOST, PORT = "0.0.0.0", 7000

if sys.version_info[0] < 3:
    import SocketServer as socketserver
else:
    import socketserver

class MeshHandler(socketserver.BaseRequestHandler):
    def do_broadcast(self, response):
        response[4] = 0
        response[5] = 0
        response[6] = 0
        response[7] = 0
        response[8] = 0
        response[9] = 0

        return response

    def handle(self):
        print('\n[+] MESH_SERVER: New client connected\n')

        try:
            while True:
                request = bytearray()
                response = bytearray()

                # The following is a blocking call until data arrives on the socket.
                request.extend(self.request.recv(8192))

                req_str = ''

                # Prepare the print the request.
                for i, d in enumerate(request):
                    if i == len(request) - 1:
                        req_str = req_str + str(d)
                    else:
                        req_str = req_str + str(d) + ', '

                '''
                Get payload of the request. We assume there are no mesh options
                in the header or payload starts from the 17th byte of the request.
                '''
                payload = request[16:]

                payload_str = ''

                # Prepare the print the request properly.
                for i, d in enumerate(payload):
                    if i == len(payload) - 1:
                        payload_str = payload_str + str(d)
                    else:
                        payload_str = payload_str + str(d) + ', '

                '''
                Assemble the response. Basically swap destination and source
                addresses.
                '''
                response.extend(request[0:4])
                response.extend(request[10:16])
                response.extend(request[4:10])
                response.extend(request[16:])

                '''
                Clear bit zero to indicate downward packet since server response
                will go into the mesh network.
                '''
                response[1] = response[1] & ~0x01

                print('\n[+] MESH_SERVER: request = ' + req_str + '\n')
                print('\n[+] MESH_SERVER: Payload = ' + payload_str + '\n')

                response_str = ''

                for i, d in enumerate(response):
                    if i == len(response) - 1:
                        response_str = response_str + str(d)
                    else:
                        response_str = response_str + str(d) + ', '

                print('\n[+] MESH_SERVER: Sending unicast response to node\n')
                print('\n[+] MESH_SERVER: Unicast packet = ' + response_str + '\n')

                self.request.sendall(response)

                # Prepare the response to be a broadcast.
                response = self.do_broadcast(response)

                response_str = ''

                for i, d in enumerate(response):
                    if i == len(response) - 1:
                        response_str = response_str + str(d)
                    else:
                        response_str = response_str + str(d) + ', '

                print('\n[+] MESH_SERVER: Sending broadcast response to node\n')
                print('\n[+] MESH_SERVER: broadcast packet = ' + response_str + '\n')

                self.request.sendall(response)

        except Exception as e:
            print('\n[+] MESH_SERVER: Exception, ' + str(e) + '\n')

    def read_full(self, n):
        while len(self.buf) < n:
            try:


                if not req:
                    raise(Exception('\n[+] MESH_SERVER: Receive error\n'))
                self.buf.extend(req)

            except Exception as e:
                print('\n[+] MESH_SERVER: Exception, ' + str(e) + '\n')

        read = self.buf[0:n]
        self.buf = self.buf[n:]

        return bytes(read)

class ThreadedTCPServer(socketserver.ThreadingMixIn, socketserver.TCPServer):
    daemon_threads = True

if __name__ == "__main__":
    socketserver.TCPServer.allow_reuse_address = True
    server = ThreadedTCPServer((HOST, PORT), MeshHandler)

    print('\n[+] MESH_SERVER: Listening on ' + HOST + ':' + str(PORT) + '\n')

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass

    print('\n[+] MESH_SERVER: End mesh server\n')

    server.server_close()
