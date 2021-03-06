#ifndef __MVME_STREAM_PROCESSOR_MODULE_CONSUMER_H__
#define __MVME_STREAM_PROCESSOR_MODULE_CONSUMER_H__

#include <QString>
#include <functional>
#include "libmvme_export.h"
#include "typedefs.h"

class VMEConfig;
namespace analysis { class Analysis; }
struct DAQStats;
struct RunInfo;

/* Interface for consumers of raw module data. */
class LIBMVME_EXPORT IMVMEStreamModuleConsumer
{
    public:
        using Logger = std::function<void (const QString &)>;

        virtual ~IMVMEStreamModuleConsumer() {};

        virtual void startup() {}
        virtual void shutdown() {}

        virtual void beginRun(const RunInfo &runInfo,
                              const VMEConfig *vmeConfig,
                              const analysis::Analysis *analysis) = 0;

        virtual void endRun(const DAQStats &stats, const std::exception *e = nullptr) = 0;

        virtual void beginEvent(s32 eventIndex) = 0;
        virtual void endEvent(s32 eventIndex) = 0;
        virtual void processModulePrefix(s32 eventIndex,
                                       s32 moduleIndex,
                                       const u32 *data, u32 size) = 0;
        virtual void processModuleData(s32 eventIndex,
                                       s32 moduleIndex,
                                       const u32 *data, u32 size) = 0;
        virtual void processModuleSuffix(s32 eventIndex,
                                       s32 moduleIndex,
                                       const u32 *data, u32 size) = 0;
        virtual void processTimetick() = 0;
        virtual void setLogger(Logger logger) = 0;
};

#endif /* __MVME_STREAM_PROCESSOR_MODULE_CONSUMER_H__ */
