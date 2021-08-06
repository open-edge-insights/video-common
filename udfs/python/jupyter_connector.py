# Copyright (c) 2021 Intel Corporation.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM,OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
"""Accepts the frame and publishes it to the EII JupyterNotebook service which
   processes the frame and publishes it back to the jupyter_connector UDF.
"""

import logging
import eii.msgbus as mb
import cfgmgr.config_manager as cfg
import threading
import time


class Udf:
    """Jupyter Notebook connector UDF object
    """
    def __init__(self):
        """Constructor
        """
        self.log = logging.getLogger('JupyterNotebookConnector')
        self.log.debug(f"In {__name__}...")

        # Initializing Etcd to fetch UDF config
        ctx = cfg.ConfigMgr()
        app_cfg = ctx.get_app_config()
        app_cfg = app_cfg.get_dict()
        if "udfs" in app_cfg:
            for udf in app_cfg["udfs"]:
                if udf["name"] == "jupyter_connector":
                    self.udf_config = udf

        # Initializing msgbus publisher
        self.msgbus_cfg = {
                            "type": "zmq_ipc",
                            "socket_dir": "/EII/sockets"
                          }
        self.msgbus = mb.MsgbusContext(self.msgbus_cfg)
        self.publisher = self.msgbus.new_publisher("jupyter_publisher")

        # Initializing msgbus subscriber
        self.subscriber = self.msgbus.new_subscriber("jupyter_subscriber")

        # Starting thread to send UDF config
        conf_thread = threading.Thread(target=self._send_config)
        conf_thread.start()

    def _send_config(self):
        # Send UDF config to jupyter notebook service
        self.config_publisher = self.msgbus.new_publisher("jupyter_config")
        while True:
            self.config_publisher.publish(self.udf_config)
            time.sleep(3)

    def process(self, frame, metadata):
        """Publishes every frame received to Jupyter Notebook service
           and returns the respective processed & subscribed frame

        :param frame: frame blob
        :type frame: numpy.ndarray
        :param metadata: frame's metadata
        :type metadata: str
        :return:  (should the frame be dropped, has the frame been updated,
                   new metadata for the frame if any)
        :rtype: (bool, numpy.ndarray, str)
        """
        self.log.debug(f"In process() method...")

        # Publishing metadata & frames
        self.publisher.publish((metadata, frame.tobytes(),))
        metadata, frame = self.subscriber.recv()
        if "jpnb_frame_drop" in metadata:
            del metadata["jpnb_frame_drop"]
            return True, None, None
        elif "jpnb_frame_updated" in metadata and "jpnb_metadata_updated" in metadata:
            del metadata["jpnb_frame_updated"]
            del metadata["jpnb_metadata_updated"]
            return False, frame, metadata
        elif "jpnb_frame_updated" in metadata or "jpnb_metadata_updated" in metadata:
            if "jpnb_frame_updated" in metadata:
                del metadata["jpnb_frame_updated"]
                return False, frame, None
            if "jpnb_metadata_updated" in metadata:
                del metadata["jpnb_metadata_updated"]
                return False, None, metadata
        else:
            return False, None, None
