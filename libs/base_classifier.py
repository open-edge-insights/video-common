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


"""Module forms the base for classifying the frames
   based on the classifier logic in the subclass of BaseClassifier
"""

import inspect
import importlib
import threading
import logging
from concurrent.futures import ThreadPoolExecutor


def load_classifier(classifier, classifier_config, input_queue, output_queue):
    """Load the specified classifier.

    :param classifier: Name of the classifier to attempt to load
    :type classifier: str
    :param classifier_config: Configuration object for the classifier
    :type classifier_config: dict
    :param input_queue: input queue for classifier
    :type input_queue: queue
    :param output_queue: output queue of classifier]
    :type output_queue: queue
    :raises Exception: If an issue arises while loading the Python module for
                       the classifier
    :raises Exception: If the configuration for the classifier is incorrect
    :raises Exception: If Classifier config key is missing
    :return: Classifier object for the specified classifier
    :rtype: Object
    """
    try:
        lib = importlib.import_module(
                '.{}'.format(classifier),
                package='VideoAnalytics.classifiers')

        arg_names = inspect.getargspec(lib.Classifier.__init__).args
        if len(arg_names) > 0:
            # Skipping the first argument since it is the self argument
            args = list()
            args.append(classifier_config)
            args.append(input_queue)
            args.append(output_queue)
        else:
            args = []

        return lib.Classifier(*args)
    except AttributeError:
        raise Exception(
                '{} module is missing the Classifier class'.format(classifier))
    except ImportError:
        raise Exception(
                'Failed to load classifier: {}'.format(classifier))
    except KeyError as e:
        raise Exception(
                'Classifier config missing key: {}'.format(e))


class BaseClassifier:
    """Base class for all classifier classes
    """

    def __init__(self, classifier_config, input_queue, output_queue):
        """Constructor to initialize classifier object

        :param classifier_config: Configuration object for the classifier
        :type classifier_config: dict
        :param input_queue: input queue for classifier
        :type input_queue: queue
        :param output_queue: output queue of classifier
        :type output_queue: queue
        """
        self.log = logging.getLogger(__name__)
        self.name = None
        self.input_queue = input_queue
        self.output_queue = output_queue
        self.config = classifier_config
        self.log.debug('classifier config: {}'.format(classifier_config))
        self.max_workers = classifier_config["max_workers"]
        self.stop_ev = threading.Event()

    def start(self):
        """Starts `max_workers` pool of threads to feed on the classifier
        input queue, run through each frame from the queue with classifier
        logic and add only key frames for further processing in the classifier
        output queue
        """
        self.classifier_threadpool = \
            ThreadPoolExecutor(max_workers=self.max_workers)
        for _ in range(self.max_workers):
            self.classifier_threadpool.submit(self.classify)

    def stop(self):
        """Stops the pool of classifier threads responsible for classiflying
        frames and adding data to the classifier output queue
        """
        self.stop_ev.set()
        self.classifier_threadpool.shutdown(wait=False)

    def set_name(self, name):
        """Sets the name of the classifier

        :param name: Name of the classifier
        :type name: str
        """
        self.name = name

    def get_name(self):
        """Gets the name of the classifier

        :return: Name of the classifier
        :rtype: str
        """
        return self.name
