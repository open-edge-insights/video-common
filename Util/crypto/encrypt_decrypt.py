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

import os
import sys
import datetime
import time

from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives.ciphers import (
    Cipher, algorithms, modes
)


class SymmetricEncryption(object):

    def __init__(self, keyStr):
        """
            SymmetricEncryption constructor.
            Arguments:
            keyStr(string) - key used for encryption/decryption
            Returns:
            SymmetricEncryption object
        """
        self.key = str.encode(keyStr)
        self.cipher = Cipher(
                    algorithms.AES(self.key),
                    None,
                    backend=default_backend()
                )

    def Encrypt(self, plainText, nonce):
        """
            Encrypts the plain text using the key and nonce
            Arguments:
            plainText([]byte) - plain text to encrypt
            nonce(string)      - unique arbitrary number
            Returns:
            cipherText([]byte) - encrypted plain text
        """
        self.cipher.mode = modes.GCM(nonce.encode())
        encryptor = self.cipher.encryptor()
        cipherText = encryptor.update(plainText)
        return cipherText

    def Decrypt(self, cipherText, nonce):
        """
            Decrypts the cipher text using the key and nonce
            Arguments:
            cipherText([]byte) - cipher text to decrypt
            nonce(string)      - unique arbitrary number
            Returns:
            plainText([]byte) - decrypted cipher text
        """
        self.cipher.mode = modes.GCM(nonce.encode())
        decryptor = self.cipher.decryptor()
        plainText = decryptor.update(cipherText)
        return plainText

    def EncryptFile(self, filePath, nonce):
        """
            Encrypts the passed file using the key and nonce
            Arguments:
            filePath(string)   - file to be encrypted
            nonce(string)      - unique arbitrary number
            Returns:
            Encrypted file written to the same path
        """
        if not os.path.exists(filePath):
            raise Exception("file: {} doesn't exist".format(filePath))

        try:
            with open(filePath, "r") as f:
                plainText = f.read()

                with open(filePath, "wb") as f:
                    cipherText = self.Encrypt(plainText, nonce)
                    f.write(cipherText)
        except Exception as e:
            raise e

    def DecryptFile(self, filePath, nonce, overwriteFile=False):
        """
            Decrypts the passed file using the key and nonce
            Arguments:
            filePath(string)   - file to be encrypted
            nonce(string)      - unique arbitrary number
            overwriteFile(bool)- flag to writeback the decrypted
                                 content to file or just send the
                                 decrypted blob
            Returns:
            Returns decrypted blob (plain text) if overwriteFile flag
            is not set. If set, the decrypted file is updated with the
            decrypted content
        """
        if not os.path.exists(filePath):
            raise Exception("file: {} doesn't exist".format(filePath))
        try:
            with open(filePath, "rb") as f:
                cipherText = f.read()
                plainText = self.Decrypt(cipherText, nonce)
            if overwriteFile:
                with open(filePath, "w") as f:
                    f.write(plainText)
            else:
                return plainText
        except Exception as e:
            raise e
