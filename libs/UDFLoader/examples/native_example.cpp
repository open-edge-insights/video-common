#include <eis/udf/base_udf.h>
#include <eis/utils/logger.h>


namespace eis {
namespace udf {

/**
 * The do nothing UDF
 */
class NativeExampleUdf : public BaseUdf {
public:
    NativeExampleUdf() : BaseUdf() {};

    ~NativeExampleUdf() {};

    bool initialize(config_t* config) override {
        bool ret = this->BaseUdf::initialize(config);
        if(!ret)
            return false;

        // TODO: Get some parameters out of the conig

        return true;
    };

    UdfRetCode process(cv::Mat* frame, msg_envelope_t* meta) override {
//        cv::Mat temp;
//		cv::resize(frame, temp, cv::Size(5,5));
		return UdfRetCode::UDF_OK;
    };
};

} // udf
} // eis

extern "C" {

/**
 * Create the UDF.
 *
 * @return void*
 */
void* initialize_udf() {
    eis::udf::NativeExampleUdf* udf = new eis::udf::NativeExampleUdf();
	return (void*) udf;
}

} // extern "C"

