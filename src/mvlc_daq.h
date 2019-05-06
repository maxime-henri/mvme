#ifndef __MVME_MVLC_DAQ_H__
#define __MVME_MVLC_DAQ_H__

#include "mvlc/mvlc_qt_object.h"
#include "vme_config.h"

namespace mesytec
{
namespace mvlc
{

using Logger = std::function<void (const QString &)>;

// Returns std::error_codes generated by the MVLC code in case of errors.
// Exceptions thrown by internal function calls are not caught but passed on to
// the caller (e.g. vme_script::ParseError).
std::error_code setup_mvlc(MVLCObject &mvlc, const VMEConfig &vmeConfig, Logger logger);

} // end namespace mvlc
} // end namespace mesytec

#endif /* __MVME_MVLC_DAQ_H__ */