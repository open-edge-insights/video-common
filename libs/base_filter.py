# Copyright (c) 2019 Intel Corporation.

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

"""Module forms the base for filtering the input frames
   based on the filter logic in the subclass of BaseFilter
"""

import logging
import importlib
import threading
import inspect
import queue
from concurrent.futures import ThreadPoolExecutor


def load_filter(filter, filter_config, input_queue, output_queue):
    """Load the given filter with the specified configuration.

    :param filter: String name of the filter
    :type filter: str
    :param filter_config: Configuration object for the filter
    :type filter_config: queue
    :param input_queue: input queue for filter
    :type input_queue: queue
    :param output_queue: output queue of filter
    :type output_queue: queue
    :raises Exception: If an issue arises while loading the Python module
                       for the filter
    :raises Exception: If the configuration for the filter is incorrect
    :raises Exception: If Filter config key is missing
    :return: Filter object for the specified filter
    :rtype: Object
    """
    try:
        lib = \
            importlib.import_module('VideoIngestion.filters.{}'.format(filter))

        arg_names = inspect.getargspec(lib.Filter.__init__).args
        if len(arg_names) > 0:
            # Skipping the first argument since it is the self argument
            args = list()
            args.append(filter_config)
            args.append(input_queue)
            args.append(output_queue)
        else:
            args = []

        filter = lib.Filter(*args)

        return filter
    except AttributeError:
        raise Exception(
                '"{}" module is missing the Filter class'.format(filter))
    except ImportError:
        raise Exception('Failed to load filter: {}'.format(filter))
    except KeyError as e:
        raise Exception('Filterr config missing key: {}'.format(e))


class BaseFilter:
    """Base class for all filter classes
    """

    def __init__(self, filter_config, input_queue, output_queue):
        """Constructor to initialize filter object

        :param filter_config: Configuration object for the filter
        :type filter_config: dict
        :param input_queue: input queue for filter
        :type input_queue: queue
        :param output_queue: output queue of filter
        :type output_queue: queue
        """
        self.log = logging.getLogger(__name__)
        self.name = None
        self.input_queue = input_queue
        self.output_queue = output_queue
        self.filter_config = filter_config
        self.log.debug('filter config: {}'.format(filter_config))
        self.max_workers = filter_config["max_workers"]
        self.stop_ev = threading.Event()

    def send_data(self, data):
        """Add data to the filter output queue

        Parameters
        ----------
        data : Object
            data to be added to the filter output queue
        """
        self.log.debug("Data added to filter output queue...")
        self.output_queue.put(data)

    def start(self):
        """Starts `max_workers` pool of threads to feed on the filter input
        queue, run through each frame from the queue with filter logic and
        add only key frames for further processing in the filter output queue.
        """
        self.filter_threadpool = \
            ThreadPoolExecutor(max_workers=self.max_workers)
        for _ in range(self.max_workers):
            self.filter_threadpool.submit(self.on_data)

    def stop(self):
        """Stops the pool of filter threads responsible for filtering frames
        and adding data to the filter output queue
        """
        self.stop_ev.set()
        self.filter_threadpool.shutdown(wait=False)

    def set_name(self, name):
        """Sets the name of the filter

        :param name: Name of the filter
        :type name: str
        """
        self.name = name

    def get_name(self):
        """Gets the name of the filter

        :return: Name of the filter
        :rtype: str
        """
        return self.name
