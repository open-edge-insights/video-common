# Copyright (c) 2019 Intel Corporation.
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


import logging
import cv2
import numpy as np
import time

"""Visual trigger for PCB anomaly detection.
"""
class Udf:
    """PCB anomaly detection trigger object.
    """
    def __init__(self, n_right_px, n_left_px, n_total_px, training_mode):
        """Constructor

        Parameters
        ----------
		n_right_px : minimum number of pixels to the right of PCB mask (default 1000)
			type   : int
		m_left_px : minimum number of pixels to the left of the PCB mask (default 1000)
        	type   : int
		n_total_px : minimum number of pixels in the PCB mask (default 300000)
			type   : int
		training_mode : flag to save image ROI's for training (default false)
			type   : bool


		Returns
        -------
        Filter object
        """
        print("Beggining of filter ctor")
        self.log = logging.getLogger('PCB_FILTER')
        self.log.debug("In ctor")
        # Initialize background subtractor
        self.fgbg = cv2.createBackgroundSubtractorMOG2()
        # Total white pixel # on MOG applied
        # frame after morphological operations
        self.n_total_px = n_total_px
        # Total white pixel # on left edge of MOG
        # applied frame after morphological operations
        self.n_left_px = n_left_px
        # Total white pixel # on right edge of MOG
        # applied frame after morphological operations
        self.n_right_px = n_right_px
        # Flag to lock trigger from forwarding frames to classifier
        self.filter_lock = False
        self.training_mode = training_mode
        self.profiling = False
        self.count = 0
        self.lock_frame_count = 0
        print("Done filter ctor")

    def _check_frame(self, frame):
        """Determines if the given frame is the key frame of interest for
        further processing or not

        Parameters
        ----------
        frame : numpy array
            frame blob

        Returns
        -------
        True if the given frame is a key frame, else False
        """
        print("start of _check_frame]]]...")
        # Apply background subtractor on frame
        # self.count = self.count + 1
        # cv2.imwrite("/EIS/test_videos/"+str(self.count)+".png", frame)
        print("before fgbg.apply...")
        # frame = cv2.imread("/EIS/test_videos/0.png")     
        fgmask = self.fgbg.apply(frame)
        print("after fgbg.apply...")
        rows, columns = fgmask.shape
        if self.filter_lock is False:
            # Applying morphological operations
            print("filter_lock is false")
            kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (20, 20))
            print("cv2.getStructuring returned...")
            ret, thresh = cv2.threshold(fgmask, 0, 255,
                                        cv2.THRESH_BINARY+cv2.THRESH_OTSU)
            print("cv2.threshold returned...")
            thresh = cv2.morphologyEx(thresh, cv2.MORPH_CLOSE, kernel)
            print("cv2.morphology returned...")
            # Crop left and right edges of frame
            left = thresh[:, 0:10]
            right = thresh[:, (columns - 10):(columns)]

            # Count the # of white pixels in thresh
            n_total = np.sum(thresh == 255)
            n_left = np.sum(left == 255)
            n_right = np.sum(right == 255)
            # If the PCB is in view of camera & is not
            # touching the left, right edge of frame
            if (n_total > self.n_total_px) & \
                (n_left < self.n_left_px) & \
                    (n_right < self.n_right_px):
                print("Before cv2.findContours...")
                # Find the PCB contour
                contours, hier = cv2.findContours(thresh.copy(),
                                                      cv2.RETR_EXTERNAL,
                                                      cv2.CHAIN_APPROX_NONE)
                print("After cv2.findContours...")
                print("Contours: {}".format(contours))
                if len(contours) != 0:
                    # Contour with largest area would be bounding the PCB
                    c = max(contours, key=cv2.contourArea)

                    # Obtain the bounding rectangle
                    # for the contour and calculate the center
                    x, y, w, h = cv2.boundingRect(c)
                    cX = int(x + (w / 2))

                    # If the rectangle bounding the
                    # PCB doesn't touch the left or right edge
                    # of frame and the center x lies within
                    if (x != 0) & ((x + w) != columns) & \
                       ((columns/2 - 100) <= cX <= (columns/2 + 100)):
                        return True
                    else:
                        return False
        return False	

    def process(self, frame):
        """Runs video frames from filter input queue and adds only the key
        frames to filter output queue based on the filter logic used
        """
        print("In process start, frame:", frame)
        metadata = {}

        if self.profiling is True:
            metadata['ts_vi_filter_entry'] = time.time()*1000

        if self.training_mode is True:
            print("training mode true")
            print(frame)
            print("Frame: {}".format(frame))
            cv2.imwrite("/EIS/test_videos/"+str(self.count)+".png", frame)
            print("cv2.imwrite successful")
            self.count = self.count + 1
            return True, None
        else:
            if self.filter_lock is False:
                print("filter_lock is False")
                if self._check_frame(frame):
                    print("Sending frame")
                    metadata["user_data"] = 1
                    print("In process check_frame true")
                    self.filter_lock = True
                    # Re-initialize frame count during trigger lock to 0
                    self.lock_frame_count = 0
                    return False, metadata
            else:
                print("filter_lock is True")
                # Continue applying background subtractor to
                # keep track of PCB positions
                self._check_frame(frame)
                # Increment frame count during trigger lock phase
                self.lock_frame_count = self.lock_frame_count + 1
                if self.lock_frame_count == 7:
                    # Clear trigger lock after timeout
                    # period (measured in frame count here)
                    self.filter_lock = False
                # else:
                #     # Continue applying background subtractor to
                #     # keep track of PCB positions
                #     print("Continue applying background")
                #     self._check_frame(frame)
                #     print("In process check_frame false")
                #     return True, None

