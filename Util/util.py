"""
Copyright (c) 2018 Intel Corporation.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
"""

import socket
import logging as log
import os
import base64
import time
from Util.crypto.encrypt_decrypt import SymmetricEncryption


def check_port_availability(hostname, port):
    """
        Verifies port availability on hostname for accepting connection
        Arguments:
        hostname(str) - hostname of the machine
        port(str)     - port
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    log.debug("Attempting to connect to {}:{}".format(hostname, port))
    numRetries = 1000
    retryCount = 0
    portUp = False
    while(retryCount < numRetries):
        if(sock.connect_ex((hostname, int(port)))):
            log.debug("{} port is up on {}".format(port, hostname))
            portUp = True
            break
        retryCount += 1
        time.sleep(0.1)
    return portUp


def write_certs(file_list, file_data):
    file_name = ""
    for file_path in file_list:
        try:
            with open(file_path, 'wb+') as fd:
                file_name = os.path.basename(file_path)
                fd.write(base64.b64decode(file_data[file_name]))
        except Exception as e:
            log.debug("Failed creating file: {}, Error: {} ".format(file_name,
                                                                    e))

def delete_certs(file_list):
    for file in file_list:
        if os.path.isfile(file):
            os.remove(file)
        else:
            log.error("Failed to delete file kapacitor certs as files not found")

def create_decrypted_pem_files(srcFiles, decryptFiles):
    key = os.environ["SHARED_KEY"]
    nonce = os.environ["SHARED_NONCE"]
    symEncrypt = SymmetricEncryption(key)

    for index, file in enumerate(srcFiles):
        try:
            certBlob = symEncrypt.DecryptFile(file, nonce)
            # This logic is needed to remove extra hex chars in
            # the cert blob appearing after last \n after END CERTIFICATE
            # TODO: see if there is a better way to fix this
            certText = certBlob.decode('utf-8', errors="ignore")
            certText = '\n'.join(certText.split('\n')[:-1]) + '\n'

            with open(decryptFiles[index], "w") as f:
                f.write(certText)
                log.debug('File created: {}'.format(decryptFiles[index]))
        except Exception as e:
            log.debug("Failed creating file: {}, Error: {} ".format(
                decryptFiles[index], e))
