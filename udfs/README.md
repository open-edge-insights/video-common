# EIS Sample UDFs

EIS supports loading and executing of native(c++) and python UDFs. In here,
one can find the sample native and python UDFs(User Defined Functions) to be used with EIS components
like VideoIngestion and VideoAnalytics. The UDFs can modify the frame, drop the frame and generate meta-data from the frame.

## UDF Configuration

Below is the JSON schema for UDF json object configuration:

```javascript
{
  "type": "object",
  "additionalProperties": false,
  "properties": {
    "max_jobs": {
      "description": "Number of queued UDF jobs",
      "type": "integer",
      "default": 20
    },
    "max_workers": {
      "description": "Number of threads acting on queued jobs",
      "type": "integer",
      "default": 4
    },
    "udfs": {
      "description": "Array of UDF config objects",
      "type": "array",
      "items": [
        {
          "description": "UDF config object",
          "type": "object",
          "properties": {
            "type": {
              "description": "UDF type",
              "type": "string",
              "enum": [
                "native",
                "python"
              ]
            },
            "name": {
              "description": "Unique UDF name",
              "type": "string"
            },
            "device": {
              "description": "Device on which inference occurs",
              "type": "string",
              "enum": [
                "CPU",
                "GPU",
                "HDDL",
                "MYRIAD"
              ]
            }
          },
          "additionalProperties": true,
          "required": [
            "type",
            "name"
          ]
        }
      ]
    }
  }
}
```

One can use [JSON validator tool](https://www.jsonschemavalidator.net/) for
validating the UDF configuration object against the above schema.

Example UDF configuration:

```javascript
{
  "max_jobs": 20,
  "max_workers": 4,
  "udfs": [ {
      "type": "native",
      "name": "dummy"
    },
    {
      "type": "python",
      "name": "pcb.filter"
    }]
}
```

## UDF Writing Guide

User can refer to [UDF Writing HOW-TO GUIDE](./HOWTO_GUIDE_FOR_WRITING_UDF.md) for an detailed explanation of process to write an custom UDF.

## Sample UDFs

> **NOTE**: The UDF config of these go as json objects in the `udfs` key in
> the overall UDF configuration object

### Native UDFs

* **Dummy UDF**

  Accepts the frame and forwards the same without doing any processing. It's a
  do-nothing UDF.

  `UDF config`:

  ```javascript
  {
      "name": "dummy",
      "type": "native"
  }
  ```

* **Resize UDF**

  Accepts the frame, resizes it based on the `width` and `height` params.

  `UDF config`:

  ```javascript
  {
      "name": "resize",
      "type": "native",
      "width": 600,
      "height": 600
  }
  ```

* **Safety Gear Demo UDF**

  Acceps the frame, detects safety gear such as safety helmet, safety jacket in
  the frame and any violations occuring.

  > **NOTE**: This works well with only the
  > [safe gear video file](../../VideoIngestion/test_videos/Safety_Full_Hat_and_Vest.mp4).
  > For camera usecase, proper tuning needs to be done to have the proper model
  > built and used for inference.

   `UDF config`:

  ```javascript
  {
      "name": "safety_gear_demo",
      "type": "native",
      "device": "CPU",
      "model_xml": "common/udfs/native/safety_gear_demo/ref/frozen_inference_graph.xml",
      "model_bin": "common/udfs/native/safety_gear_demo/ref/frozen_inference_graph.bin"
  }
  ```

  ----
  **NOTE**:
  The above config works for both "CPU" and "GPU" devices after setting
  appropriate `device` value. If the device in the above config is "HDDL" or
  "MYRIAD", please use the below config where the model_xml and model_bin
  files are different. Please set the "device" value appropriately based on
  the device used for inferencing.

  ```javascript
  {
      "name": "safety_gear_demo",
      "type": "native",
      "device": "HDDL",
      "model_xml": "common/udfs/native/safety_gear_demo/ref/frozen_inference_graph_fp16.xml",
      "model_bin": "common/udfs/native/safety_gear_demo/ref/frozen_inference_graph_fp16.bin"
  }
  ```

  ----


### Python UDFs

> **NOTE**: Additional properties/keys other than `name` and `type` in the UDF
> config are the parameters of the python UDF constructor

* **Dummy UDF**

  Accepts the frame and forwards the same without doing any processing. It's a
  do-nothing UDF.

  `UDF config`:

  ```javascript
  {
      "name": "dummy",
      "type": "python"
  }
  ```

* **PCB Filter UDF**

  Accepts the frame and based on if `pcb` board is at the center in the frame or not,
  it forwards or drops the frame. It basically sends out only the key frames forward
  for further processing and not all frames it receives.

  > **NOTE**: This works well with only the
  > [pcb demo video file](../../VideoIngestion/test_videos/pcb_d2000.avi).
  > For camera usecase, proper tuning needs to be done to have the proper model
  > built and used for inference.

  `UDF config`:

  ```javascript
  {
      "name": "pcb.pcb_filter",
      "type": "python",
      "training_mode": "false",
      "n_total_px": 300000,
      "n_left_px": 1000,
      "n_right_px": 1000
  }
  ```

* **PCB Classifier UDF**

  Accepts the frame, uses openvino inference engine APIs to determine whether it's
  a `good` pcb with no defects or `bad` pcb with defects. Metadata associated with
  the frame is populated accordingly.

  > **NOTE**: This works well with only the
  > [pcb demo video file](../../VideoIngestion/test_videos/pcb_d2000.avi).
  > For camera usecase, proper tuning needs to be done to have the proper model
  > built and used for inference.

  `UDF config`:

  ```javascript
  {
      "name": "pcb.pcb_classifier",
      "type": "python",
      "device": "CPU",
      "ref_img": "common/udfs/python/pcb/ref/ref.png",
      "ref_config_roi": "common/udfs/python/pcb/ref/roi_2.json",
      "model_xml": "common/udfs/python/pcb/ref/model_2.xml",
      "model_bin": "common/udfs/python/pcb/ref/model_2.bin"
  }
  ```

* **Safety Gear Demo UDF**

  Acceps the frame, detects safety gear such as safety helmet, safety jacket in
  the frame and any violations occuring.

  > **NOTE**: This works well with only the
  > [safe gear video file](../../VideoIngestion/test_videos/Safety_Full_Hat_and_Vest.mp4).
  > For camera usecase, proper tuning needs to be done to have the proper model
  > built and used for inference.

   `UDF config`:

  ```javascript
  {
      "name": "safety_gear.safety_classifier",
      "type": "python",
      "device": "CPU",
      "model_xml": "common/udfs/python/safety_gear/ref/frozen_inference_graph.xml",
      "model_bin": "common/udfs/python/safety_gear/ref/frozen_inference_graph.bin"
  }
  ```

  ----
  **NOTE**:
  The above config works for both "CPU" and "GPU" devices after setting
  appropriate `device` value. If the device in the above config is "HDDL" or
  "MYRIAD", please use the below config where the model_xml and model_bin
  files are different. Please set the "device" value appropriately based on
  the device used for inferencing.

  ```javascript
  {
      "name": "safety_gear.safety_classifier",
      "type": "python",
      "device": "HDDL",
      "model_xml": "common/udfs/python/safety_gear/ref/frozen_inference_graph_fp16.xml",
      "model_bin": "common/udfs/python/safety_gear/ref/frozen_inference_graph_fp16.bin"
  }
  ```

  ----

## Chaining of UDFs

One can chain multiple native/python UDFs in the `udfs` key. The way chaining
works here is the output of the UDF listed first would send the modified frame
and metadata to the subsequent UDF and so on. One such classic example is having
`pcb.pcb_filter` and `pcb.pcb_classifier` in VideoIngestion service config to
do both the pre-processing and the classification logic without the need of
VideoAnalytics service.
