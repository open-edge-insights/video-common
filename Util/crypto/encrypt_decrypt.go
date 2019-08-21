/*
Copyright (c) 2018 Intel Corporation.

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

package encryptutil

import (
	"crypto/aes"
	"crypto/cipher"
	"io/ioutil"
	"os"
)

// SymmetricEncryption structure
type SymmetricEncryption struct {
	key []byte      //key used for encryption/decryption
	gcm cipher.AEAD //gcm object
}

// NewSymmetricEncryption : This is the constructor to initialize the SymmetricEncryption
func NewSymmetricEncryption(keyStr string) (*SymmetricEncryption, error) {
	key := []byte(keyStr)

	block, err := aes.NewCipher(key)
	if err != nil {
		return nil, err
	}

	gcm, err := cipher.NewGCM(block)
	if err != nil {
		return nil, err
	}

	return &SymmetricEncryption{key: key, gcm: gcm}, nil
}

// Encrypt function encrypts the plain text using the key and the nonce
func (pSymEncrpt *SymmetricEncryption) Encrypt(plainText []byte, nonce string) ([]byte, error) {
	cipherText := pSymEncrpt.gcm.Seal(plainText[:0], []byte(nonce), plainText, nil)
	return cipherText, nil
}

// Decrypt function decrypts the cipher text using the key and the nonce
func (pSymEncrpt *SymmetricEncryption) Decrypt(cipherText []byte, nonce string) ([]byte, error) {
	plainText, err := pSymEncrpt.gcm.Open(nil, []byte(nonce), cipherText, nil)
	return plainText, err
}

// EncryptFile function encrypts the file passed
func (pSymEncrpt *SymmetricEncryption) EncryptFile(filePath string, nonce string) error {
	if _, err := os.Stat(filePath); err != nil {
		return err
	}
	plainText, err := ioutil.ReadFile(filePath)
	if err != nil {
		return err
	}
	cipherText, err := pSymEncrpt.Encrypt(plainText, nonce)

	err = ioutil.WriteFile(filePath, cipherText, 0777)
	if err != nil {
		return err
	}
	return nil
}

// DecryptFile function decrypts the file passed and returns just the decrypted content if overwrite flag is not set.
// If overwriteFile flag is set, then the file itself is decrypted
func (pSymEncrpt *SymmetricEncryption) DecryptFile(filePath string, nonce string, overwriteFile bool) ([]byte, error) {
	if _, err := os.Stat(filePath); err != nil {
		return nil, err
	}
	cipherText, err := ioutil.ReadFile(filePath)
	if err != nil {
		return nil, err
	}
	plainText, err := pSymEncrpt.Decrypt(cipherText, nonce)
	if err != nil {
		return nil, err
	}
	if overwriteFile {
		err = ioutil.WriteFile(filePath, plainText, 0777)
		if err != nil {
			return nil, err
		}
	}
	return plainText, err
}
