# Contents

- [Contents](#contents)
  - [How-To guide for writing UDF for OEI](#how-to-guide-for-writing-udf-for-oei)
  - [Introduction](#introduction)
  - [Steps for writing native(c++) UDFs](#steps-for-writing-nativec-udfs)
    - [OEI APIs for Writing Native UDFs (C++)](#oei-apis-for-writing-native-udfs-c)
    - [OEI APIs for Writing Raw Native UDFs(c++) for Multi Frame Support](#oei-apis-for-writing-raw-native-udfsc-for-multi-frame-support)
    - [OEI Infrastructure Related Changes](#oei-infrastructure-related-changes)
  - [Steps for writing Python UDFs](#steps-for-writing-python-udfs)
    - [Python APIs for writing UDF for OEI](#python-apis-for-writing-udf-for-oei)
    - [OEI Infrastructure changes](#oei-infrastructure-changes)
    - [Conclusion](#conclusion)

## How-To guide for writing UDF for OEI

This document describes stepwise instruction for writing a User defined Function(UDF) in C++/Python in order to make it deployable in OEI environment. It  explains APIs and configurational changes required in order to write an UDF for OEI.

>**Note:** In this document, you will find labels of 'Edge Insights for Industrial (EII)' for filenames, paths, code snippets, and so on. Consider the references of EII as OEI. This is due to the product name change of EII as OEI.

## Introduction

UDFs(User Defined Function) are one of the cardinal feature of OEI framework. It enables users to adjoin any pre-processing or post-processing logic in data pipeline defined by OEI configuration. As of OEI 2.1 release, it supports UDFs to be implemented using following languages.

- C++  (It is also called **Native** UDF as OEI core components are implemented in C++)
- Python

The order in which the UDFs are defined in OEI configuration file is the order in which data will flow across them. Currently there is no support for demux/mux the data flow to/fro the UDFs.

All configs related to UDFs are to be in `config.json` of apps like VideoIngestion and VideoAnalytics.The UDF schema and description about keys/values in it presented in detail in the [UDF README](./README.md) file.

## Steps for writing native(c++) UDFs

Every UDF writing has two major part to it.

- Writing actual pre/post processing logic using OEI exposed APIs.

- Adding OEI infra specific configuration component for deploying it

### OEI APIs for Writing Native UDFs (C++)

There are three APIs defined semantically to add the pre/post processing logic. These APIs must be implemented as method of a user defined class inherited from the Udf class named ***BaseUdf***.

- #### Initialization and DeInitialization

    ``` C++
        class DummyUdf : public BaseUdf {
                public:
                    DummyUdf(config_t* config) : BaseUdf(config) {
                        //initialization Code can be added here
                    };

                    ~DummyUdf() {
                        // Any de-initialization logic to be added here
                    };
        };

    ```

  The **DummyUdf** in the above code snippet is the user defined class and the constructor of the class initialize the UDF's specific data-structure. The only argument passed to this function is **config** which depicts configuration details mentioned in the `config.json` file of apps liks VideoIngestion and VideoAnalytics which process UDFs. The API to consume **config** is defined in [Util README](https://github.com/open-edge-insights/eii-c-utils/blob/master/README.md)

- #### Processing the Actual Data

  The API to utilize the ingested input looks as below:

    ``` C++
    UdfRetCode
    process(cv::Mat& frame, cv::Mat& outputFrame, msg_envelope_t* meta) override {
        // Logic for processing the frame & returning the inference result.
    }
    ```

  This function is a override method which user need to define in its UDF file.

  The *argument* details are described as below:

  - **Argument 1(cv::Mat &frame)**: It represents the input frame for inference.

  - **Argument 2(cv::Mat &outputFrame)**: It represents the modified frame by the user. This can be used if user need to pass a modified frame forward.

  - **Argument 3(msg_envelope_t\* meta)**: It represents the inference result returned by UDF. The user need to fill the **msg_envelope_t** structure as described in following [**EIIMsgEnv README**](https://github.com/open-edge-insights/eii-messagebus/blob/master/README.md). There are sample code suggested in the README which explains the API usage in detail.

  The *return* code details are  described as below:

  - **UdfRetCode**: User need to return appropriate macro as mentioned below:
  - **UDF_OK** - UDF has processed the frame gracefully.
  - **UDF_DROP_FRAME** - The frame passed to process function need to be dropped.
  - **UDF_ERROR** - it should be returned for any kind of error in UDF.

- #### Linking Udfloader and Custom UDFs

  The **initialize_udf()** function need to defined as follows to create a link between UdfLoader module and respective UDF. This function ensure UdfLoader to call proper constructor and process() function of respective UDF.

    ```C++
    extern "C" {
        void *initialize_udf(config_t *config) {
            DummyUdf *udf = new DummyUdf(config);
            return (void *)udf;
        }
    }
    ```

  The **"DummyUdf"** is the class name of the user defined custom UDF.

---

### OEI APIs for Writing Raw Native UDFs(c++) for Multi Frame Support

There are three APIs defined semantically to add the pre/post processing logic. These APIs must be implemented as method of a user defined class inherited from the Udf class named ***RawUdf***.

- #### Initialization and Deinitialization

    ``` C++
        class RealSenseUdf : public RawBaseUdf {
                public:
                    RealSenseUdf(config_t* config) : RawBaseUdf(config) {
                        //initialization Code can be added here
                    };

                    ~RealSenseUdf() {
                        // Any de-initialization logic to be added here
                    };
        };

    ```

  The **RealSenseUdf** in the above code snippet is the user defined class and the constructor of the class initialize the UDF's specific data-structure. The only argument passed to this function is **config** which depicts configuration details mentioned in the `config.json` file of apps liks VideoIngestion and VideoAnalytics which process UDFs. The API to consume **config** is defined in [Util README](../common/util/c/README.md)

- #### Processing Actual Data

  The API to utilize the ingested input looks as below:

    ``` C++
    UdfRetCode
    process(Frame* frame) override {
        // Logic for processing the frame & returning the inference result.
    }
    ```

  This function is a override method which user need to define in its UDF file.

  The *argument* details are described as below:

- **Argument 1(Frame* frame)**: It represents the frame object.

  The *return* code details are  described as below:

  - **UdfRetCode**: User need to return appropriate macro as mentioned below:
  - **UDF_OK** - UDF has processed the frame gracefully.
  - **UDF_DROP_FRAME** - The frame passed to process function need to be dropped.
  - **UDF_ERROR** - it should be returned for any kind of error in UDF.

- #### Linking Udfloader and Custom-UDFs

  The **initialize_udf()** function need to defined as follows to create a link between UdfLoader module and respective UDF. This function ensure UdfLoader to call proper constructor and process() function of respective UDF.

    ```C++
    extern "C" {
        void *initialize_udf(config_t *config) {
            RealSenseUdf *udf = new RealSenseUdf(config);
            return (void *)udf;
        }
    }
    ```

  The **"RealSenseUdf"** is the class name of the user defined custom UDF.

### OEI Infrastructure Related Changes

This section describes the check-list one has to ensure to be completed before exercising C++ UDF.

1. All the code specific to UDF should be kept under one directory which should be placed under **.../common/video/udfs/native** directory.

2. Write appropriate ***CMakeLists.txt*** file and link appropriate external library based on the code. Currently two sample CMAKE files are defined for two different use-case.
    - [ Dummy UDF's CMakeLists.txt file] (./native/dummy/CMakeLists.txt)
    - [Safety Gear Demo CMakeLists.txt file](./native/safety_gear_demo/CMakeLists.txt)

3. If the OpenVINO is used kindly follow the [safety_gear_demo](common/video/udfs/native/safety_gear_demo) example for proper linking of  **cpu_extension.so** shared object.

4. An appropriate entry must exist for each UDF that need to be loaded as part of OEI deployment. The UDF entry syntax is explained in detail in the following document [UDF README](./README.md)

## Steps for writing Python UDFs

This section describes the process of writing a Python UDF. As discussed in the aforementioned scenario, it also has two aspects to it.

- Writing the actual UDF. It needs knowledge OEI exposed python APIs

- Adding the UDF to OEI framework by altering different configs.

### Python APIs for writing UDF for OEI

- Initialization

   In case of Python the initialization callback is also same as the native case. User must create a ***Udf class*** and the ***\_\_init\_\_()*** function defined in class act as a initialization routine for custom UDF. A dummy constructor code is pasted below:

    ```Python
    class Udf:
    """Example UDF
    """
    def __init__(self):
        """Constructor
        """
        # Add the initialization code in this method.
    ```

- Process Actual Data

  The API used to process the actual frame looks as below.

    ```Python
    process(self, frame, metadata):
        # Process the frame in this method
        # metadata can be used to return inference result
    ```

  **Argument:**

  *frame*: Image frame in numpy's ndarray format

  *metadata*: An empty dictionary. Inference results can be inserted in this data structure.

  **Return value:**

  This function returns three values.

  *1st Value* : Represents if the frame Need to be dropped or Not. It is boolean in nature. In case of failure user can return *True* in this positional return value.

  *2nd Value* : It represents the actual modified frame if at all it has been modified. Hence the type is **numpy's ndarray**. If the frame is not modified user can return a *None* in this place.

  *3rd Value* : Metadata is returned in this place. Hence the type is **dict**. In general user can return the passed argument as part of this function.

For reference user can find example UDFs code in below mentioned links

- [PCB_FILTER](./python/pcb/pcb_filter.py)

- [PCB_CLASSIFIER](./python/pcb/pcb_classifier.py)

- [Dummy UDF](./python/dummy.py)

### OEI Infrastructure changes

For any UDF to be utilized by the OEI infrastructure, user must follow the below steps.

- Corresponding UDF entry must be added to the `config.json` file of apps like VideoIngestion and VideoAnalytics.
  The UDF entry syntax is explained in detail in the following document [UDF README](./README.md)

- All the python UDFs must be kept under [python udf directory](./python). Additionally the entry *name* key must have the file hierarchy till the file name as the name of the udf. For example:
A file present in this path *./python/pcb/pcb_filter.py* must have the *name* field as *pcb.pcb_filter*.

    >This syntax is described in detail in the [UDF README](./README.md).

### Conclusion

The UDFs currently supported using python and C++. UDF uses in-memory message passing instead of sockets for the communication between the pipeline and UDFs making it faster in comparison. OEI has many sample UDFs can be found in following paths [for C++](./native) & [for python](./python)
