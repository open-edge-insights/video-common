#!/bin/bash
# Copyright (c) 2020 Intel Corporation.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

BITSTREAM_PATH="/opt/intel/openvino/bitstreams/a10_vision_design_sg1_bitstreams"

#------------------------------------------------------------------------------ 
# setup_fpga_environment
#
# Description: 
#       Creates FPGA environment
# Args:
#       None
# Return:
#       None
# Usage:
#       setup_fpga_environment
#------------------------------------------------------------------------------
setup_fpga_environment()
{
    if [ -d "${BITSTREAM_PATH}/BSP/" ];then
        cd "${BITSTREAM_PATH}"/BSP/
        tar -xvzf a10_1150_sg1_r4.tgz
    else
        echo "The directory ${BITSTREAM_PATH}/BSP/ doesn't exist. Exiting"
        exit 1
    fi
    chmod -R 755 "${BITSTREAM_PATH}"
}

#main
echo "Setup FPGA environment-start"
setup_fpga_environment
echo "Setup FPGA environment-end"
