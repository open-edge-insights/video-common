/*
Copyright (c) 2018 Intel Corporation.

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

package tpmutil

import (
	"bytes"
	"errors"
	"github.com/golang/glog"
	"os/exec"
)

const (
	outDir   = "/IEI/tpm_secret/"
	tpmAddr  = "0x81010001"
	pcrBank  = "sha256:"
	pcrIndex = "7,8,9"
)

// execCmd - Executes command in the shell
func execCmd(tpmCmd string, args []string) (bool, []byte) {
	var sout, serr bytes.Buffer
	var status bool
	cmd := exec.Command(tpmCmd, args...)
	cmd.Stdout = &sout
	cmd.Stderr = &serr
	err := cmd.Run()
	if err != nil {
		status = false
		glog.Errorln("Error is ", (serr.String()))
	} else {
		status = true
	}
	return status, sout.Bytes()
}

func clearPersistentMem() bool {
	var evicArgs = []string{"-a", "o", "-c", tpmAddr, "-p", tpmAddr}
	status, _ := execCmd("tpm2_flushcontext", []string{"-t"})
	status, _ = execCmd("tpm2_evictcontrol", evicArgs)
	return status
}

//Creates PCR policy with 0 PCR index
func createPcrPolicy() bool {
	var authsessArgs = []string{"-S", outDir + "session.ctx"}
	var policypcrArgs = []string{"-S", outDir + "session.ctx", "-L", pcrBank + pcrIndex, "-f", outDir + "pcr.policy"}

	status, _ := execCmd("tpm2_startauthsession", authsessArgs)
	status, _ = execCmd("tpm2_policypcr", policypcrArgs)
	if !status {
		return false
	}
	status, _ = execCmd("tpm2_flushcontext", authsessArgs)
	return status
}

//Creates Pub Priv key Pairs..
func createPubPrivKeyPair() bool {
	var genArgs = []string{"genrsa", "-out", outDir + "signing.priv.pem"}
	var rsaArgs = []string{"rsa", "-in", outDir + "signing.priv.pem",
		"-out", outDir + "signing.pub.pem", "-pubout"}

	status, _ := execCmd("openssl", genArgs)
	status, _ = execCmd("openssl", rsaArgs)
	return status
}

func loadPubKeyintoTpm() bool {
	var loadArgs = []string{"-G", "rsa", "-a", "o", "-u", outDir + "signing.pub.pem",
		"-o", outDir + "signing.key.ctx", "-n",
		outDir + "signing.key.name"}

	status, _ := execCmd("tpm2_loadexternal", loadArgs)
	return status
}

//Authorize PCR policy with the Public key
func authorize() bool {
	var sessArgs = []string{"-S", outDir + "session.ctx"}
	var authArgs = []string{"-S", outDir + "session.ctx", "-o", outDir + "authorized.policy", "-n", outDir + "signing.key.name", "-f", outDir + "pcr.policy"}

	flushTpmcontext()
	status, _ := execCmd("tpm2_startauthsession", sessArgs)
	status, _ = execCmd("tpm2_policyauthorize", authArgs)
	if !status {
		return false
	}
	status, _ = execCmd("tpm2_flushcontext", sessArgs)
	return status
}

// Flush the context
func flushTpmcontext() bool {
	status, _ := execCmd("tpm2_flushcontext", []string{"-t"})
	status, _ = execCmd("tpm2_flushcontext", []string{"-s"})
	status, _ = execCmd("tpm2_flushcontext", []string{"-l"})
	return status
}

// Seal the Vault Token
func sealSecret(vaultToken string) bool {
	var primArgs = []string{"-V", "-a", "o", "-g", "sha256", "-G", "rsa", "-o", outDir + "prim.ctx"}
	var evicArgs = []string{"-a", "o", "-c", outDir + "key.obj.ctx", "-p", tpmAddr}
	var createArgs = []string{"-C", outDir + "prim.ctx", "-g", "sha256", "-u",
		outDir + "key.obj.pub", "-r", outDir + "key.obj.priv",
		"-I", vaultToken, "-L", outDir + "authorized.policy"}

	var loadArgs = []string{"-C", outDir + "prim.ctx", "-u", outDir + "key.obj.pub",
		"-r", outDir + "key.obj.priv", "-n", outDir + "key.obj.name",
		"-o", outDir + "key.obj.ctx"}

	flushTpmcontext()
	glog.Infof("TPM Sealing:: Started ")
	status, _ := execCmd("tpm2_createprimary", primArgs)
	if !status {
		return false
	}
	status, _ = execCmd("tpm2_create", createArgs)
	if !status {
		glog.Errorf("TPM Sealing:: Creating Object Failed ")
		return false
	}
	flushTpmcontext()
	status, _ = execCmd("tpm2_load", loadArgs)
	if !status {
		glog.Errorf("TPM Sealing:: Loading Object Failed ")
		return false
	}
	status, _ = execCmd("tpm2_evictcontrol", evicArgs)
	return status
}

//Creates Signature from the Priv Key
func createSignature() bool {
	var sigArgs = []string{"dgst", "-sign", outDir + "signing.priv.pem", "-out", outDir + "pcr.signature", outDir + "pcr.policy"}
	status, _ := execCmd("openssl", sigArgs)
	return status
}

//Invokes all the helpers functions from sealVault
func sealVault(vaultToken string) bool {
	if createPcrPolicy() &&
		createPubPrivKeyPair() &&
		loadPubKeyintoTpm() &&
		authorize() &&
		sealSecret(vaultToken) &&
		createSignature() {
		return true
	}
	return false
}

//API for unsealing the vault from TPM
func unsealVault() (bool, []byte) {
	var status bool
	var vault []byte
	var sigArgs = []string{"-t", outDir + "verification.tkt",
		"-c", outDir + "signing.key.ctx", "-G", "sha256",
		"-m", outDir + "pcr.policy", "-s", outDir + "pcr.signature", "-f", "rsassa"}
	var sessArgs = []string{"-a", "-S", outDir + "session.ctx"}
	var policyArgs = []string{"-S", outDir + "session.ctx", "-L", pcrBank + pcrIndex}
	var authArgs = []string{"-S", outDir + "session.ctx",
		"-o", outDir + "authorized.policy", "-f", outDir + "pcr.policy",
		"-n", outDir + "signing.key.name", "-t", outDir + "verification.tkt"}
	var unsealArgs = []string{"-c", tpmAddr, "-p", "session:" + outDir + "session.ctx"}

	glog.Infof("TPM UnSealing:: Started.... ")
	flushTpmcontext()
	status = loadPubKeyintoTpm()
	status, _ = execCmd("tpm2_verifysignature", sigArgs)
	glog.Infof("TPM UnSealing:: Verification Ticket Generated... ")
	if !status {
		glog.Errorf("TPM UnSealing:: Failed to verify signature ")
		return status, nil
	}
	status, _ = execCmd("tpm2_startauthsession", sessArgs)
	status, _ = execCmd("tpm2_policypcr", policyArgs)
	if !status {
		return status, nil
	}
	status, _ = execCmd("tpm2_policyauthorize", authArgs)
	if !status {
		glog.Errorf("TPM UnSealing:: Policy Authorization Failure ")
		return status, nil
	}
	glog.Infof("TPM UnSealing:: Policy Authorization Success ")
	status, vault = execCmd("tpm2_unseal", unsealArgs)
	if status {
		execCmd("tpm2_flushcontext", []string{"-t"})
		glog.Infof("TPM UnSealing:: Completed ")
		return status, vault
	}
	return status, nil
}

// ***************END:: TPM Helper Functions....*****************************8

// ReadFromTpm - This function reads the secret from TPM
func ReadFromTpm() ([]byte, error) {
	status, token := unsealVault()
	if status {
		return token, nil
	}
	err := errors.New("TPM Unsealing Failed")
	return nil, err
}

// WriteToTpm - This function Writes secret to TPM
func WriteToTpm(filePath string) error {
	clearPersistentMem()
	if sealVault(filePath) {
		glog.Infof("TPM Sealing:: Completed Successfully... ")
		return nil
	}
	err := errors.New("TPM Sealing Failed ")
	return err
}
