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
#ifndef __HISTO1D_WIDGET_H__
#define __HISTO1D_WIDGET_H__

#include "analysis/condition_editor_interface.h"
#include "histo1d.h"
#include "libmvme_export.h"

#include <QSpinBox>
#include <QWidget>

#include <qwt_plot_picker.h>

class QwtPlotPicker;
class MVMEContext;

namespace analysis
{
    class CalibrationMinMax;
    class Histo1DSink;
}

struct Histo1DWidgetPrivate;

class LIBMVME_EXPORT Histo1DWidget: public QWidget, public analysis::ConditionEditorInterface
{
    Q_OBJECT
    Q_INTERFACES(analysis::ConditionEditorInterface);

    signals:
        void histogramSelected(int histoIndex);

    public:
        using SinkPtr = std::shared_ptr<analysis::Histo1DSink>;
        using HistoSinkCallback = std::function<void (const SinkPtr &)>;
        using HistoList = QVector<std::shared_ptr<Histo1D>>;

        // single histo
        Histo1DWidget(const Histo1DPtr &histo, QWidget *parent = nullptr);

        // list of histos
        Histo1DWidget(const HistoList &histos, QWidget *parent = nullptr);

        // analysis histo sink
        Histo1DWidget(const SinkPtr &histoSink, QWidget *parent = nullptr);
        virtual ~Histo1DWidget();

        HistoList getHistograms() const;

        void setHistogram(const Histo1DPtr &histo);

        virtual bool eventFilter(QObject *watched, QEvent *e) override;
        virtual bool event(QEvent *event) override;

        friend class Histo1DListWidget;

        void setContext(MVMEContext *context);
        MVMEContext *getContext() const;
        void setCalibration(const std::shared_ptr<analysis::CalibrationMinMax> &calib);
        void setSink(const SinkPtr &sink, HistoSinkCallback sinkModifiedCallback);
        SinkPtr getSink() const;
        void selectHistogram(int histoIndex);

        void setResolutionReductionFactor(u32 rrf);
        void setResolutionReductionSliderEnabled(bool b);

        QwtPlot *getPlot() const;

        // ConditionEditorInterface
        virtual bool setEditCondition(const analysis::ConditionLink &cl) override;
        virtual analysis::ConditionLink getEditCondition() const override;
        virtual void beginEditCondition() override;

        void activatePlotPicker(QwtPlotPicker *picker);
        QwtPlotPicker *getActivePlotPicker() const;

    public slots:
        void replot();

    private slots:
        /* IMPORTANT: leave slots invoked by qwt here for now. do not use lambdas!
         * Reason: there is/was a bug where qwt signals could only be succesfully
         * connected using the old SIGNAL/SLOT macros. Newer function pointer based
         * connections did not work. */
        // TODO 10/2018: recheck this. It might have just been an issue with
        // missing casts of overloaded signals.
        void zoomerZoomed(const QRectF &);
        void mouseCursorMovedToPlotCoord(QPointF);
        void mouseCursorLeftPlot();

        void on_tb_subRange_clicked();
        void on_tb_rate_toggled(bool checked);
        void on_tb_gauss_toggled(bool checked);
        void on_tb_test_clicked();
        void on_ratePointerPicker_selected(const QPointF &);
        void onHistoSpinBoxValueChanged(int index);

    private:
        std::unique_ptr<Histo1DWidgetPrivate> m_d;
        friend struct Histo1DWidgetPrivate;
};

#endif /* __HISTO1D_WIDGET_H__ */
