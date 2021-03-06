/* mvme - Mesytec VME Data Acquisition
 *
 * Copyright (C) 2016-2020 mesytec GmbH & Co. KG <info@mesytec.com>
 *
 * Author: Florian Lüke <f.lueke@mesytec.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#ifndef __MVME_EVENT_SERVER_H__
#define __MVME_EVENT_SERVER_H__

#include "libmvme_export.h"
#include "mvme_stream_processor.h"
#include <QHostAddress>

class LIBMVME_EXPORT EventServer: public QObject, public IMVMEStreamModuleConsumer
{
    Q_OBJECT
    signals:
        void clientConnected();
        void clientDisconnected();

    public:
        static const uint16_t Default_ListenPort = 13801;

        explicit EventServer(QObject *parent = nullptr);
        virtual ~EventServer();

        virtual void startup() override;
        virtual void shutdown() override;

        virtual void beginRun(const RunInfo &runInfo,
                              const VMEConfig *vmeConfig,
                              const analysis::Analysis *analysis) override;

        virtual void endRun(const DAQStats &stats, const std::exception *e = nullptr) override;

        virtual void beginEvent(s32 eventIndex) override;
        virtual void endEvent(s32 eventIndex) override;
        virtual void processModulePrefix(s32 eventIndex, s32 moduleIndex,
                                       const u32 *data, u32 size) override;
        virtual void processModuleData(s32 eventIndex, s32 moduleIndex,
                                       const u32 *data, u32 size) override;
        virtual void processModuleSuffix(s32 eventIndex, s32 moduleIndex,
                                       const u32 *data, u32 size) override;
        virtual void processTimetick() override;
        virtual void setLogger(Logger logger) override;

        // Server specific settings and info
        void setListeningInfo(const QHostAddress &address,
                              quint16 port = Default_ListenPort);

        bool isListening() const;
        size_t getNumberOfClients() const;

    public slots:
        void setEnabled(bool b);

    private:
        struct Private;
        std::unique_ptr<Private> m_d;
};

#endif /* __MVME_EVENT_SERVER_H__ */
